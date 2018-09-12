//
// Created by penglei on 18-9-12.
//

#ifndef IMGPROCESS_IA_ANOMALYDETECTION_H
#define IMGPROCESS_IA_ANOMALYDETECTION_H

#include "imgtool_progress.hpp"
#include "imgtool_error.h"
#include <string>


class GDALDataset;

namespace ImgAlgo {

    enum RXType {
        RXD,
        UTD,
        RXD_UTD
    };

    class RXAnomalyDetection : public ImgTool::ProgressFunctor,
            public ImgTool::ErrorBase {
    public:
        RXAnomalyDetection(const std::string &inFile,
                const std::string &outFile,
                const std::string &outFormat,
                RXType rxtType = RXD);

        ~RXAnomalyDetection() {
            delete[](pMean_);
            delete[](pCovariance_);
        }

        bool run();

    private:
        bool init();

        template <typename T>
        bool runCore();



    private:
        std::string inFile_;
        std::string outFile_;
        std::string outFileFormat_;

        GDALDataset *poInDS_;
        GDALDataset *poOutDS_;

        int imgXSize_;
        int imgYSize_;
        int imgBandCount_;

        double *pMean_;
        double *pCovariance_;

        RXType rxdType_;
    };

}

#endif //IMGPROCESS_IA_ANOMALYDETECTION_H
