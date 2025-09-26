include_directories(
    "."
)

file(
    GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(risk_assessment_engine ${SOURCES})
target_link_libraries(risk_assessment_engine PRIVATE spdlog::spdlog)