cmake_minimum_required(VERSION 3.31)
project(spp)

set(CMAKE_CXX_STANDARD 26)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror -Wno-nonnull-compare")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
  -Wall -Wextra -Wpedantic -Werror \
  -Wcast-align -Wcast-qual -Wconversion \
  -Wold-style-cast -Woverloaded-virtual -Wundef \
  -Wnull-dereference -Wformat=2 -Wduplicated-cond -Wduplicated-branches \
  -Wlogical-op -Wuseless-cast -Wdouble-promotion -Wstrict-overflow=5 \
  -Wswitch-enum -Wswitch-default -Wno-nonnull-compare")

find_package(ColEx REQUIRED)
find_package(GenEx REQUIRED)

find_package(magic_enum REQUIRED)
find_package(rapidfuzz REQUIRED)
find_package(Boost REQUIRED)

file(GLOB_RECURSE SOURCE_FILES "sources/*.cpp")

add_executable(${PROJECT_NAME} main.cpp ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/headers)
target_link_libraries(${PROJECT_NAME} PRIVATE ColEx::ColExLib GenEx::GenExLib)
target_link_libraries(${PROJECT_NAME} PRIVATE magic_enum::magic_enum rapidfuzz::rapidfuzz Boost::boost)
