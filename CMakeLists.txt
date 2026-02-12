cmake_minimum_required(VERSION 3.14)
project(resetpropAlone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(resetprop src/main.cpp)
target_compile_definitions(resetprop PRIVATE RESETPROP_STANDALONE=1)
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  target_link_libraries(resetprop PRIVATE log)
endif()
install(TARGETS resetprop DESTINATION bin)
