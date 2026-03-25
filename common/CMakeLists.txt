project(common_lib)

# 1. 搜集 common 下的所有源码
file(GLOB_RECURSE COMMON_SRC "src/*.cpp")
list(APPEND COMMON_SRC "${THIRDPARTY_DIR}/jsoncpp/jsoncpp.cpp")

# 2. 路径处理
set(PROTO_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(PROTO_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${PROTO_OUT_DIR})

# 3. 编译器路径（确保这些变量在根目录 CMake 中已定义）
set(PROTOC_EXE "${GRPC_ROOT}/visualpro/third_party/protobuf/Debug/protoc.exe")
set(GRPC_CPP_PLUGIN "${GRPC_ROOT}/visualpro/Debug/grpc_cpp_plugin.exe")

# 4. 生成代码定义
set(PROTO_GEN_FILES 
    "${PROTO_OUT_DIR}/message.pb.cc" 
    "${PROTO_OUT_DIR}/message.pb.h"
    "${PROTO_OUT_DIR}/message.grpc.pb.cc" 
    "${PROTO_OUT_DIR}/message.grpc.pb.h"
)

# --- 改进后的命令 ---
add_custom_command(
    OUTPUT ${PROTO_GEN_FILES}
    # 核心改进：切换到 proto 文件所在目录执行，输入文件直接写文件名
    COMMAND ${PROTOC_EXE}
    ARGS --cpp_out=${PROTO_OUT_DIR}
         --grpc_out=${PROTO_OUT_DIR}
         -I.
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
         message.proto
    DEPENDS "${PROTO_SRC_DIR}/message.proto"
    WORKING_DIRECTORY "${PROTO_SRC_DIR}"  # 在 proto 目录下执行命令
    COMMENT "Generating Protobuf and gRPC code..."
    VERBATIM
)

# 5. 编译静态库
add_library(common STATIC ${COMMON_SRC} ${PROTO_GEN_FILES} "src/BaseSession.cpp" "include/BaseLogic.h" "src/BaseLogic.cpp")

# 6. 导出路径
target_include_directories(common PUBLIC 
    include 
    ${PROTO_OUT_DIR}
)
target_link_libraries(common PUBLIC ${GLOBAL_LINK_LIBS})