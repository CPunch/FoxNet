cmake_minimum_required(VERSION 3.10)

project(FoxArithClient)
add_executable(FoxArithClient main.cpp)
target_link_libraries(FoxArithClient PUBLIC FoxNet)