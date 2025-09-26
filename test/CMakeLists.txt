cmake_minimum_required(VERSION 3.20)

project(
    risk_tests
    DESCRIPTION "Unit tests for risk-assessment-engine modules"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(RISK_CORE_SOURCES
    ${PROJECT_ROOT}/src/bs.cpp
    ${PROJECT_ROOT}/src/greeks.cpp
    ${PROJECT_ROOT}/src/hvar.cpp
    ${PROJECT_ROOT}/src/instrument_soa.cpp
    ${PROJECT_ROOT}/src/mcvar.cpp
    ${PROJECT_ROOT}/src/utils.cpp
)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(risk_tests
    ${TEST_SOURCES}
    ${RISK_CORE_SOURCES}
)

target_include_directories(risk_tests PRIVATE
    ${PROJECT_ROOT}/include
)

if (NOT TARGET Catch2::Catch2WithMain)
    if (EXISTS "${PROJECT_ROOT}/lib/Catch2/CMakeLists.txt")
        add_subdirectory(${PROJECT_ROOT}/lib/Catch2 ${CMAKE_CURRENT_BINARY_DIR}/Catch2)
    else()
        find_package(Catch2 3 REQUIRED)
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_ROOT}/lib/Catch2/extras")
include(Catch)

target_link_libraries(risk_tests PRIVATE Catch2::Catch2WithMain)

catch_discover_tests(risk_tests)
