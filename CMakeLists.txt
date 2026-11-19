cmake_minimum_required(VERSION 3.21)

project(CameraIpSearch VERSION 0.2.0 DESCRIPTION "Cross-platform multi-brand IP camera discovery and provisioning utility" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network Sql Xml Test)
qt_standard_project_setup()

qt_add_executable(camera-ip-search
    src/main.cpp
    src/core/device_record.h
    src/discovery/discovery_service.h
    src/discovery/discovery_parser.h
    src/discovery/discovery_parser.cpp
    src/discovery/discovery_service.cpp
    src/discovery/subnet_scanner.h
    src/discovery/subnet_scanner.cpp
    src/discovery/reachability_checker.h
    src/discovery/reachability_checker.cpp
    src/ui/main_window.cpp
    src/ui/main_window.h
)

target_link_libraries(camera-ip-search PRIVATE Qt6::Widgets Qt6::Network Qt6::Sql Qt6::Xml)
target_include_directories(camera-ip-search PRIVATE src)

if(MSVC)
    target_compile_options(camera-ip-search PRIVATE /W4 /permissive-)
else()
    target_compile_options(camera-ip-search PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(discovery-parser-tests
        tests/discovery_parser_test.cpp
        src/core/device_record.h
        src/discovery/discovery_parser.h
        src/discovery/discovery_parser.cpp
    )
    target_include_directories(discovery-parser-tests PRIVATE src)
    target_link_libraries(discovery-parser-tests PRIVATE Qt6::Test Qt6::Network Qt6::Xml)
    add_test(NAME discovery-parser-tests COMMAND discovery-parser-tests)
endif()

include(GNUInstallDirs)
install(TARGETS camera-ip-search RUNTIME DESTINATION \${CMAKE_INSTALL_BINDIR} BUNDLE DESTINATION .)
