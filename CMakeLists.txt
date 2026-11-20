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
    src/adapters/device_adapter.h
    src/app/batch_operation_preflight.h
    src/app/batch_operation_preflight.cpp
    src/app/ip_assignment_preflight.h
    src/app/ip_assignment_preflight.cpp
    src/app/batch_operation_runner.h
    src/app/batch_operation_runner.cpp
    src/adapters/onvif_adapter.h
    src/adapters/onvif_adapter.cpp
    src/adapters/adapter_registry.h
    src/adapters/adapter_registry.cpp
    src/adapters/network_config_validator.h
    src/adapters/network_config_validator.cpp
    src/core/device_record.h
    src/discovery/discovery_service.h
    src/discovery/discovery_parser.h
    src/discovery/discovery_parser.cpp
    src/discovery/discovery_service.cpp
    src/discovery/onvif_network_config.h
    src/discovery/onvif_network_config.cpp
    src/discovery/onvif_network_interface_parser.h
    src/discovery/onvif_network_interface_parser.cpp
    src/discovery/onvif_dns_parser.h
    src/discovery/onvif_dns_parser.cpp
    src/discovery/subnet_scanner.h
    src/discovery/subnet_scanner.cpp
    src/discovery/reachability_checker.h
    src/discovery/reachability_checker.cpp
    src/discovery/ip_conflict_probe.h
    src/discovery/ip_conflict_probe.cpp
    src/discovery/ip_assignment_planner.h
    src/discovery/ip_assignment_planner.cpp
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

    qt_add_executable(adapter-registry-tests
        tests/adapter_registry_test.cpp
        src/core/device_record.h
        src/adapters/device_adapter.h
        src/adapters/onvif_adapter.h
        src/adapters/onvif_adapter.cpp
        src/adapters/adapter_registry.h
        src/adapters/adapter_registry.cpp
    )
    target_include_directories(adapter-registry-tests PRIVATE src)
    target_link_libraries(adapter-registry-tests PRIVATE Qt6::Test Qt6::Network)
    add_test(NAME adapter-registry-tests COMMAND adapter-registry-tests)

    qt_add_executable(onvif-network-config-tests
        tests/onvif_network_config_test.cpp
        src/discovery/onvif_network_config.h
        src/discovery/onvif_network_config.cpp
    )
    target_include_directories(onvif-network-config-tests PRIVATE src)
    target_link_libraries(onvif-network-config-tests PRIVATE Qt6::Test Qt6::Xml)
    add_test(NAME onvif-network-config-tests COMMAND onvif-network-config-tests)

    qt_add_executable(onvif-network-interface-parser-tests
        tests/onvif_network_interface_parser_test.cpp
        src/discovery/onvif_network_interface_parser.h
        src/discovery/onvif_network_interface_parser.cpp
    )
    target_include_directories(onvif-network-interface-parser-tests PRIVATE src)
    target_link_libraries(onvif-network-interface-parser-tests PRIVATE Qt6::Test Qt6::Xml)
    add_test(NAME onvif-network-interface-parser-tests COMMAND onvif-network-interface-parser-tests)

    qt_add_executable(onvif-dns-parser-tests
        tests/onvif_dns_parser_test.cpp
        src/discovery/onvif_dns_parser.h
        src/discovery/onvif_dns_parser.cpp
    )
    target_include_directories(onvif-dns-parser-tests PRIVATE src)
    target_link_libraries(onvif-dns-parser-tests PRIVATE Qt6::Test Qt6::Network Qt6::Xml)
    add_test(NAME onvif-dns-parser-tests COMMAND onvif-dns-parser-tests)

    qt_add_executable(network-config-validator-tests
        tests/network_config_validator_test.cpp
        src/adapters/network_config_validator.h
        src/adapters/network_config_validator.cpp
    )
    target_include_directories(network-config-validator-tests PRIVATE src)
    target_link_libraries(network-config-validator-tests PRIVATE Qt6::Test Qt6::Network)
    add_test(NAME network-config-validator-tests COMMAND network-config-validator-tests)

    qt_add_executable(ip-assignment-planner-tests
        tests/ip_assignment_planner_test.cpp
        src/discovery/ip_assignment_planner.h
        src/discovery/ip_assignment_planner.cpp
    )
    target_include_directories(ip-assignment-planner-tests PRIVATE src)
    target_link_libraries(ip-assignment-planner-tests PRIVATE Qt6::Test Qt6::Network)
    add_test(NAME ip-assignment-planner-tests COMMAND ip-assignment-planner-tests)

    qt_add_executable(ip-assignment-preflight-tests
        tests/ip_assignment_preflight_test.cpp
        src/discovery/ip_assignment_planner.h
        src/app/ip_assignment_preflight.h
        src/app/ip_assignment_preflight.cpp
    )
    target_include_directories(ip-assignment-preflight-tests PRIVATE src)
    target_link_libraries(ip-assignment-preflight-tests PRIVATE Qt6::Test Qt6::Network)
    add_test(NAME ip-assignment-preflight-tests COMMAND ip-assignment-preflight-tests)

    qt_add_executable(ip-conflict-probe-tests
        tests/ip_conflict_probe_test.cpp
        src/app/ip_assignment_preflight.h
        src/discovery/ip_assignment_planner.h
        src/discovery/ip_conflict_probe.h
        src/discovery/ip_conflict_probe.cpp
    )
    target_include_directories(ip-conflict-probe-tests PRIVATE src)
    target_link_libraries(ip-conflict-probe-tests PRIVATE Qt6::Test Qt6::Network)
    add_test(NAME ip-conflict-probe-tests COMMAND ip-conflict-probe-tests)

    qt_add_executable(batch-operation-preflight-tests
        tests/batch_operation_preflight_test.cpp
        src/core/device_record.h
        src/adapters/device_adapter.h
        src/adapters/onvif_adapter.h
        src/adapters/onvif_adapter.cpp
        src/adapters/adapter_registry.h
        src/adapters/adapter_registry.cpp
        src/app/batch_operation_preflight.h
        src/app/batch_operation_preflight.cpp
    )
    target_include_directories(batch-operation-preflight-tests PRIVATE src)
    target_link_libraries(batch-operation-preflight-tests PRIVATE Qt6::Test Qt6::Network)
    add_test(NAME batch-operation-preflight-tests COMMAND batch-operation-preflight-tests)

    qt_add_executable(batch-operation-runner-tests
        tests/batch_operation_runner_test.cpp
        src/core/device_record.h
        src/adapters/device_adapter.h
        src/adapters/onvif_adapter.h
        src/adapters/onvif_adapter.cpp
        src/adapters/adapter_registry.h
        src/adapters/adapter_registry.cpp
        src/app/batch_operation_preflight.h
        src/app/batch_operation_preflight.cpp
        src/app/batch_operation_runner.h
        src/app/batch_operation_runner.cpp
    )
    target_include_directories(batch-operation-runner-tests PRIVATE src)
    target_link_libraries(batch-operation-runner-tests PRIVATE Qt6::Test Qt6::Network)
    add_test(NAME batch-operation-runner-tests COMMAND batch-operation-runner-tests)
endif()

include(GNUInstallDirs)
install(TARGETS camera-ip-search RUNTIME DESTINATION \${CMAKE_INSTALL_BINDIR} BUNDLE DESTINATION .)
