cmake_minimum_required(VERSION 3.20)

project(DTAssessment VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost 1.74 REQUIRED COMPONENTS program_options)

add_executable(dtassessment
    src/main.cpp
)

target_link_libraries(dtassessment PRIVATE Boost::program_options)

if(MSVC)
    target_compile_options(dtassessment PRIVATE /W4 /permissive-)
else()
    target_compile_options(dtassessment PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS dtassessment RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
