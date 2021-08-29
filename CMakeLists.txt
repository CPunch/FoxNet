cmake_minimum_required(VERSION 3.10)
project(FoxNet VERSION 0.0)

set(CXX_STANDARD_REQUIRED 17)

# Output binaries to the bin folder in the source directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# define foxnet cmake variables
set(FOXNET_INCLUDEDIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Put CMake targets (ALL_BUILD/ZERO_CHECK) into a folder
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set the project as the default startup project for VS
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT FoxNet)

# compile FoxNet library
file(GLOB_RECURSE FOXSOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/**.cpp)
add_library(FoxNet STATIC ${FOXSOURCE})
target_include_directories(FoxNet PUBLIC ${FOXNET_INCLUDEDIR})

# now compile the examples
add_subdirectory(examples)