# ctrace

### BUILD

```bash
mkdir -p build && cd build
```

Depending on your configuration, you can pass these parameters to cmake. The parameters will be documented here.

```bash
cmake ..                        \
    -DUSE_THREAD_SANITIZER=ON   \
    -DUSE_ADDRESS_SANITIZER=OFF
```

```bash
make -j4
```

### CODE STYLE (clang-format)

- Version cible : `clang-format` 17 (utilisée dans la CI).
- Formater localement : `./scripts/format.sh`
- Vérifier sans modifier : `./scripts/format-check.sh`
- CMake : `cmake --build build --target format` ou `--target format-check`
- CI : le job GitHub Actions `clang-format` échoue si un fichier n’est pas formaté.

### DEBUG

You can pass arguments to CMake and invoke ASan.

```bash
-DUSE_THREAD_SANITIZER=ON
```
or
```bash
-DUSE_ADDRESS_SANITIZER=ON
```

> ⚠️ **Warning**: You cannot use `-DUSE_THREAD_SANITIZER=ON` and `-DUSE_ADDRESS_SANITIZER=ON` at the same time.

### ARGUMENT

The option list is generated from the binary and is the single source of truth:

```bash
./ctrace --help
```

### VERSION

```bash
./ctrace --version
```

Version resolution is centralized at configure time:

- release builds pass an explicit version string from CI, so published binaries print the exact GitHub release tag
- local Git builds resolve the version from `git describe --tags --dirty --always --match 'v*'`
- if Git metadata is unavailable, the build falls back to `dev`

You can also override the embedded version manually:

```bash
cmake .. -DCORETRACE_VERSION_OVERRIDE=v0.74.0
```

### CONFIGURATION

- Canonical default config: `config/tool-config.json`
- Full schema and semantics: `docs/configuration.md`
- Precedence: built-in defaults < config file < CLI

```bash
./ctrace --input ../tests/EmptyForStatement.cc --entry-points=main --verbose --static --dyn
```

### SERVER MODE

Start the HTTP server:

```bash
./ctrace --ipc serve --serve-host 127.0.0.1 --serve-port 8080 --shutdown-token mytoken
```

Send a request:

```bash
curl -X POST http://127.0.0.1:8080/api \
  -H "Content-Type: application/json" \
  -d '{
    "proto": "coretrace-1.0",
    "id": 1,
    "type": "request",
    "method": "run_analysis",
    "params": {
      "input": ["./tests/buffer_overflow.cc"],
      "entry_points": ["main"],
      "static_analysis": true,
      "dynamic_analysis": false,
      "invoke": ["flawfinder"],
      "sarif_format": true,
      "report_file": "ctrace-report.txt",
      "output_file": "ctrace.out",
      "ipc": "serve",
      "ipc_path": "/tmp/coretrace_ipc",
      "async": false,
      "verbose": true
    }
  }'
```

Response notes:
- `status` is `ok` or `error`.
- `result.diagnostics` lists every finding in one model (`tool`, `rule_id`, `file`, `line`, `column`, `severity`, `message`, `cwe`), whatever tool produced it; `result.diagnostics_summary_total` counts them by severity.
- `result.uninterpreted_tools` names the tools whose output CoreTrace could not interpret: their findings are shown in `result.outputs` but are not counted.
- `result.outputs` groups tool output by tool name.
- Each output entry has `stream` and `message`. If a tool emits JSON, `message` is returned as a JSON object.

Shutdown the server (HTTP request):

```bash
curl -i -X POST http://127.0.0.1:8080/shutdown \
  -H "Authorization: Bearer mytoken"
```

Alternative header:

```bash
curl -i -X POST http://127.0.0.1:8080/shutdown \
  -H "X-Admin-Token: mytoken"
```

The server responds with `202 Accepted` and stops accepting new requests while allowing in-flight requests to finish
(up to `--shutdown-timeout-ms` if configured).

### Mangle/Demangle API

```c++
bool hasMangled = ctrace_tools::mangle::isMangled(entry_points);

std::cout << "Is mangled : " << hasMangled << std::endl;
if (hasMangled)
    std::cout << abi::__cxa_demangle(entry_points.c_str(), 0, 0, &status) << std::endl;

std::vector<std::string> params1 = {};
std::string mangled1 = ctrace_tools::mangle::mangleFunction("", "single_compute()", params1);
std::cout << "Mangled single_compute(): " << mangled1 << "\n";
std::cout << abi::__cxa_demangle(mangled1.c_str(), 0, 0, &status) << std::endl;

// Example 2 : with namespace
std::vector<std::string> params2 = {"std::string", "int"};
std::string mangled2 = ctrace_tools::mangle::mangleFunction("math", "compute", params2);
std::cout << "Mangled math::compute(std::string, int): " << mangled2 << "\n";

// Example 3 : without parameters with namespace
std::vector<std::string> params3;
std::string mangled3 = ctrace_tools::mangle::mangleFunction("utils", "init", params3);
std::cout << "Mangled utils::init(): " << mangled3 << "\n";

```

## TODO

```
- Mangle function with parameters
- Thread execution : datarace condition detected
- 'Format log' function needs to be implement
- Handle multi-file parsing
- Add mangling for windows
- Add lvl verbosity to --verbose like : --verbose=[1|2|3|4]
- sanitazier explication ...
- passer du code au lieu du fichier
```
