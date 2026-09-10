// SPDX-License-Identifier: Apache-2.0
#include "IAnalysisTools.hpp"
#include "../Ipc/IpcStrategy.hpp"

namespace ctrace
{

    class AnalysisToolBase : public IAnalysisTool
    {
      protected:
        std::shared_ptr<IpcStrategy> ipc;

        /// Records how the tool finished. Tools that do not count their own
        /// diagnostics still have to say whether they ran, or a failure and a
        /// clean file are indistinguishable downstream.
        ///
        /// A non-zero exit is a failure even though the process started: a
        /// launcher that reports "no module named flawfinder" starts perfectly
        /// well and analyses nothing.
        void markFinished(int exitCode) const
        {
            m_outcome = exitCode == 0 ? ToolOutcome::Ran : ToolOutcome::Failed;
        }

        void markNotRun() const
        {
            m_outcome = ToolOutcome::NotRun;
        }

      public:
        void setIpcStrategy(std::shared_ptr<IpcStrategy> strategy) override
        {
            ipc = std::move(strategy);
        }

        [[nodiscard]] DiagnosticSummary lastDiagnosticsSummary() const override
        {
            DiagnosticSummary summary;
            summary.outcome = m_outcome;
            return summary;
        }

      private:
        mutable ToolOutcome m_outcome = ToolOutcome::NotRun;
    };

} // namespace ctrace
