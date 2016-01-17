project(pbmpgfx)
cmake_minimum_required(VERSION 2.8)

option(edges "BUILD_EDGE_TEST" OFF)

if(BUILD_EDGE_TEST)
    set(SRC_LIST edge.c)
else()
    set(SRC_LIST bitmap.c)
endif()

add_executable(${PROJECT_NAME} ${SRC_LIST})

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -g -D_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O3")
set(CMAKE_CXX_FLAGS "-Wall -O3")

target_link_libraries(pbmpgfx m)
