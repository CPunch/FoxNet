cmake_minimum_required(VERSION 3.10)

project(FoxServer)
add_executable(FoxServer main.cpp)
target_link_libraries(FoxServer PUBLIC FoxNet)