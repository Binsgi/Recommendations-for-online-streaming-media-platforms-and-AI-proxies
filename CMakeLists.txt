cmake_minimum_required(VERSION 3.16)
project(demo_player CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -pthread")

# Output binary to bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Find Required Packages
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

# Find MySQL Client library
find_path(MYSQL_INCLUDE_DIR 
    NAMES mysql.h 
    PATHS /usr/include/mysql /usr/local/include/mysql /usr/include/x86_64-linux-gnu/mysql
)

find_library(MYSQL_LIBRARY 
    NAMES mysqlclient mariadbclient
    PATHS /usr/lib/mysql /usr/lib/x86_64-linux-gnu /usr/local/lib/mysql /usr/local/lib
)

if (NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(WARNING "MySQL client libraries not found automatically in standard paths. You can install via: sudo apt install libmysqlclient-dev")
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${MYSQL_INCLUDE_DIR}
)

# Source files
file(GLOB_RECURSE SRC_FILES 
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.c"
)

add_executable(demo_player ${SRC_FILES})

target_link_libraries(demo_player
    PRIVATE
    Threads::Threads
    ${CURL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    ${MYSQL_LIBRARY}
)

message(STATUS "Configured demo_player for C++17 on Linux (Ubuntu 22.04)")
