// SPDX-License-Identifier: Apache-2.0
#include "ArgumentParser/ArgumentParserFactory.hpp"

#if !defined(USE_GETOPT)
#include "ArgumentParser/CLI11/CLI11ArgumentParser.hpp"
#endif

#if defined(USE_GETOPT)
#include "ArgumentParser/GetOpt/GetOptArgumentParser.hpp"
#endif

std::unique_ptr<IArgumentParser> createArgumentParser()
{
#ifdef USE_GETOPT
    return std::make_unique<GetoptArgumentParser>();
#else
    return std::make_unique<CLI11ArgumentParser>();
#endif
}
