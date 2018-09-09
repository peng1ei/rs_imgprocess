cmake_minimum_required(VERSION 3.12)

include_directories(/usr/include/gdal ../imgtools)
link_directories(/usr/lib)

aux_source_directory(. DIR_TESTADD_SRCS)
add_library(test_add ${DIR_TESTADD_SRCS})

target_link_libraries(test_add gdal)