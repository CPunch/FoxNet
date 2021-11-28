cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# define foxnet cmake variables
set(FOXNET_INCLUDEDIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# version details
set(FOXNET_VERSION_MAJOR 0)
set(FOXNET_VERSION_MINOR 2)

project(FoxNet VERSION ${FOXNET_VERSION_MAJOR}.${FOXNET_VERSION_MINOR})

#set(CMAKE_VERBOSE_MAKEFILE on)

# Output binaries to the bin folder in the source directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Put CMake targets (ALL_BUILD/ZERO_CHECK) into a folder
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set the project as the default startup project for VS
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT FoxNet)

# compile FoxNet library
file(GLOB_RECURSE FOXSOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/**.cpp)
add_library(FoxNet STATIC ${FOXSOURCE})

# add the version definitions and the 'DEBUG' preprocessor definition if we're compiling as Debug
target_compile_definitions(FoxNet PUBLIC _FOXNET_VER_MAJOR=${FOXNET_VERSION_MAJOR} _FOXNET_VER_MINOR=${FOXNET_VERSION_MINOR} "$<$<CONFIG:Debug>:DEBUG>")

# add include directory
target_include_directories(FoxNet PUBLIC ${FOXNET_INCLUDEDIR})

set_target_properties(FoxNet PROPERTIES OUTPUT_NAME foxnet-${FOXNET_VERSION_MAJOR}.${FOXNET_VERSION_MINOR})

# now compile the examples
add_subdirectory(examples)
