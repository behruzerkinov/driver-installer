cmake_minimum_required(VERSION 3.25)
project(DriverNest
    VERSION 1.3.1
    DESCRIPTION "Free, lightweight, portable Windows driver installer"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Global compiler options
add_compile_options(
    /W4
    /permissive-        # Strict conformance
    /utf-8              # Source and execution charset UTF-8
    /Zc:__cplusplus     # Correct __cplusplus
    /Zc:referenceBinding  # Strict reference binding
    /Zc:throwingNew     # new throws on failure
    /Zc:char8_t-        # u8string() returns std::string (std::filesystem interop)
)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Install prefix logic
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/dist")
endif()

# Windows SDK version
set(CMAKE_SYSTEM_VERSION 10.0.22621.0)

# Source groups for IDE
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX "Source Files"
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/App.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/Settings.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/Log.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/hw/DeviceEnum.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/hw/DriverState.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/packs/PackIndex.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/packs/InfParser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/packs/PackScanner.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/packs/Ranker.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/install/Installer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/install/RestorePoint.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/install/Verify.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/install/InstallHistory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/net/Downloader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/net/Hash.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/MainWindow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/DeviceList.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/DetailsPane.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Theme.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Backdrop.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Dialogs.cpp
)

# --- DriverNest main executable ---
add_executable(DriverNest)

target_sources(DriverNest
    PRIVATE
        # main entry
        src/main.cpp

        # app layer
        src/app/App.cpp
        src/app/Settings.cpp
        src/app/Log.cpp

        # hardware enumeration
        src/hw/DeviceEnum.cpp
        src/hw/DriverState.cpp

        # driver packs
        src/packs/PackIndex.cpp
        src/packs/InfParser.cpp
        src/packs/PackScanner.cpp
        src/packs/Ranker.cpp

        # installation
        src/install/Installer.cpp
        src/install/RestorePoint.cpp
        src/install/Verify.cpp
        src/install/InstallHistory.cpp

        # networking
        src/net/Downloader.cpp
        src/net/Hash.cpp

        # UI
        src/ui/MainWindow.cpp
        src/ui/DeviceList.cpp
        src/ui/DetailsPane.cpp
        src/ui/Theme.cpp
        src/ui/Backdrop.cpp
        src/ui/Dialogs.cpp
)

target_include_directories(DriverNest
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app
        ${CMAKE_CURRENT_SOURCE_DIR}/src/hw
        ${CMAKE_CURRENT_SOURCE_DIR}/src/packs
        ${CMAKE_CURRENT_SOURCE_DIR}/src/install
        ${CMAKE_CURRENT_SOURCE_DIR}/src/net
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
)

# Link system libraries
target_link_libraries(DriverNest
    PRIVATE
        setupapi
        advapi32
        ole32
        oleaut32
        comctl32
        user32
        gdi32
        dwmapi
        shlwapi
        version
        cfgmgr32
        crypt32
        bcrypt
        srclient
        newdev
        winhttp
        shell32
        uxtheme
        wintrust
        msimg32
        dbghelp
)

target_compile_definitions(DriverNest
    PRIVATE
        UNICODE
        _UNICODE
        WINVER=0x0A00       # Windows 10
        _WIN32_WINNT=0x0A00
        _WIN32_IE=0x0A00
        NTDDI_VERSION=NTDDI_WIN10_NI   # 10.0.19041.0
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        $<$<CONFIG:Release>:NDEBUG>
        $<$<CONFIG:Debug>:_DEBUG;DEBUG>
)

# Generate version resource from CMake project version
# Configure the .rc file with version info from CMake
set(DN_PROJECT_URL "https://github.com/example/drivernest" CACHE STRING "Project URL for About dialog")
string(TIMESTAMP CURRENT_YEAR "%Y" UTC)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.rc"
    "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
    @ONLY
)

# Add the configured RC file and icon
target_sources(DriverNest
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/app.rc
        assets/app.ico
)

set_target_properties(DriverNest
    PROPERTIES
        OUTPUT_NAME DriverNest
        LINK_FLAGS "/MANIFEST:NO"   # manifest embedded via rc
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

# Enable /guard:cf, /DYNAMICBASE, /NXCOMPAT, /CETCOMPAT, /HIGHENTROPYVA via linker options
target_link_options(DriverNest
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:
            /GUARD:CF
            /DYNAMICBASE
            /NXCOMPAT
            /CETCOMPAT
            /HIGHENTROPYVA
        >
)

# Debug/Release flags
target_compile_options(DriverNest
    PRIVATE
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:
            /GL
            /O2
            /MT
        >
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:
            /Od
            /Zi
            /MTd
        >
)

set_target_properties(DriverNest
    PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
        INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF
        DEBUG_POSTFIX ""  # no d suffix
)

# --- Tests executable ---
option(DN_BUILD_TESTS "Build unit tests" ON)

if(DN_BUILD_TESTS)
    enable_testing()

    add_executable(DriverNestTests)
    set_target_properties(DriverNestTests PROPERTIES VS_GLOBAL_PLATFORMToolset "v143")

    target_sources(DriverNestTests
        PRIVATE
            tests/main_test.cpp
            tests/InfParser.test.cpp
            tests/Ranker.test.cpp
            tests/PackIndex.test.cpp
            tests/Hash.test.cpp
            tests/PathValidate.test.cpp
            tests/InstallHistory.test.cpp

            # Code under test (non-UI translation units).
            src/app/Log.cpp
            src/hw/DriverState.cpp
            src/packs/InfParser.cpp
            src/packs/PackIndex.cpp
            src/packs/PackScanner.cpp
            src/packs/Ranker.cpp
            src/install/Verify.cpp
            src/install/InstallHistory.cpp
            src/net/Hash.cpp
    )

    target_include_directories(DriverNestTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/app
            ${CMAKE_CURRENT_SOURCE_DIR}/src/hw
            ${CMAKE_CURRENT_SOURCE_DIR}/src/packs
            ${CMAKE_CURRENT_SOURCE_DIR}/src/install
            ${CMAKE_CURRENT_SOURCE_DIR}/src/net
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    target_link_libraries(DriverNestTests
        PRIVATE
            bcrypt
            crypt32
            wintrust
    )

    target_compile_definitions(DriverNestTests
        PRIVATE
            UNICODE
            _UNICODE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            WINVER=0x0A00
            _WIN32_WINNT=0x0A00
            _WIN32_IE=0x0A00
    )

    target_compile_options(DriverNestTests
        PRIVATE
            /W4
            /permissive-
            /utf-8
            /Zc:char8_t-
            /MT
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:Debug>:/Od;/Zi>
    )

    target_link_options(DriverNestTests
        PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:
                /GUARD:CF
                /DYNAMICBASE
                /HIGHENTROPYVA
            >
    )

    set_target_properties(DriverNestTests
        PROPERTIES
            OUTPUT_NAME DriverNestTests
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    # CTest preset integration
    add_test(
        NAME DriverNestTests
        COMMAND DriverNestTests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# --- CPack packaging ---
set(CPACK_PACKAGE_NAME "DriverNest")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP")
include(CPack)
