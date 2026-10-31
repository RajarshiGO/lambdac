cmake_minimum_required(VERSION 3.20)
project(lambdac CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra)

add_library(lambdac_core src/AST.cpp)

target_include_directories(lambdac_core PUBLIC include)

add_executable(ast_smoke tests/ast_smoke.cpp)
target_link_libraries(ast_smoke PRIVATE lambdac_core)
