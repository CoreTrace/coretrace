// SPDX-License-Identifier: Apache-2.0
#ifndef ANALYSIS_TOOLS_BASE_HPP
#define ANALYSIS_TOOLS_BASE_HPP

#include "IAnalysisTools.hpp"
#include "../Ipc/IpcStrategy.hpp"

namespace ctrace
{

    class AnalysisToolBase : public IAnalysisTool
    {
      protected:
        std::shared_ptr<IpcStrategy> ipc;

      public:
        void setIpcStrategy(std::shared_ptr<IpcStrategy> strategy) override
        {
            ipc = std::move(strategy);
        }
    };

} // namespace ctrace

#endif // ANALYSIS_TOOLS_BASE_HPP
