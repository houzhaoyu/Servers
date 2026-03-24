project(ChatServer)

file(GLOB_RECURSE CHAT_SRC "src/*.cpp")

add_executable(ChatServer ${CHAT_SRC})

# 包含自己特有的 include
target_include_directories(ChatServer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(ChatServer PRIVATE common ${GLOBAL_LINK_LIBS})

add_custom_command(TARGET ChatServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/config.ini" $<TARGET_FILE_DIR:ChatServer>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MYSQL_ROOT}/lib64/debug/mysqlcppconn-9-vs14.dll" $<TARGET_FILE_DIR:ChatServer>
)