cmake_minimum_required(VERSION 3.16)

project(cpuminer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform detection
if(WIN32)
    # Windows specific settings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /WX- /O2 /Oi /Ot /Oy /GL")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /DNDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
    set(CMAKE_WIN32_EXECUTABLE TRUE)
else()
    # Linux/Unix specific settings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native -mtune=native")
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find libcurl
find_package(CURL REQUIRED)

# Find JSON library
find_package(nlohmann_json REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/crypto)
include_directories(${CMAKE_SOURCE_DIR}/src/primitives)
include_directories(${CMAKE_SOURCE_DIR}/src/uint256.h)
include_directories(${CMAKE_SOURCE_DIR}/src/arith_uint256.h)
include_directories(${CMAKE_SOURCE_DIR}/src/util)
include_directories(${CMAKE_SOURCE_DIR}/src/streams.h)

# Source files
set(SOURCES
    main.cpp
    miner.cpp
    rpc_client.cpp
    randomq_miner.cpp
    config.cpp
)

# Create executable
add_executable(cpuminer ${SOURCES})

# Link libraries
target_link_libraries(cpuminer
    ${CURL_LIBRARIES}
    nlohmann_json::nlohmann_json
    Threads::Threads
)

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
        advapi32
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

# Copy crypto source files to build directory
file(COPY ${CMAKE_SOURCE_DIR}/src/crypto/randomq.cpp DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/src/crypto/randomq_hash.h DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/src/crypto/randomq.h DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/src/crypto/randomq_mining.cpp DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/src/crypto/randomq_mining.h DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/src/crypto/sha256.cpp DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/src/crypto/sha256.h DESTINATION ${CMAKE_BINARY_DIR})

# Windows specific: Copy DLLs
if(WIN32)
    # Copy curl DLL if found
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
    
    # Copy vcpkg DLLs if using vcpkg
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