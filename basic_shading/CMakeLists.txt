cmake_minimum_required(VERSION 3.5)

project(basic_shading)

add_executable(basic_shading main.cpp)

target_link_libraries(basic_shading 
    PRIVATE
    common
    )

add_custom_command(TARGET basic_shading POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_PROPERTY:glew,dll>
        $<TARGET_FILE_DIR:basic_shading>)