# TODO

## Stack Analyzer Summary Debt

- [ ] Expose a structured diagnostics summary in `ctrace::stack::app::RunResult` from `coretrace-stack-analyzer`.
  - Add `info`, `warning`, and `error` counters in the tool result contract.
  - Compute the summary once from final filtered diagnostics inside the analyzer core.
  - Keep output strategies (`human`, `json`, `sarif`) focused on serialization only.
  - Make `coretrace` consume `RunResult.summary` as the primary source.
  - Remove text/JSON/SARIF parsing fallback in `StackAnalyzerToolImplementation.cpp` after migration.
  - Add compatibility notes and tests for mixed versions during transition.

## Interprocedural Ownership Path Analysis Debt

- [ ] Implement interprocedural ownership tracking that follows each abstract object/pointer through control-flow paths up to release/destructor points.
  - Build an interprocedural CFG (call/return edges) and run a forward dataflow typestate analysis.
  - Track per-object states (`Owned`, `Transferred`, `Released`, `Escaped`, `Unknown`) keyed by allocation/wrapper origins.
  - Introduce transfer semantics (`transfer_arg` / adopt ownership) to model delayed-release patterns (GC/wrapper handoff).
  - Use function summaries to scale cross-TU propagation while preserving precision on ownership effects.
  - Report `MissingRelease`, `DoubleRelease`, `UseAfterRelease`, and `ReleasedHandleEscapes` from path-feasible states.
  - Add regression tests for wrapper allocators, GC registration APIs, destructor-backed cleanup, and mixed modeled/unmodeled calls.
