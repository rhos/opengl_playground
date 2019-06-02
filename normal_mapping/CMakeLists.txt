cmake_minimum_required(VERSION 3.5)

project(normal_mapping)

add_executable(normal_mapping main.cpp)

target_link_libraries(normal_mapping 
    PRIVATE
    common
    )

add_custom_command(TARGET normal_mapping POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_PROPERTY:glew,dll>
        $<TARGET_FILE_DIR:normal_mapping>)