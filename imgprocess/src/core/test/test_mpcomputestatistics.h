//
// Created by penglei on 18-9-11.
//

#ifndef IMGPROCESS_TEST_MPCOMPUTESTATISTICS_H
#define IMGPROCESS_TEST_MPCOMPUTESTATISTICS_H

#include <string>
#include "gdal_priv.h"

class Test_MpComputeStatistics {
public:
    Test_MpComputeStatistics(const std::string &file)
        : file_(file){
    }

    bool run();



private:
    std::string file_;
};


#endif //IMGPROCESS_TEST_MPCOMPUTESTATISTICS_H
