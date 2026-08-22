cmake_minimum_required(VERSION 3.24)
project(AIRelayBench VERSION 1.2.1 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "AI Relay Bench is Windows only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets Network Sql Test)

qt_add_executable(ai-relay-bench WIN32
    src/main.cpp
    src/domain/Types.h
    src/domain/Types.cpp
    src/domain/Statistics.h
    src/domain/Statistics.cpp
    src/protocol/SseDecoder.h
    src/protocol/SseDecoder.cpp
    src/protocol/ProtocolAdapter.h
    src/protocol/ProtocolAdapter.cpp
    src/network/BenchmarkJob.h
    src/network/BenchmarkJob.cpp
    src/services/BenchmarkController.h
    src/services/BenchmarkController.cpp
    src/services/ScanService.h
    src/services/ScanService.cpp
    src/services/ModelService.h
    src/services/ModelService.cpp
    src/services/ProfileStore.h
    src/services/ProfileStore.cpp
    src/services/ExportService.h
    src/services/ExportService.cpp
    src/persistence/SecureStorage.h
    src/persistence/SecureStorage.cpp
    src/persistence/ResultRepository.h
    src/persistence/ResultRepository.cpp
    src/security/RedactionService.h
    src/security/RedactionService.cpp
    src/capture/CaptureTypes.cpp
    src/capture/HttpParser.cpp
    src/capture/ClientFingerprint.cpp
    src/capture/HarExporter.cpp
    src/capture/CaptureProxyServer.cpp
    src/ui/CaptureDialog.h
    src/ui/CaptureDialog.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/TrendChart.h
    src/ui/TrendChart.cpp
    src/i18n/LanguageManager.h
    src/i18n/LanguageManager.cpp
    src/ui/ThemeManager.h
    src/ui/ThemeManager.cpp
    src/ui/TitleBar.h
    src/ui/TitleBar.cpp
    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp
    src/ui/ScanDialog.h
    src/ui/ScanDialog.cpp
    src/ui/CompareDialog.h
    src/ui/CompareDialog.cpp
    resources/resources.qrc
)

target_include_directories(ai-relay-bench PRIVATE src)
target_link_libraries(ai-relay-bench PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Sql)
if(WIN32)
    target_link_libraries(ai-relay-bench PRIVATE crypt32)
endif()

target_compile_definitions(ai-relay-bench PRIVATE APP_VERSION="${PROJECT_VERSION}")

qt_add_executable(ai-relay-bench-tests
    tests/tst_core.cpp
    src/domain/Types.cpp
    src/domain/Statistics.cpp
    src/protocol/SseDecoder.cpp
    src/protocol/ProtocolAdapter.cpp
    src/security/RedactionService.cpp
    src/services/ScanService.cpp
)
target_include_directories(ai-relay-bench-tests PRIVATE src)
target_link_libraries(ai-relay-bench-tests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
add_test(NAME core-tests COMMAND ai-relay-bench-tests)

qt_add_executable(ai-relay-bench-language-tests
    tests/tst_language.cpp
    src/i18n/LanguageManager.cpp
    src/ui/CaptureDialog.cpp
    src/ui/SettingsDialog.cpp
    src/ui/TitleBar.cpp
    src/ui/ThemeManager.cpp
    src/capture/CaptureTypes.cpp
    src/capture/HttpParser.cpp
    src/capture/ClientFingerprint.cpp
    src/capture/HarExporter.cpp
    src/capture/CaptureProxyServer.cpp
    src/security/RedactionService.cpp
    src/protocol/SseDecoder.cpp
    resources/resources.qrc
)
target_include_directories(ai-relay-bench-language-tests PRIVATE src)
target_link_libraries(ai-relay-bench-language-tests PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Test)
add_test(NAME language-tests COMMAND ai-relay-bench-language-tests)

qt_add_executable(ai-relay-bench-capture-tests
    tests/tst_capture.cpp
    src/security/RedactionService.cpp
    src/protocol/SseDecoder.cpp
    src/capture/CaptureTypes.cpp
    src/capture/HttpParser.cpp
    src/capture/ClientFingerprint.cpp
    src/capture/HarExporter.cpp
    src/capture/CaptureProxyServer.cpp
)
target_include_directories(ai-relay-bench-capture-tests PRIVATE src)
target_link_libraries(ai-relay-bench-capture-tests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
add_test(NAME capture-tests COMMAND ai-relay-bench-capture-tests)

qt_add_executable(ai-relay-bench-integration-tests
    tests/tst_integration.cpp
    src/domain/Types.cpp
    src/protocol/SseDecoder.cpp
    src/protocol/ProtocolAdapter.cpp
    src/network/BenchmarkJob.cpp
    src/security/RedactionService.cpp
    src/services/ScanService.cpp
    src/services/ExportService.cpp
)
target_include_directories(ai-relay-bench-integration-tests PRIVATE src)
target_link_libraries(ai-relay-bench-integration-tests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
target_compile_definitions(ai-relay-bench-integration-tests PRIVATE APP_VERSION="${PROJECT_VERSION}")
add_test(NAME integration-tests COMMAND ai-relay-bench-integration-tests)

enable_testing()

install(TARGETS ai-relay-bench RUNTIME DESTINATION .)
qt_generate_deploy_app_script(
    TARGET ai-relay-bench
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
