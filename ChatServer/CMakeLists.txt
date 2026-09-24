project(ChatServer)

file(GLOB_RECURSE CHAT_SRC "src/*.cpp")

add_executable(ChatServer ${CHAT_SRC})

# 包含自己特有的 include
target_include_directories(ChatServer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(ChatServer PRIVATE common ${GLOBAL_LINK_LIBS})

add_custom_command(TARGET ChatServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/config.ini" $<TARGET_FILE_DIR:ChatServer>
)

# Windows 下复制 MySQL 连接器 DLL 到可执行文件目录（Linux 无需）
if(WIN32)
    copy_mysql_runtime(ChatServer)
endif()
