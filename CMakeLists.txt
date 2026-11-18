cmake_minimum_required(VERSION 3.21)

project(CameraIpSearch VERSION 0.1.0 DESCRIPTION "Cross-platform IP camera discovery and provisioning utility" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network Sql)
qt_standard_project_setup()

qt_add_executable(camera-ip-search
    src/main.cpp
    src/ui/main_window.cpp
    src/ui/main_window.h
)

target_link_libraries(camera-ip-search PRIVATE Qt6::Widgets Qt6::Network Qt6::Sql)
target_include_directories(camera-ip-search PRIVATE src)

if(MSVC)
    target_compile_options(camera-ip-search PRIVATE /W4 /permissive-)
else()
    target_compile_options(camera-ip-search PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS camera-ip-search RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} BUNDLE DESTINATION .)
