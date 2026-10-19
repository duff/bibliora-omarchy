cmake_minimum_required(VERSION 3.24)

project(bibliora VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Qml Quick Concurrent Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE REQUIRED IMPORTED_TARGET sqlite3)

qt_standard_project_setup(REQUIRES 6.8)
qt_policy(SET QTP0004 NEW)

# The Bible itself (src/core, src/bible) is plain C++ over QtCore/QtGui and
# SQLite, in a static library, so the tests and the command line use exactly
# the code the window does.
file(GLOB_RECURSE BIBLIORA_CORE_SOURCES CONFIGURE_DEPENDS
    src/core/*.cpp src/core/*.h src/bible/*.cpp src/bible/*.h)
qt_add_library(bibliora_core STATIC ${BIBLIORA_CORE_SOURCES})
target_include_directories(bibliora_core PUBLIC src/core src/bible)
target_link_libraries(bibliora_core PUBLIC Qt6::Core Qt6::Gui PkgConfig::SQLITE)

# The window: QML-facing controllers in src/ui, the page renderer, and QML.
file(GLOB_RECURSE BIBLIORA_UI_SOURCES CONFIGURE_DEPENDS src/ui/*.cpp src/ui/*.h)
file(GLOB BIBLIORA_QML_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} qml/*.qml)
file(GLOB BIBLIORA_FONTS CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} resources/fonts/*.ttf)

qt_add_executable(bibliora src/app/main.cpp src/app/Instance.cpp src/app/Instance.h)
qt_add_qml_module(bibliora
    URI Bibliora
    VERSION 1.0
    QML_FILES ${BIBLIORA_QML_FILES}
    SOURCES ${BIBLIORA_UI_SOURCES}
    RESOURCES ${BIBLIORA_FONTS}
)
target_include_directories(bibliora PRIVATE src/app src/ui)
target_link_libraries(bibliora PRIVATE bibliora_core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::Concurrent)

target_compile_definitions(bibliora_core PUBLIC BIBLIORA_VERSION="${PROJECT_VERSION}")
target_compile_definitions(bibliora PRIVATE $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bibliora_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
    target_compile_options(bibliora PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------- tests
# One executable per tests/tst_*.cpp, run offscreen. Tests that read the
# Bible look for it where the app does and skip when it isn't there.
enable_testing()
file(GLOB BIBLIORA_TESTS CONFIGURE_DEPENDS tests/tst_*.cpp)
foreach(test_src ${BIBLIORA_TESTS})
    get_filename_component(test_name ${test_src} NAME_WE)
    qt_add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE bibliora_core Qt6::Test)
    target_compile_definitions(${test_name} PRIVATE BIBLIORA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

# ---------------------------------------------------------------- install
include(GNUInstallDirs)
install(TARGETS bibliora RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/bibliora.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/bibliora.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps)
# The agent skill. The window links it into each agent's skills folder.
install(DIRECTORY skills/bibliora DESTINATION ${CMAKE_INSTALL_DATADIR}/bibliora/skills)
# Bibliora's license, the reading fonts' (SIL Open Font License), and the
# translations' copyrights.
file(GLOB BIBLIORA_FONT_LICENSES resources/fonts/*-OFL.txt)
install(FILES LICENSE ${BIBLIORA_FONT_LICENSES} packaging/translations.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/bibliora)
