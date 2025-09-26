include_directories(
    "."
    "../include/risk"
)

file(
    GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(risk_assessment_engine ${SOURCES})

target_include_directories(
    risk_assessment_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/eigen
)
