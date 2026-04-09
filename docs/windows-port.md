# Windows Port Plan

This project can be ported to Windows without rewriting the application, but the port needs to be treated as two separate deliverables:

1. A native host application: `ctrace.exe`
2. A tool runtime strategy for every analyzer that `ctrace` orchestrates

The host application is a good fit for native Windows. The analyzer toolchain is mixed: some tools have native Windows stories, some do not. For feature parity, keep the host native and make tool execution configurable per tool.

## Target Architecture

Use a layered design:

1. Core host
   Native C++/CMake application with the same CLI, config loading, HTTP server mode, and report generation on every OS.
2. Platform adapters
   Isolate process launching, local socket IPC, temp-path defaults, and packaging behind Windows-safe abstractions.
3. Tool adapters
   Resolve each analyzer through a dedicated launcher instead of hard-coded Unix paths.
4. Packaging
   Build an install tree containing `bin/ctrace.exe`, required runtime DLLs, and `config/`.

The current tree now includes:

- A real Windows process launcher in `include/Process/WinProcess.hpp`
- Cross-platform local socket IPC in `include/Process/Ipc/IpcStrategy.hpp`
- Platform-aware IPC defaults in `include/App/Platform.hpp`
- Tool command resolution with environment overrides in `include/App/ToolResolver.hpp`
- A Windows build helper in `scripts/build-windows.ps1`

## Build Requirements

Install:

- Visual Studio 2022 with C++ build tools
- CMake 3.28+
- LLVM/Clang development packages with `LLVMConfig.cmake` and `ClangConfig.cmake`

Recommended configure path:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1 `
  -LLVMDir "C:\Program Files\LLVM\lib\cmake\llvm" `
  -PackageZip
```

Expected output:

- `dist\windows\bin\ctrace.exe`
- `dist\windows\config\tool-config.json`
- `coretrace-windows-Release.zip` when `-PackageZip` is used

## Tool Runtime Strategy

Do not keep analyzer paths hard-coded in source for Windows deployments. Use environment overrides instead:

- `CORETRACE_CPPCHECK_BIN`
- `CORETRACE_TSCANCODE_BIN`
- `CORETRACE_IKOS_BIN`
- `CORETRACE_FLAWFINDER_LAUNCHER`
- `CORETRACE_FLAWFINDER_SCRIPT`
- `CORETRACE_PYTHON_BIN`

Recommended approach:

- Run `cppcheck` natively on Windows
- Run `ctrace_stack_analyzer` natively on Windows with LLVM/Clang
- Run `tscancode` natively if you have a Windows-capable binary available
- Run `ikos` and `flawfinder` through a compatibility layer or wrapper if native Windows binaries are not part of your supported toolchain

For production, prefer wrapper executables or scripts per analyzer instead of embedding platform rules directly in `ctrace`. That keeps the host binary stable and lets you swap tool installations without recompiling.

## Packaging Guidance

For an industry-ready Windows release:

1. Build `ctrace.exe` with MSVC in `Release`
2. Install with `cmake --install` into a staging directory
3. Bundle required runtime DLLs next to the executable
4. Keep mutable assets in `config/`
5. Publish a portable ZIP first
6. Add an installer only after the portable artifact is stable

If you need a Windows installer later, add a separate packaging layer such as CPack/NSIS or WiX, but keep the portable install tree as the canonical artifact.

## Important Constraint

A native `ctrace.exe` is straightforward. Full native parity for every external analyzer depends on whether each analyzer itself is supported on Windows. If one analyzer is Linux-only, the correct architecture is a native Windows host plus a compatibility-backed adapter for that analyzer, not a partial reimplementation inside `ctrace`.
