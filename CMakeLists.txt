cmake_minimum_required(VERSION 3.5)

project(fluid)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if(MSVC)
    add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
    add_compile_definitions(_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
    add_compile_options(/Zc:__cplusplus)
endif()

add_subdirectory(glew)
add_subdirectory(glfw)
add_subdirectory(glm)
add_subdirectory(common)
add_subdirectory(normal_mapping)
add_subdirectory(render2texture)
add_subdirectory(basic_shading)
add_subdirectory(fluid)