project(GateServer)

# 自动搜集源码
file(GLOB_RECURSE GATE_SRC "src/*.cpp")

add_executable(GateServer ${GATE_SRC})

# 【核心修改】：使用绝对路径指向当前目录下的 include
target_include_directories(GateServer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# 链接 common 库（common 已经 PUBLIC 导出了它自己的 include 和生成的 proto 头文件）
target_link_libraries(GateServer PRIVATE common ${GLOBAL_LINK_LIBS})

# 后置处理
add_custom_command(TARGET GateServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/config.ini" $<TARGET_FILE_DIR:GateServer>
)

# Windows 下复制 MySQL 连接器 DLL 到可执行文件目录（Linux 无需）
if(WIN32)
    copy_mysql_runtime(GateServer)
endif()
