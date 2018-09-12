cmake_minimum_required(VERSION 3.12)

include_directories(/usr/include/gdal ../imgtools ../mattools
        ../../third_party_lib/Eigen3.3.5/include)
link_directories(/usr/lib)

aux_source_directory(. DIR_IMGALGO_SRCS)
add_library(imgalgo ${DIR_IMGALGO_SRCS} ia_anomalydetection.cpp)

find_package(Threads REQUIRED)
target_link_libraries(imgalgo gdal Threads::Threads)