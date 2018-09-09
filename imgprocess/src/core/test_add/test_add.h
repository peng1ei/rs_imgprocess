//
// Created by penglei on 18-9-9.
//

#ifndef IMGPROCESS_TEST_ADD_H
#define IMGPROCESS_TEST_ADD_H

#include <string>
#include "../imgtools/imgtool_common.hpp"

class test_add {
public:
    test_add(const std::string &strInFile,
            const std::string &strOutFile);

    bool run();

private:
    std::string m_strInFile;
    std::string m_strOutFile;

    GDALDataset *poSrcDS;
    GDALDataset *poDstDS;
};


#endif //IMGPROCESS_TEST_ADD_H
