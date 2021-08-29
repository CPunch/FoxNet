cmake_minimum_required(VERSION 3.10)

project(FoxArithServer)
add_executable(FoxArithServer main.cpp)
target_link_libraries(FoxArithServer PUBLIC FoxNet)