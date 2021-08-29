cmake_minimum_required(VERSION 3.10)

project(FoxContentClient)
add_executable(FoxContentClient main.cpp)
target_link_libraries(FoxContentClient PUBLIC FoxNet)