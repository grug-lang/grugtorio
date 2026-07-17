cmake_minimum_required(VERSION 3.15)
project(RaylibSquare C)

# Use standard C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find the globally installed raylib package
find_package(raylib 5.0 REQUIRED)

# Create the executable
add_executable(grug-factory main.c)

# Link raylib to our executable
target_link_libraries(grug-factory PRIVATE raylib)
