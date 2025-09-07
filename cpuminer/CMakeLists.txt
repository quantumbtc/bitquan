cmake_minimum_required(VERSION 3.16)

project(cpuminer VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Windows specific settings
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Linux/Unix specific settings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native -mtune=native")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -D_GLIBCXX_USE_C99")

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find libcurl
find_package(CURL REQUIRED)
if(NOT CURL_FOUND)
    message(FATAL_ERROR "libcurl not found. Please install libcurl development package")
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
3. Manual: Download from https://github.com/nlohmann/json/releases")
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

include_directories(/usr/include)

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

# Compiler flags
target_compile_options(cpuminer PRIVATE
    -Wall
    -Wextra
    -O3
    -march=native
    -mtune=native
)

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
