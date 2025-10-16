cmake_minimum_required(VERSION 3.20)

project(
    risk-assessment-engine
    DESCRIPTION "Engine to generate portfolio risk metrics"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    "."
    "../include"
)

file(
    GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    *.cpp
)

add_executable(risk_assessment_engine ${SOURCES})

target_include_directories(
    risk_assessment_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/../lib/CLI11/include
    ${CMAKE_SOURCE_DIR}/../lib/spdlog/include
    ${CMAKE_SOURCE_DIR}/../lib/capi
)

target_link_libraries(
    risk_assessment_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/../lib/capi/l64/c.o
)
