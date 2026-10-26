cmake_minimum_required(VERSION 3.20)

project(
    risk-assessment-engine
    DESCRIPTION "Engine to generate portfolio risk metrics"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_KDB "Build with KDB+ data ingestion support" OFF)

include_directories(
    "."
    "../include"
)

file(
    GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    *.cpp
)

if(NOT ENABLE_KDB)
    list(FILTER SOURCES EXCLUDE REGEX "/kdb_(connection|loader)\\.cpp$")
endif()

add_executable(risk_assessment_engine ${SOURCES})

target_include_directories(
    risk_assessment_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/../lib/CLI11/include
    ${CMAKE_SOURCE_DIR}/../lib/spdlog/include
)

if(ENABLE_KDB)
    target_compile_definitions(risk_assessment_engine PRIVATE ENABLE_KDB)

    target_include_directories(risk_assessment_engine PRIVATE
        ${CMAKE_SOURCE_DIR}/../lib/capi
    )

    if(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(KCAPI_OBJ ${CMAKE_SOURCE_DIR}/../lib/capi/m1/c.o)
        else()
            set(KCAPI_OBJ ${CMAKE_SOURCE_DIR}/../lib/capi/m64/c.o)
        endif()
    else()
        set(KCAPI_OBJ ${CMAKE_SOURCE_DIR}/../lib/capi/l64/c.o)
    endif()

    if(NOT EXISTS "${KCAPI_OBJ}")
        message(FATAL_ERROR "KDB+ C API object not found: ${KCAPI_OBJ}")
    endif()

    file(READ "${KCAPI_OBJ}" KCAPI_OBJ_MAGIC LIMIT 4 HEX)
    if(APPLE)
        if(NOT KCAPI_OBJ_MAGIC MATCHES "^(cffaedfe|feedfacf|cafebabe|bebafeca)$")
            message(FATAL_ERROR
                "KDB+ C API object is not a Mach-O binary: ${KCAPI_OBJ}. "
                "Obtain the macOS KDB+ C API object and replace this file; "
                "it may currently contain an HTML download page."
            )
        endif()
    elseif(NOT KCAPI_OBJ_MAGIC STREQUAL "7f454c46")
        message(FATAL_ERROR
            "KDB+ C API object is not an ELF binary: ${KCAPI_OBJ}. "
            "Obtain the Linux KDB+ C API object and replace this file."
        )
    endif()

    target_link_libraries(risk_assessment_engine PRIVATE ${KCAPI_OBJ})
endif()
