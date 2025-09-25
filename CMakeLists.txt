cmake_minimum_required(VERSION 3.20)

project(
    risk-assessment-engine
    DESCRIPTION "Engine to generate portfolio risk metrics"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(lib/spdlog)

add_subdirectory(src)