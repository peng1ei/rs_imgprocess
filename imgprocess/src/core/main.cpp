#include <iostream>
#include "gdal_priv.h"
#include "test_add/test_add.h"
#include "imgtool_imgblockprocess.hpp"

int main() {
    int size = sizeof(ImgTool::ImgSpectralSubset);
    std::cout << "ImgSpectralSubset Size: " << size << std::endl;

    size = sizeof(ImgTool::ImgSpatialSubset);
    std::cout << "ImgSpatialSubset Size: " << size << std::endl;

    size = sizeof(ImgTool::ImgBlockEnvelope);
    std::cout << "ImgBlockEnvelope Size: " << size << std::endl;

    size = sizeof(ImgTool::ImgBlockData<double>);
    std::cout << "ImgBlockData Size: " << size << std::endl;

    size = sizeof(ImgTool::ImgBlockDataRead<double>);
    std::cout << "ImgBlockDataRead Size: " << size << std::endl;

    size = sizeof(ImgTool::ImgBlockProcess<double>);
    std::cout << "ImgBlockProcess Size: " << size << std::endl;

    test_add add("/home/penglei/data/cup95eff.img",
            "/home/penglei/data/temp/test_add2.img");

    add.run();



    return 0;
}