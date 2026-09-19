# Standalone distribution: static LLVM/Clang foundation

Tracking: [CoreTrace/coretrace#68](https://github.com/CoreTrace/coretrace/issues/68).

This first milestone adds `CORETRACE_STATIC_LLVM=ON`. It links the existing
stack analyzer and compiler with LLVM/Clang archives and audits the resulting
`ctrace` executable, including its transitive shared dependencies. The default
build and the existing release archive remain unchanged. This is **not yet a
complete standalone source distribution**: resources, other libraries, worker
isolation and clean-machine validation are separate milestones.

## Architecture and inventory

Audited integration: CoreTrace main at `1435de0`, stack analyzer v0.20.0
(`a493208373995996283b80ba17a77940d872e8ae`), compiler
`866fa76403f29e4fefda99770e04d284175f4408`.

| Component | Current integration and remaining dependency |
| --- | --- |
| `ctrace` | C++20 entry in `main.cpp`, CLI11/Getopt, CLI and HTTP server. Links `coretrace::stack_usage_analyzer_lib` already. |
| Stack analyzer | Static library, data API `parseArguments` → `runAnalysis` → `renderReport`; structured JSON/SARIF/text reports. Upstream CMake forces `LLVM_LINK_LLVM_DYLIB=ON`. No CLI entry extraction is necessary for this milestone. |
| Compiler | `cc::compilerlib_static`, Clang frontend in process. It initializes every backend configured in its LLVM SDK. Its static link branch requests the `all` LLVM pseudo-component. |
| LLVM/Clang | LLVM 20 SDK archives required for the opt-in profile. A library called `compilerlib_static` alone does not guarantee static transitive dependencies. Packaged SDKs may export static Clang targets linked to `LLVM`. |
| Z3 | Optional, auto-detected upstream; can introduce a shared Z3 dependency. `ENABLE_Z3_BACKEND=OFF` excludes it. Solver parity requires the same solver configuration in both binaries. |
| HTTP | cpp-httplib v0.14.3 auto-detects OpenSSL, zlib and Brotli. These are outside the LLVM linkage guarantee. |
| Logger / JSON | Logger is fetched from `main` (still needs a release pin); nlohmann JSON is header-only. Record resolved commits for releases, including any FetchContent source overrides. |
| Source environment | Compiler resolves Clang paths, a build-time resource directory, project SDK/includes and macOS `xcrun`. Resources are not embedded. Do not claim source compilation works on a machine without LLVM yet. |
| Models / cache | Default JSON config references model files copied by the existing configure step into `config/models`; external model paths are not embedded. An IR invocation without `--config` can use engine defaults. |
| Other tools | Tscancode, IKOS, flawfinder and cppcheck still use external tool paths/processes. They are outside this milestone. |
| Dynamic instrumentation | Compiler records runtime/logger archive paths from the build tree; program linking/execution remains an external-toolchain workflow. |
| Exit status / isolation | Analyzer currently runs in a thread in process. Bridge failures are printed but do not uniformly propagate to the public CLI exit status. Self-executable workers and reliable failures remain necessary before release. |

The adapter operates on CMake link interfaces after FetchContent, preserving
non-LLVM dependencies and leaving downloaded source/SDK files untouched. It
replaces the monolithic `LLVM` and `clang-cpp` edges with interface targets backed
by archives. It also traverses static Clang's transitive interfaces because
setting `BUILD_SHARED_LIBS=OFF` cannot alter imported targets.

The LLVM component selection names the frontend, driver, codegen and analyzer
dependencies explicitly, plus the compiler's all-backend initialization. The
`all` pseudo-component is insufficient in installed SDKs that do not export
the `LLVM_COMPONENT_LIBS` global property. This costs link time and potentially binary size;
restrict backends in the private SDK (`Native` by default). No `--whole-archive`
flag is used. A future upstream build option can replace this adapter without
changing the analyzer API or diagnostics. The artifact audit remains necessary
even if an SDK encodes link edges using generator expressions the adapter does
not rewrite.

## Try the linkage profile

Use a fresh build directory and explicit LLVM/Clang SDK package directories:

```sh
cmake -S . -B build-static -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCORETRACE_STATIC_LLVM=ON \
  -DLLVM_DIR="$LLVM_SDK/lib/cmake/llvm" \
  -DClang_DIR="$LLVM_SDK/lib/cmake/clang"
cmake --build build-static --parallel 2
ctest --test-dir build-static --output-on-failure

./build-static/ctrace --version
./build-static/ctrace --invoke ctrace_stack_analyzer --input example.ll \
  --report-file report.txt

# Install only this executable, excluding upstream compiler install rules/configs.
cmake --install build-static --prefix "$PWD/staging" --component standalone-ir
cmake -DBINARY="$PWD/staging/bin/ctrace" -P cmake/AuditLLVMDependencies.cmake
```

The post-link audit fails on dynamic LLVM/Clang or unresolved dependencies. It
prints remaining dependencies rather than certifying them as system libraries.
Configuration rejects missing imported static archives. For the apt.llvm.org
SDK, install `libpolly-20-dev` alongside `llvm-20-dev` and `libclang-20-dev`:
its exported LLVM extensions reference Polly even when its archive is not installed.
Only Linux and macOS are supported by this profile/audit. The normal build
continues to use its previous linkage. CI builds and runs the existing tests
with both profiles on Ubuntu 24.04 and macOS 14; those jobs are developer-SDK
checks, not clean-machine distribution acceptance.

For a reduced dependency experiment, configure with
`-DENABLE_Z3_BACKEND=OFF -DHTTPLIB_USE_OPENSSL_IF_AVAILABLE=OFF
-DHTTPLIB_USE_ZLIB_IF_AVAILABLE=OFF -DHTTPLIB_USE_BROTLI_IF_AVAILABLE=OFF`.
Do not request the Z3 backend in this configuration. These switches intentionally
exclude optional features and are not silently forced by the linkage profile.
An SDK built with compression or other optional dependencies can still bring
those in through LLVM archives. Audit the executable again after installation.

## Build the private reference SDK

Release reference: LLVM 20.1.8, exact commit
`87f0227cb60147a26a1eeb4fb06e3b505e9c7261` (resolved from the annotated tag
`llvmorg-20.1.8`). The current linkage profile accepts LLVM 20 patch versions for
development; that does not certify them as the release reference.

From the CoreTrace checkout, choose a separate SDK workspace:

```sh
CORETRACE_SOURCE="$PWD"
SDK_WORK="$PWD/build-sdk"
mkdir -p "$SDK_WORK"
git clone --branch llvmorg-20.1.8 --depth 1 \
  https://github.com/llvm/llvm-project.git "$SDK_WORK/llvm-project"
test "$(git -C "$SDK_WORK/llvm-project" rev-parse HEAD)" = \
  87f0227cb60147a26a1eeb4fb06e3b505e9c7261
cmake -S "$SDK_WORK/llvm-project/llvm" -B "$SDK_WORK/build" -G Ninja \
  -C "$CORETRACE_SOURCE/cmake/StandaloneLLVM20.cmake" \
  -DCMAKE_INSTALL_PREFIX="$SDK_WORK/install"
cmake --build "$SDK_WORK/build" --parallel 2
cmake --install "$SDK_WORK/build"
```

The initial cache enables RTTI/exceptions and disables optional compression,
XML, FFI, line editing and networking. Overrides are possible; save the final
cache, host compiler/version, source revisions and SDK hash as release provenance.
No SDK build or download occurs at runtime. Python used by an LLVM build is a
developer dependency only. On macOS, select the same deployment target for both
SDK and application. Linux minimum glibc and macOS minimum versions remain to
be validated; the developer CI runner versions are not release promises.

## Remaining acceptance work

- Build the pinned SDK and validate an executable on a clean Linux x86_64 host
  without LLVM; restrict remaining shared libraries to an explicit system list.
- Compare complete diagnostics on identical IR and solver/model options. Preserve
  diagnostic locations/categories; normalize only known variable fields.
- Embed all Clang resource headers and notices with a deterministic generator,
  private cache, interprocess locking, integrity checks and atomic publication.
- Add internal compiler/analyzer workers using the current executable path,
  argument arrays, explicit working directories and timeout/crash propagation.
- Verify source and compdb behavior without LLVM; distinguish missing project
  headers from packaging failures. Preserve optimization/debug settings.
- Pin every dependency, record provenance, add license access, complete the
  clean-machine distribution matrix, then update release packaging/signing.

References consulted before implementation:
[LLVM 20 CMake](https://releases.llvm.org/20.1.0/docs/CMake.html) and
[LLVM 20 distributions](https://releases.llvm.org/20.1.0/docs/BuildingADistribution.html).
