cmake_minimum_required(VERSION 3.15)
project(GrugFactory C)

set(CMAKE_C_STANDARD 11)

find_package(raylib 5.0 REQUIRED)

include(FetchContent)
FetchContent_Declare(
    grug_rs
    GIT_REPOSITORY https://github.com/grug-lang/grug-rs.git
    GIT_TAG generics
)
FetchContent_MakeAvailable(grug_rs)

find_program(CARGO_COMMAND cargo REQUIRED)

set(GRUG_RS_SOURCE_DIR ${grug_rs_SOURCE_DIR})

set(GRUG_RS_LIB ${GRUG_RS_SOURCE_DIR}/target/release/libgruggers.a) 

add_custom_target(
    build_grug_rs ALL
    COMMAND ${CARGO_COMMAND} build --release
    WORKING_DIRECTORY ${GRUG_RS_SOURCE_DIR}
    BYPRODUCTS ${GRUG_RS_LIB}
    COMMENT "Compiling grug-rs via Cargo..."
)

add_executable(grug_factory main.c)

add_dependencies(grug_factory build_grug_rs)

target_include_directories(grug_factory PRIVATE ${GRUG_RS_SOURCE_DIR}/include)

target_link_libraries(grug_factory PRIVATE raylib ${GRUG_RS_LIB} dl pthread m)
