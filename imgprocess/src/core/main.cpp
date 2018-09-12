#include <iostream>
#include "gdal_priv.h"
#include "test_add/test_add.h"
#include "test/test_mpcomputestatistics.h"
#include "test/test_computestatistics.h"
#include "imgtool_measure.hpp"


#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "imgtool_progress.hpp"

int main() {
    //int size = sizeof(ImgTool::ImgSpectralSubset);
    //std::cout << "ImgSpectralSubset Size: " << size << std::endl;

    //size = sizeof(ImgTool::ImgSpatialSubset);
    //std::cout << "ImgSpatialSubset Size: " << size << std::endl;

    //size = sizeof(ImgTool::ImgBlockEnvelope);
    //std::cout << "ImgBlockEnvelope Size: " << size << std::endl;

    //size = sizeof(ImgTool::ImgBlockData<double>);
    //std::cout << "ImgBlockData Size: " << size << std::endl;

    //size = sizeof(ImgTool::ImgBlockDataRead<double>);
    //std::cout << "ImgBlockDataRead Size: " << size << std::endl;

    //size = sizeof(ImgTool::ImgBlockProcess<double>);
    //std::cout << "ImgBlockProcess Size: " << size << std::endl;

    std::string file_gf5("/home/penglei/data/GF5_AHSI_0943.tiff");
    std::string file_cup99("/home/penglei/data/cup99hy.tiff");
    std::string file_out("/home/penglei/data/temp/test.img");

    //test_add add("/home/penglei/data/GF5_AHSI_0943.tiff",
    //        "/home/penglei/data/temp/test_add4.img");

//    test_add add("/home/penglei/data/cup99hy.tiff",
//                 "/home/penglei/data/temp/test_add4.img");

    //std::cout << ImgTool::measure<>::execution(&test_add::run, &add) << " ms\n";
    //add.run();

    // 测试 多线程统计
    Test_MpComputeStatistics test_stats(file_gf5);

    ImgTool::printRunTime(ImgTool::measure<>::execution(&Test_MpComputeStatistics::run, &test_stats));
    //ImgTool::printRunTime(ImgTool::measure<>::execution(computeStatistics, file_gf5));

    return 0;
}