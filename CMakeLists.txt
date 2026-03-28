cmake_minimum_required(VERSION 3.16)
project(ChatApp)

# 针对 MSVC 编译器强制开启 /utf-8 选项
if(MSVC)
    add_compile_options("/utf-8")
endif()

set(CMAKE_CXX_STANDARD 17)       
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------
# 1. 全局第三方库路径定义 (根据你的环境修改)
# ----------------------------------
set(BOOST_ROOT "E:/study/c++/library/boost_1_88_0")
set(HIREDIS_ROOT "E:/study/c++/library/redis/hredis")
set(MYSQL_ROOT "E:/study/c++/library/mysql-connector-c++-8.3.0-winx64")
set(GRPC_ROOT "E:/study/c++/library/grpc")
set(THIRDPARTY_DIR "E:/study/c++/project/FS_ChatApp/Servers/thirdparty")

# ----------------------------------
# 2. 统一头文件搜索路径
# ----------------------------------
include_directories(
    ${BOOST_ROOT}/include/boost-1_88
    ${HIREDIS_ROOT}/deps/hiredis
    ${MYSQL_ROOT}/include
    ${GRPC_ROOT}/include
    ${GRPC_ROOT}/third_party/re2
    ${GRPC_ROOT}/third_party/address_sorting/include
    ${GRPC_ROOT}/third_party/abseil-cpp
    ${GRPC_ROOT}/third_party/protobuf/src
    ${THIRDPARTY_DIR}
    ${THIRDPARTY_DIR}/jsoncpp
)

# ----------------------------------
# 3. 统一库文件搜索路径
# ----------------------------------
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

# ----------------------------------
# 4. 定义全局链接库列表 (提取重复的 lib)
# ----------------------------------
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