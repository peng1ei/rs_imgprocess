//
// Created by penglei on 18-10-16.
//

#ifndef IMGPROCESS_MG_STRIPEREMOVE_H
#define IMGPROCESS_MG_STRIPEREMOVE_H

#include "mg_datasetmanager.h"
#include "mg_progress.hpp"
#include <string>

namespace Mg {

    class MgStripeRemove {
    public:
        explicit MgStripeRemove(const std::string &fileIn,
                                const std::string &fileOut,
                                const std::string &format,
                                int n = 5);

        bool run();

    private:
        template <typename OutScalar>
        bool commonProcess();

    private:
        std::string fileIn_;
        std::string fileOut_;
        std::string format_;

        MgDatasetManagerPtr mgDatasetInPtr_;
        MgDatasetManagerPtr mgDatasetOutPtr_;

        int n_;
    };

} // namespace Mg



#endif //IMGPROCESS_MG_STRIPEREMOVE_H
