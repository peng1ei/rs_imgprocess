cmake_minimum_required(VERSION 3.12)

set(TARGET_NAME imgprocess)
project(${TARGET_NAME})

# 只能一个目录一个目录添加
add_subdirectory(test_add)
add_subdirectory(test)

set(CMAKE_CXX_STANDARD 11)

include_directories(/usr/include/gdal
        imgtools)

link_directories(/usr/lib)
#link_libraries(/usr/lib/libgdal.a)

add_executable(${TARGET_NAME} main.cpp)

find_package(Threads REQUIRED)

target_link_libraries(${TARGET_NAME}
        gdal
        test_add
        Threads::Threads
        test
        )