# ctrace

### BUILD

```bash
mkdir -p build && cd build
```

Depending on your configuration, you can pass these parameters to cmake. The parameters will be documented here.

```bash
cmake ..                        \
    -DPARSER_TYPE=CLI11         \
    -DUSE_THREAD_SANITIZER=ON   \
    -DUSE_ADDRESS_SANITIZER=OFF
```

```bash
make -j4
```

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

```bash
./ctrace [-h|--help]
ctrace - Static & Dynamic C/C++ Code Analysis Tool

Usage:
  ctrace [options]

Options:
  --help                   Displays this help message.
  --verbose                Enables detailed (verbose) output.
  --sarif-format           Generates a report in SARIF format.
  --report-file <path>     Specifies the path to the report file (default: ctrace-report.txt).
  --output-file <path>     Specifies the output file for the analysed binary (default: ctrace.out).
  --entry-points <names>   Sets the entry points for analysis (default: main). Accepts a comma-separated list.
  --static                 Enables static analysis.
  --dyn                    Enables dynamic analysis.
  --invoke <tools>         Invokes specific tools (comma-separated).
                           Available tools: flawfinder, ikos, cppcheck, tscancode.
  --input <files>          Specifies the source files to analyse (comma-separated).

Examples:
  ctrace --input main.cpp,util.cpp --static --invoke=cppcheck,flawfinder
  ctrace --verbose --report-file=analysis.txt --sarif-format

Description:
  ctrace is a modular C/C++ code analysis tool that combines both static and dynamic
  analysis. It can be finely configured to detect vulnerabilities, security issues,
  and memory misuse.
```

```bash
./ctrace --input ../tests/EmptyForStatement.cc --entry-points=main --verbose --static --dyn
```

### Mangle/Demangle API

```c++
bool hasMangled = ctrace_tools::isMangled(entry_points);

std::cout << "Is mangled : " << hasMangled << std::endl;
if (hasMangled)
    std::cout << abi::__cxa_demangle(entry_points.c_str(), 0, 0, &status) << std::endl;

std::vector<std::string> params1 = {};
std::string mangled1 = ctrace_tools::mangleFunction("", "single_compute()", params1);
std::cout << "Mangled single_compute(): " << mangled1 << "\n";
std::cout << abi::__cxa_demangle(mangled1.c_str(), 0, 0, &status) << std::endl;

// Example 2 : with namespace
std::vector<std::string> params2 = {"std::string", "int"};
std::string mangled2 = ctrace_tools::mangleFunction("math", "compute", params2);
std::cout << "Mangled math::compute(std::string, int): " << mangled2 << "\n";

// Example 3 : without parameters with namespace
std::vector<std::string> params3;
std::string mangled3 = ctrace_tools::mangleFunction("utils", "init", params3);
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
