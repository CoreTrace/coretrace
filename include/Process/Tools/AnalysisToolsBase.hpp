#include "IAnalysisTools.hpp"

namespace ctrace
{

    class AnalysisToolBase : public IAnalysisTool
    {
        protected:
        std::shared_ptr<IpcStrategy> ipc;
        public:
        void setIpcStrategy(std::shared_ptr<IpcStrategy> strategy) override {
            ipc = std::move(strategy);
        }
    };

}