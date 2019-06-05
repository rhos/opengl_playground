cmake_minimum_required(VERSION 3.5)

project(fluid2)

add_executable(fluid2 main.cpp)

target_link_libraries(fluid2 
    PRIVATE
    common
    )

add_custom_command(TARGET fluid2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_PROPERTY:glew,dll>
        $<TARGET_FILE_DIR:fluid2>)