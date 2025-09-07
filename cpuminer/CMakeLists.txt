cmake_minimum_required(VERSION 3.16)

project(cpuminer VERSION 1.0.0 LANGUAGES C CXX)

# =============================
# 交叉编译工具链设置
# =============================
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# 限制查找路径到 MinGW sysroot
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# =============================
# 编译选项
# =============================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native -mtune=native")

# =============================
# 手动寻找 libcurl
# =============================
find_path(CURL_INCLUDE_DIR curl/curl.h
    PATHS /usr/x86_64-w64-mingw32/include
)
find_library(CURL_LIBRARY NAMES curl libcurl
    PATHS /usr/x86_64-w64-mingw32/lib
)

if(NOT CURL_LIBRARY OR NOT CURL_INCLUDE_DIR)
    message(FATAL_ERROR "Cross-compiled libcurl not found in /usr/x86_64-w64-mingw32")
endif()

# =============================
# 手动寻找 nlohmann_json (header-only)
# =============================
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
    PATHS /usr/x86_64-w64-mingw32/include
)

if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    message(FATAL_ERROR "Cross-compiled nlohmann_json not found in /usr/x86_64-w64-mingw32")
endif()

# =============================
# 源文件
# =============================
set(SOURCES
    main.cpp
    miner.cpp
    rpc_client.cpp
    randomq_miner.cpp
    config.cpp
    ${CMAKE_SOURCE_DIR}/../src/crypto/randomq.cpp
    ${CMAKE_SOURCE_DIR}/../src/crypto/randomq_mining.cpp
    ${CMAKE_SOURCE_DIR}/../src/crypto/sha256.cpp
)

# =============================
# 构建可执行文件
# =============================
add_executable(cpuminer ${SOURCES})

# 包含目录
target_include_directories(cpuminer PRIVATE
    ${CURL_INCLUDE_DIR}
    ${NLOHMANN_JSON_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/../src/crypto
    ${CMAKE_SOURCE_DIR}/../src/primitives
    ${CMAKE_SOURCE_DIR}/../src/util
)

# 链接库
target_link_libraries(cpuminer
    ${CURL_LIBRARY}
    ws2_32      # Windows sockets
    winmm       # Windows multimedia (time functions)
    bcrypt      # crypto functions
)

# =============================
# 安装规则
# =============================
install(TARGETS cpuminer
    RUNTIME DESTINATION bin
)
