cmake_minimum_required(VERSION 3.12)

include_directories(/usr/include/gdal ../imgtools)
link_directories(/usr/lib)

aux_source_directory(. DIR_TEST_SRCS)
add_library(test ${DIR_TEST_SRCS} test_computestatistics.cpp)

find_package(Threads REQUIRED)
target_link_libraries(test gdal pthread Threads::Threads)