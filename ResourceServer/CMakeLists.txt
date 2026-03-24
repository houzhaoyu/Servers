project(ResourceServer)

file(GLOB_RECURSE RES_SRC "src/*.cpp")

add_executable(ResourceServer ${RES_SRC})

# 包含自己特有的 include
target_include_directories(ResourceServer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(ResourceServer PRIVATE common ${GLOBAL_LINK_LIBS})

add_custom_command(TARGET ResourceServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different 
        "${CMAKE_SOURCE_DIR}/config.ini"
        $<TARGET_FILE_DIR:ResourceServer>
)