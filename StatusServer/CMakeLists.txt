project(StatusServer)

file(GLOB_RECURSE STATUS_SRC "src/*.cpp")

add_executable(StatusServer ${STATUS_SRC})

# 包含自己特有的 include
target_include_directories(StatusServer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(StatusServer PRIVATE common ${GLOBAL_LINK_LIBS})

add_custom_command(TARGET StatusServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different 
        "${CMAKE_SOURCE_DIR}/config.ini"
        $<TARGET_FILE_DIR:StatusServer>
)