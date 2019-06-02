cmake_minimum_required(VERSION 3.5)

project(render2texture)

add_executable(render2texture main.cpp)

target_link_libraries(render2texture 
    PRIVATE
    common
    )

add_custom_command(TARGET render2texture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_PROPERTY:glew,dll>
        $<TARGET_FILE_DIR:render2texture>)