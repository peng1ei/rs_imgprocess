#include <iostream>
#include "gdal_priv.h"
#include "test_add/test_add.h"

int main() {
    //GDALAllRegister();
    int size = sizeof(ImgTool::ImgSpectralSubset);
    std::cout << size << std::endl;

    test_add add("/home/penglei/data/cup95eff.img",
            "/home/penglei/data/temp/test_add2.img");

    add.run();



    return 0;
}