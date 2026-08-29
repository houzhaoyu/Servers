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
    set(PROTOC_EXE "${GRPC_ROOT}/visualpro/third_party/protobuf/Debug/protoc.exe")
    set(GRPC_CPP_PLUGIN "${GRPC_ROOT}/visualpro/Debug/grpc_cpp_plugin.exe")
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
    # ---------------- Windows：使用手动指定的库路径 ----------------
    if(NOT BOOST_ROOT)
        message(FATAL_ERROR "Windows 下请通过 -DBOOST_ROOT=... 指定 Boost 路径")
    endif()
    if(NOT GRPC_ROOT)
        message(FATAL_ERROR "Windows 下请通过 -DGRPC_ROOT=... 指定 gRPC 路径")
    endif()
    include_directories(
        ${BOOST_ROOT}/include/boost-1_88
        ${HIREDIS_ROOT}/deps/hiredis
        ${MYSQL_ROOT}/include
        ${GRPC_ROOT}/include
        ${GRPC_ROOT}/third_party/re2
        ${GRPC_ROOT}/third_party/address_sorting/include
        ${GRPC_ROOT}/third_party/abseil-cpp
        ${GRPC_ROOT}/third_party/protobuf/src
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
    link_directories(
        ${BOOST_ROOT}/lib
        ${HIREDIS_ROOT}/lib
        ${MYSQL_ROOT}/lib64/vs14/debug

        ${GRPC_ROOT}/visualpro/third_party/re2/Debug
        ${GRPC_ROOT}/visualpro/third_party/abseil-cpp/absl/types/Debug
        ${GRPC_ROOT}/visualpro/third_party/abseil-cpp/absl/synchronization/Debug
        ${GRPC_ROOT}/visualpro/third_party/abseil-cpp/absl/status/Debug
        ${GRPC_ROOT}/visualpro/third_party/abseil-cpp/absl/random/Debug
        ${GRPC_ROOT}/visualpro/third_party/abseil-cpp/absl/flags/Debug
        ${GRPC_ROOT}/visualpro/third_party/abseil-cpp/absl/debugging/Debug
        ${GRPC_ROOT}/visualpro/third_party/abseil-cpp/absl/container/Debug
        ${GRPC_ROOT}/visualpro/third_party/abseil-cpp/absl/hash/Debug
        ${GRPC_ROOT}/visualpro/third_party/boringssl-with-bazel/Debug
        ${GRPC_ROOT}/visualpro/third_party/abseil-cpp/absl/numeric/Debug
        ${GRPC_ROOT}/visualpro/third_party/abseil-cpp/absl/time/Debug
        ${GRPC_ROOT}/visualpro/third_party/abseil-cpp/absl/base/Debug
        ${GRPC_ROOT}/visualpro/third_party/abseil-cpp/absl/strings/Debug
        ${GRPC_ROOT}/visualpro/third_party/protobuf/Debug
        ${GRPC_ROOT}/visualpro/third_party/zlib/Debug
        ${GRPC_ROOT}/visualpro/Debug
        ${GRPC_ROOT}/visualpro/third_party/cares/cares/lib/Debug
    )

    set(GLOBAL_LINK_LIBS
        # protobuf & grpc
        libprotobufd.lib
        gpr.lib
        grpc.lib
        grpc++.lib
        grpc++_reflection.lib
        address_sorting.lib
        cares.lib
        zlibstaticd.lib
        upb.lib
        ssl.lib
        crypto.lib
        re2.lib
        Win32_Interop.lib

    # abseil
    absl_bad_any_cast_impl.lib
    absl_bad_optional_access.lib
    absl_bad_variant_access.lib
    absl_base.lib
    absl_city.lib
    absl_civil_time.lib
    absl_cord.lib
    absl_debugging_internal.lib
    absl_demangle_internal.lib
    absl_examine_stack.lib
    absl_exponential_biased.lib
    absl_failure_signal_handler.lib
    absl_flags.lib
    absl_flags_config.lib
    absl_flags_internal.lib
    absl_flags_marshalling.lib
    absl_flags_parse.lib
    absl_flags_program_name.lib
    absl_flags_usage.lib
    absl_flags_usage_internal.lib
    absl_graphcycles_internal.lib
    absl_hash.lib
    absl_hashtablez_sampler.lib
    absl_int128.lib
    absl_leak_check.lib
    absl_leak_check_disable.lib
    absl_log_severity.lib
    absl_malloc_internal.lib
    absl_periodic_sampler.lib
    absl_random_distributions.lib
    absl_random_internal_distribution_test_util.lib
    absl_random_internal_pool_urbg.lib
    absl_random_internal_randen.lib
    absl_random_internal_randen_hwaes.lib
    absl_random_internal_randen_hwaes_impl.lib
    absl_random_internal_randen_slow.lib
    absl_random_internal_seed_material.lib
    absl_random_seed_gen_exception.lib
    absl_random_seed_sequences.lib
    absl_raw_hash_set.lib
    absl_raw_logging_internal.lib
    absl_scoped_set_env.lib
    absl_spinlock_wait.lib
    absl_stacktrace.lib
    absl_status.lib
    absl_strings.lib
    absl_strings_internal.lib
    absl_str_format_internal.lib
    absl_symbolize.lib
    absl_synchronization.lib
    absl_throw_delegate.lib
    absl_time.lib
    absl_time_zone.lib
    absl_statusor.lib

        # hiredis
        hiredis.lib

        # mysql
        mysqlcppconn.lib
        mysqlcppconn8.lib

        # Win32
        ws2_32.lib
    )
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