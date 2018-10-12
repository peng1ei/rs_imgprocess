cmake_minimum_required(VERSION 3.12)

include_directories(/usr/include/gdal
        ../../third_party_lib/Eigen3.3.5/include)
link_directories(/usr/lib)

aux_source_directory(. DIR_MATTOOLS_SRCS)
add_library(mattools ${DIR_MATTOOLS_SRCS} mg_datasetmanager.cpp mg_cube.cpp mg_cube.h mg_computestatistics.cpp mg_computestatistics.h mg_progress.hpp)

find_package(Threads REQUIRED)
target_link_libraries(mattools gdal Threads::Threads)