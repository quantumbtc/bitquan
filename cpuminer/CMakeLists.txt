cmake_minimum_required(VERSION 3.16)

project(cpuminer VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Platform detection and settings
if(WIN32)
    # Windows specific settings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /WX- /O2 /Oi /Ot /Oy /GL")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /DNDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
    set(CMAKE_WIN32_EXECUTABLE TRUE)
else()
    # Linux/Unix specific settings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native -mtune=native")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -D_GLIBCXX_USE_C99")
    
    # Check if we're using MinGW cross-compiler
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER MATCHES "mingw")
        message(STATUS "Detected MinGW cross-compiler")
        set(CMAKE_SYSTEM_NAME Windows)
        set(CMAKE_SYSTEM_PROCESSOR x86_64)
        set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
        set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
        set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
        set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__USE_MINGW_ANSI_STDIO=1")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_WIN32_WINNT=0x0601")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_WIN32_IE=0x0800")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_WIN32 -D_WIN64")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isystem /usr/x86_64-w64-mingw32/include")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isystem /usr/lib/gcc/x86_64-w64-mingw32/13-win32/include")
    endif()
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find libcurl
find_package(CURL REQUIRED)
if(NOT CURL_FOUND)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER MATCHES "mingw")
        pkg_check_modules(CURL REQUIRED libcurl)
        if(CURL_FOUND)
            message(STATUS "Found libcurl via pkg-config")
            set(CURL_LIBRARIES ${CURL_LIBRARIES})
            set(CURL_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
        else()
            find_library(CURL_LIBRARY
                NAMES curl libcurl
                PATHS /usr/x86_64-w64-mingw32/lib
                NO_DEFAULT_PATH
            )
            find_path(CURL_INCLUDE_DIR
                NAMES curl/curl.h
                PATHS /usr/x86_64-w64-mingw32/include
                NO_DEFAULT_PATH
            )
            if(CURL_LIBRARY AND CURL_INCLUDE_DIR)
                message(STATUS "Found libcurl manually for MinGW")
                set(CURL_LIBRARIES ${CURL_LIBRARY})
                set(CURL_INCLUDE_DIRS ${CURL_INCLUDE_DIR})
                set(CURL_FOUND TRUE)
            else()
                message(FATAL_ERROR "libcurl not found for MinGW. Please install MinGW libcurl development package")
            endif()
        endif()
    else()
        message(FATAL_ERROR "libcurl not found. Please install libcurl development package")
    endif()
endif()

# Find JSON library
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    pkg_check_modules(NLOHMANN_JSON QUIET nlohmann_json)
    if(NLOHMANN_JSON_FOUND)
        message(STATUS "Found nlohmann_json via pkg-config")
        set(nlohmann_json_FOUND TRUE)
    else()
        find_path(NLOHMANN_JSON_INCLUDE_DIR 
            NAMES nlohmann/json.hpp
            PATHS /usr/include /usr/local/include
        )
        if(NLOHMANN_JSON_INCLUDE_DIR)
            message(STATUS "Found nlohmann_json header-only version")
            set(nlohmann_json_FOUND TRUE)
            add_library(nlohmann_json INTERFACE)
            target_include_directories(nlohmann_json INTERFACE ${NLOHMANN_JSON_INCLUDE_DIR})
        else()
            message(FATAL_ERROR "nlohmann_json not found. Please install it using one of the following methods:
1. Ubuntu/Debian: sudo apt-get install nlohmann-json3-dev
2. CentOS/RHEL: sudo yum install nlohmann-json3-devel
3. Manual: Download from https://github.com/nlohmann/json/releases
4. vcpkg: vcpkg install nlohmann-json")
        endif()
    endif()
endif()

# Source files
set(SOURCES
    main.cpp
    miner.cpp
    rpc_client.cpp
    randomq_miner.cpp
    config.cpp
    # Crypto source files
    ${CMAKE_SOURCE_DIR}/../src/crypto/randomq.cpp
    ${CMAKE_SOURCE_DIR}/../src/crypto/randomq_mining.cpp
    ${CMAKE_SOURCE_DIR}/../src/crypto/sha256.cpp
)

# Create executable
add_executable(cpuminer ${SOURCES})

# Include directories
target_include_directories(cpuminer PRIVATE
    ${CMAKE_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/../src/crypto
    ${CMAKE_SOURCE_DIR}/../src/primitives
    ${CMAKE_SOURCE_DIR}/../src/util
)

# Link libraries
target_link_libraries(cpuminer
    ${CURL_LIBRARIES}
    Threads::Threads
)

# Link JSON library
if(TARGET nlohmann_json)
    target_link_libraries(cpuminer nlohmann_json)
elseif(NLOHMANN_JSON_FOUND)
    target_link_libraries(cpuminer ${NLOHMANN_JSON_LIBRARIES})
    target_include_directories(cpuminer PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})
    target_compile_options(cpuminer PRIVATE ${NLOHMANN_JSON_CFLAGS_OTHER})
endif()

# Platform specific linking
if(WIN32)
    target_link_libraries(cpuminer
        ws2_32
        winmm
        wldap32
        crypt32
        normaliz
        advapi32
        user32
        gdi32
        winspool
        shell32
        ole32
        oleaut32
        uuid
        comdlg32
    )
endif()

# Compiler flags
if(MSVC)
    target_compile_options(cpuminer PRIVATE
        /W3
        /WX-
        /O2
        /Oi
        /Ot
        /Oy
        /GL
        /DNDEBUG
        /DWIN32_LEAN_AND_MEAN
        /DNOMINMAX
    )
else()
    target_compile_options(cpuminer PRIVATE
        -Wall
        -Wextra
        -O3
        -march=native
        -mtune=native
    )
endif()

# Windows specific: Copy DLLs
if(WIN32)
    find_file(CURL_DLL
        NAMES libcurl.dll curl.dll
        PATHS ${CURL_LIBRARY_DIR} ${CURL_LIBRARY_DIR}/../bin
        NO_DEFAULT_PATH
    )
    if(CURL_DLL)
        add_custom_command(TARGET cpuminer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CURL_DLL} $<TARGET_FILE_DIR:cpuminer>
        )
    endif()
    
    if(DEFINED VCPKG_TARGET_TRIPLET)
        find_file(VCPKG_CURL_DLL
            NAMES libcurl.dll
            PATHS ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/bin
            NO_DEFAULT_PATH
        )
        if(VCPKG_CURL_DLL)
            add_custom_command(TARGET cpuminer POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${VCPKG_CURL_DLL} $<TARGET_FILE_DIR:cpuminer>
            )
        endif()
    endif()
endif()

# Set output directory
if(WIN32)
    set_target_properties(cpuminer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

# Install rules
install(TARGETS cpuminer
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Package configuration
set(CPACK_PACKAGE_NAME "cpuminer")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Bitquantum RandomQ CPU Miner")
set(CPACK_PACKAGE_VENDOR "Bitquantum")

include(CPack)