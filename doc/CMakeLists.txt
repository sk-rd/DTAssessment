find_package(Doxygen REQUIRED)

set(DTA_DOXYGEN_INPUT_DIR "${CMAKE_SOURCE_DIR}/include")
set(DTA_DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/doc")
set(DTA_DOXYGEN_INDEX_FILE "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
    ${DTA_DOXYGEN_INDEX_FILE}
    @ONLY
)

add_custom_target(dta_docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${DTA_DOXYGEN_INDEX_FILE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM
)
