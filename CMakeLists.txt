cmake_minimum_required(VERSION 3.21)

project(CameraIpSearch VERSION 0.2.0 DESCRIPTION "Cross-platform multi-brand IP camera discovery and provisioning utility" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(CTest)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network Sql Xml Concurrent)
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
endif()
qt_standard_project_setup()

# Compile application code once and share it between the desktop executable and
# focused test binaries. This avoids recompiling the same implementation files
# for every test target, keeping CI resource usage predictable.
set(CAMERA_CORE_SOURCES
    src/adapters/device_adapter.h
    src/adapters/onvif_adapter.h
    src/adapters/onvif_adapter.cpp
    src/adapters/adapter_registry.h
    src/adapters/adapter_registry.cpp
    src/adapters/network_config_validator.h
    src/adapters/network_config_validator.cpp
    src/app/batch_operation_preflight.h
    src/app/batch_operation_preflight.cpp
    src/app/ip_assignment_preflight.h
    src/app/ip_assignment_preflight.cpp
    src/app/batch_operation_runner.h
    src/app/batch_operation_runner.cpp
    src/core/device_record.h
    src/core/device_classifier.h
    src/core/device_classifier.cpp
    src/core/operation_compatibility.h
    src/core/operation_compatibility.cpp
    src/core/device_identity.h
    src/core/device_identity.cpp
    src/core/inventory_backup.h
    src/core/inventory_backup.cpp
    src/oxita_branding.qrc
    src/core/diagnostic_logger.h
    src/core/diagnostic_logger.cpp
    src/core/diagnostic_log.h
    src/core/diagnostic_log.cpp
    src/firmware/firmware_package_validator.h
    src/firmware/firmware_package_validator.cpp
    src/discovery/discovery_service.h
    src/discovery/discovery_service.cpp
    src/discovery/discovery_parser.h
    src/discovery/discovery_parser.cpp
    src/discovery/ssdp_parser.h
    src/discovery/ssdp_parser.cpp
    src/discovery/onvif_network_config.h
    src/discovery/onvif_network_config.cpp
    src/discovery/onvif_maintenance_operations.h
    src/discovery/onvif_maintenance_operations.cpp
    src/discovery/subnet_scanner.h
    src/discovery/subnet_scanner.cpp
    src/discovery/reachability_checker.h
    src/discovery/reachability_checker.cpp
    src/discovery/onvif_network_interface_parser.h
    src/discovery/onvif_network_interface_parser.cpp
    src/discovery/onvif_dns_parser.h
    src/discovery/onvif_dns_parser.cpp
    src/discovery/onvif_service_parser.h
    src/discovery/onvif_service_parser.cpp
    src/discovery/onvif_soap_fault_parser.h
    src/discovery/onvif_soap_fault_parser.cpp
    src/discovery/onvif_wsse.h
    src/discovery/onvif_wsse.cpp
    src/discovery/ip_conflict_probe.h
    src/discovery/ip_conflict_probe.cpp
    src/discovery/ip_assignment_planner.h
    src/discovery/ip_assignment_planner.cpp
    src/ui/main_window.h
    src/ui/main_window.cpp
    src/ui/oxita_theme.h
)

add_library(camera-core STATIC ${CAMERA_CORE_SOURCES})
target_include_directories(camera-core PUBLIC src)
target_link_libraries(camera-core PUBLIC Qt6::Widgets Qt6::Network Qt6::Sql Qt6::Xml Qt6::Concurrent)

if(WIN32)
    target_link_libraries(camera-core PUBLIC iphlpapi)
endif()

if(MSVC)
    target_compile_options(camera-core PRIVATE /W4 /permissive-)
else()
    target_compile_options(camera-core PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(camera-ip-search src/main.cpp)
if(APPLE)
    # macdeployqt expects a real .app bundle, not a loose installed executable.
    set_target_properties(camera-ip-search PROPERTIES MACOSX_BUNDLE TRUE)
endif()
target_link_libraries(camera-ip-search PRIVATE camera-core)

if(BUILD_TESTING)
    function(add_camera_test target source)
        qt_add_executable(${target} ${source})
        target_link_libraries(${target} PRIVATE camera-core Qt6::Test)
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endfunction()

    add_camera_test(discovery-parser-tests tests/discovery_parser_test.cpp)
    add_camera_test(ssdp-parser-tests tests/ssdp_parser_test.cpp)
    add_camera_test(adapter-registry-tests tests/adapter_registry_test.cpp)
    add_camera_test(onvif-network-config-tests tests/onvif_network_config_test.cpp)
    add_camera_test(onvif-network-write-integration-tests tests/onvif_network_write_integration_test.cpp)
    add_camera_test(onvif-maintenance-operations-tests tests/onvif_maintenance_operations_test.cpp)
    add_camera_test(network-config-validator-tests tests/network_config_validator_test.cpp)
    add_camera_test(subnet-scanner-tests tests/subnet_scanner_test.cpp)
    add_camera_test(reachability-checker-tests tests/reachability_checker_test.cpp)
    add_camera_test(device-identity-tests tests/device_identity_test.cpp)
    add_camera_test(device-classifier-tests tests/device_classifier_test.cpp)
    add_camera_test(operation-compatibility-tests tests/operation_compatibility_test.cpp)
    add_camera_test(inventory-backup-tests tests/inventory_backup_test.cpp)
    add_camera_test(onvif-network-interface-parser-tests tests/onvif_network_interface_parser_test.cpp)
    add_camera_test(onvif-dns-parser-tests tests/onvif_dns_parser_test.cpp)
    add_camera_test(onvif-service-parser-tests tests/onvif_service_parser_test.cpp)
    add_camera_test(onvif-soap-fault-parser-tests tests/onvif_soap_fault_parser_test.cpp)
    add_camera_test(onvif-wsse-tests tests/onvif_wsse_test.cpp)
    add_camera_test(ip-assignment-planner-tests tests/ip_assignment_planner_test.cpp)
    add_camera_test(ip-assignment-preflight-tests tests/ip_assignment_preflight_test.cpp)
    add_camera_test(ip-conflict-probe-tests tests/ip_conflict_probe_test.cpp)
    add_camera_test(batch-operation-preflight-tests tests/batch_operation_preflight_test.cpp)
    add_camera_test(batch-operation-runner-tests tests/batch_operation_runner_test.cpp)
    add_camera_test(firmware-package-validator-tests tests/firmware_package_validator_test.cpp)
    add_camera_test(diagnostic-logger-tests tests/diagnostic_logger_test.cpp)
    add_camera_test(diagnostic-log-tests tests/diagnostic_log_test.cpp)
    add_camera_test(discovery-wire-integration-tests tests/discovery_wire_integration_test.cpp)
    add_camera_test(main-window-smoke-tests tests/main_window_smoke_test.cpp)
endif()

include(GNUInstallDirs)
install(TARGETS camera-ip-search RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} BUNDLE DESTINATION .)
