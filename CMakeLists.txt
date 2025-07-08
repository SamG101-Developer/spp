cmake_minimum_required(VERSION 3.31)
project(spp)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")

find_package(magic_enum REQUIRED)
find_package(ICU REQUIRED COMPONENTS uc)
find_package(Boost REQUIRED)
file(GLOB_RECURSE SOURCE_FILES "sources/*.cpp")

add_executable(${PROJECT_NAME} main.cpp ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/headers)
target_link_libraries(${PROJECT_NAME} PRIVATE magic_enum::magic_enum ICU::uc)
