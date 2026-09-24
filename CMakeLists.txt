cmake_minimum_required(VERSION 3.16)
project(ChatApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 针对 MSVC 编译器强制开启 /utf-8 选项
if(MSVC)
    add_compile_options("/utf-8")
endif()

# ============================================================
# 1. 第三方库根路径（缓存变量，可用 -D 在命令行覆盖）
#    示例：cmake -DBOOST_ROOT=... -DGRPC_ROOT=... -DMYSQL_ROOT=... ..
#    - 留空时使用系统默认安装路径（Linux 下 apt 安装的库）
#    - Windows 下 BOOST_ROOT / GRPC_ROOT / MYSQL_ROOT 一般必须指定
# ============================================================
set(BOOST_ROOT      "" CACHE PATH "Boost 根目录")
set(HIREDIS_ROOT    "" CACHE PATH "hiredis 根目录")
set(MYSQL_ROOT      "" CACHE PATH "MySQL Connector/C++ 根目录")
set(GRPC_ROOT       "" CACHE PATH "gRPC 根目录")
set(THIRDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty" CACHE PATH "内置第三方库(jsoncpp/spdlog)目录")

# ============================================================
# 2. protoc / grpc_cpp_plugin（生成 .pb 代码）
#    Windows：使用 GRPC_ROOT 内编译好的 .exe
#    Linux  ：从 PATH 中查找系统安装的 protoc / grpc_cpp_plugin
# ============================================================
if(WIN32)
    # Windows：依赖由 vcpkg 提供，用 CMake 目标解析库与工具路径
    find_package(Boost CONFIG REQUIRED COMPONENTS filesystem)
    find_package(hiredis CONFIG REQUIRED)
    find_package(protobuf CONFIG REQUIRED)
    find_package(gRPC CONFIG REQUIRED)
    find_package(mysql-concpp CONFIG REQUIRED COMPONENTS debug)

    # 工具可执行文件：生成器表达式在生成阶段解析为实际路径
    set(PROTOC_EXE "$<TARGET_FILE:protobuf::protoc>")
    set(GRPC_CPP_PLUGIN "$<TARGET_FILE:gRPC::grpc_cpp_plugin>")
else()
    find_program(PROTOC_EXE protoc REQUIRED)
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)
endif()

# ============================================================
# 3. 统一头文件搜索路径
# ============================================================
include_directories(
    ${THIRDPARTY_DIR}
    ${THIRDPARTY_DIR}/jsoncpp
)

if(WIN32)
    # ---------------- Windows：Boost/gRPC/hiredis 由 vcpkg 管理，MySQL 沿用官方二进制 ----------------
    if(NOT MYSQL_ROOT)
        message(FATAL_ERROR "Windows 下请通过 -DMYSQL_ROOT=... 指定 MySQL Connector/C++ 路径")
    endif()
    # vcpkg 已安装头文件根目录（由 vcpkg toolchain 注入的变量拼接）
    set(VCPKG_INC_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
    include_directories(
        ${VCPKG_INC_DIR}
        ${VCPKG_INC_DIR}/hiredis   # 代码以 <hiredis.h> 引用，需额外包含 hiredis 子目录
        ${MYSQL_ROOT}/include
    )
else()
    # ---------------- Linux / Unix：系统默认路径（可叠加自定义路径） ----------------
    if(BOOST_ROOT)
        include_directories(${BOOST_ROOT}/include)
    endif()
    if(GRPC_ROOT)
        include_directories(${GRPC_ROOT}/include)
    endif()

    # ---- hiredis：代码按 <hiredis.h> 引用，apt 安装时头文件位于 /usr/include/hiredis 子目录 ----
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(HIREDIS QUIET hiredis)
    endif()
    if(HIREDIS_FOUND)
        include_directories(${HIREDIS_INCLUDE_DIRS})
    elseif(HIREDIS_ROOT)
        include_directories(${HIREDIS_ROOT}/include)
    elseif(EXISTS "/usr/include/hiredis/hiredis.h")
        include_directories("/usr/include/hiredis")
    else()
        message(FATAL_ERROR "未找到 hiredis 头文件，请安装 libhiredis-dev 或指定 -DHIREDIS_ROOT=...")
    endif()

    # ---- MySQL：代码统一使用 <jdbc/...> 前缀 ----
    #   8.x 官方包 / Windows：include 下自带 jdbc/ 子目录，直接添加即可
    #   apt 1.1.x：mysql_driver.h 在 /usr/include 根、cppconn/ 在 /usr/include/cppconn，
    #              与 jdbc/ 布局不符 → 在构建目录生成 jdbc/ 软链接兼容层
    if(MYSQL_ROOT)
        include_directories(${MYSQL_ROOT}/include)
    elseif(EXISTS "/usr/include/jdbc/mysql_driver.h")
        # 系统已是 jdbc 布局，无需处理
    elseif(EXISTS "/usr/include/mysql_driver.h")
        set(MYSQL_COMPAT_DIR "${CMAKE_BINARY_DIR}/generated/mysql_jdbc_compat")
        file(MAKE_DIRECTORY "${MYSQL_COMPAT_DIR}/jdbc")
        foreach(_hdr mysql_driver.h mysql_connection.h mysql_error.h)
            if(NOT EXISTS "${MYSQL_COMPAT_DIR}/jdbc/${_hdr}")
                execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "/usr/include/${_hdr}" "${MYSQL_COMPAT_DIR}/jdbc/${_hdr}")
            endif()
        endforeach()
        if(NOT EXISTS "${MYSQL_COMPAT_DIR}/jdbc/cppconn")
            execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "/usr/include/cppconn" "${MYSQL_COMPAT_DIR}/jdbc/cppconn")
        endif()
        include_directories("${MYSQL_COMPAT_DIR}")
    else()
        message(FATAL_ERROR "未找到 MySQL Connector/C++ 头文件，请安装 libmysqlcppconn-dev 或指定 -DMYSQL_ROOT=...")
    endif()
endif()

# ============================================================
# 4. 库搜索路径 + 全局链接库列表
# ============================================================
if(WIN32)
    # ---------------- Windows ----------------
    set(GLOBAL_LINK_LIBS
        # 依赖库改用 vcpkg 的 CMake 目标，传递依赖（absl/re2/zlib/openssl 等）自动展开
        Boost::filesystem
        hiredis::hiredis
        protobuf::libprotobuf
        gRPC::grpc++
        gRPC::grpc++_reflection

        # MySQL 传统 JDBC API；自动匹配 Debug/Release 版本
        mysql::concpp-jdbc

        # Win32
        ws2_32.lib
    )

    # 将 Connector 及其动态依赖复制到服务可执行文件目录。
    function(copy_mysql_runtime target_name)
        set(_vcpkg_root "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
        set(_vcpkg_bin "$<IF:$<CONFIG:Debug>,${_vcpkg_root}/debug/bin,${_vcpkg_root}/bin>")
        set(_zlib_dll "$<IF:$<CONFIG:Debug>,${_vcpkg_root}/debug/bin/zd.dll,${_vcpkg_root}/bin/z.dll>")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:mysql::concpp-jdbc>"
                "${_vcpkg_bin}/libmysql.dll"
                "${_vcpkg_bin}/libssl-3-x64.dll"
                "${_vcpkg_bin}/libcrypto-3-x64.dll"
                "${_vcpkg_bin}/zstd.dll"
                "${_zlib_dll}"
                "$<TARGET_FILE_DIR:${target_name}>"
            VERBATIM
        )
    endfunction()
else()
    # ---------------- Linux / Unix ----------------
    # 用 pkg-config 自动解析 gRPC/Protobuf 及其 absl 依赖库
    # （apt 的 libgrpc++-dev 自带 grpc++.pc / grpc.pc / protobuf.pc，
    #   会自动展开 absl_synchronization 等所有依赖库，避免手动补库）
    find_package(PkgConfig REQUIRED)
    execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} --libs grpc++ grpc protobuf
        OUTPUT_VARIABLE GRPC_PKG_LINK
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT GRPC_PKG_LINK)
        message(FATAL_ERROR "pkg-config 未找到 grpc++/grpc/protobuf，请先安装 libgrpc++-dev libprotobuf-dev")
    endif()
    # 将 pkg-config 输出拆分为独立链接参数（如 -lgrpc++、-labsl_synchronization 等）
    separate_arguments(GRPC_PKG_LINK_LIST UNIX_COMMAND "${GRPC_PKG_LINK}")

    # 手动补充非 pkg-config 管理的库
    set(GLOBAL_LINK_LIBS
        ${GRPC_PKG_LINK_LIST}
        hiredis
        mysqlcppconn
        boost_filesystem
        pthread
        dl
        rt
    )
endif()

# ----------------------------------
# 5. 添加各个子模块
# ----------------------------------
add_subdirectory(common)
add_subdirectory(GateServer)
add_subdirectory(ChatServer)
add_subdirectory(StatusServer)
add_subdirectory(ResourceServer)

# 如果你想为 spdlog 设置一些全局宏（可选）
add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG) # 设置日志级别
add_definitions(-DSPDLOG_COMPILED_LIB)
