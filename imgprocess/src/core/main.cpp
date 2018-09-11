#include <iostream>
#include "gdal_priv.h"
#include "test_add/test_add.h"
//#include "imgtool_imgblockprocess.hpp"

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

    test_add add("/home/penglei/data/GF5_AHSI_0943.tiff",
            "/home/penglei/data/temp/test_add4.img");

//    test_add add("/home/penglei/data/cup99hy.tiff",
//                 "/home/penglei/data/temp/test_add4.img");

    std::cout << ImgTool::measure<>::execution(&test_add::run, &add) << " ms\n";
    //add.run();



    return 0;
}