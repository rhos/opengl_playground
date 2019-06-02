cmake_minimum_required(VERSION 3.5)

project(fluid)

add_executable(fluid main.cpp)

target_link_libraries(fluid 
    PRIVATE
    common
    )

add_custom_command(TARGET fluid POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_PROPERTY:glew,dll>
        $<TARGET_FILE_DIR:fluid>)