cmake_minimum_required(VERSION 2.8)

project(copy_on_write)

set(CMAKE_CXX_FLAGS "-std=c++14")

include_directories(externals/catch/include)
add_executable(test_copy_on_write test_copy_on_write.cpp)

enable_testing()
add_test(
  NAME test_copy_on_write
  COMMAND test_copy_on_write
  WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

