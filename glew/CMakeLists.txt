cmake_minimum_required(VERSION 3.5)

project(glew)

add_library(glew INTERFACE)

get_filename_component(lib lib/Release/x64/glew32.lib ABSOLUTE)
get_filename_component(dll bin/Release/x64/glew32.dll ABSOLUTE)
target_link_libraries(glew INTERFACE ${lib})
set_target_properties(glew PROPERTIES dll ${dll})
target_include_directories(glew INTERFACE include)