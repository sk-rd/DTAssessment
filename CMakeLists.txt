cmake_minimum_required(VERSION 3.20)

project(dta VERSION 0.1.0 LANGUAGES CXX)

option(DTA_BUILD_DOCS "Build API documentation with Doxygen" OFF)

add_library(dta INTERFACE)

add_library(dta::dta ALIAS dta)

target_include_directories(dta
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(dta INTERFACE cxx_std_17)

include(CTest)
if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
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
