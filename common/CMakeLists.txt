cmake_minimum_required(VERSION 3.5)

project(common)

file(GLOB SRC 
    ./src/*.cpp)

add_library(common STATIC ${SRC})
add_library(common::common ALIAS common)

target_link_libraries(common 
    PUBLIC
    glew
    glfw
    glm
    )
    
target_include_directories(common 
    PUBLIC  ./include
    PRIVATE ./include/common)

