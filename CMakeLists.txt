cmake_minimum_required(VERSION 3.20)

project(dta VERSION 0.1.0 LANGUAGES CXX)

option(DTA_BUILD_DOCS "Build API documentation with Doxygen" OFF)
option(DTA_BUILD_PYTHON_BINDINGS "Build Python bindings with Boost.Python" ON)

add_library(dta INTERFACE)

add_library(dta::dta ALIAS dta)

target_include_directories(dta
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(dta INTERFACE cxx_std_17)

if(DTA_BUILD_PYTHON_BINDINGS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
    find_package(Boost CONFIG REQUIRED COMPONENTS python)

    add_library(dta_python MODULE
        src/python_bindings.cpp
    )

    target_link_libraries(dta_python
        PRIVATE
            dta
            Boost::python
            Python3::Python
    )

    set_target_properties(dta_python PROPERTIES
        PREFIX ""
        OUTPUT_NAME dta
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()

if(DTA_BUILD_DOCS)
    add_subdirectory(doc)
endif()

install(TARGETS dta
    EXPORT dtaTargets
    INCLUDES DESTINATION include
)

install(DIRECTORY include/ DESTINATION include)
