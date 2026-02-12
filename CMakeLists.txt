cmake_minimum_required(VERSION 3.20)

project(resetprop_alone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# resetprop uses Android system property API; meaningful on Android (NDK) only.
# Host build produces a stub that prints usage.
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  add_executable(resetprop src/resetprop_main.cpp)
  target_compile_definitions(resetprop PRIVATE RESETPROP_ANDROID=1)
  target_include_directories(resetprop PRIVATE src)
  # bionic provides __system_property_* in libc
  target_link_libraries(resetprop PRIVATE log)
else()
  add_executable(resetprop src/resetprop_main.cpp)
  target_compile_definitions(resetprop PRIVATE RESETPROP_STANDALONE=1)
  target_include_directories(resetprop PRIVATE src)
endif()
