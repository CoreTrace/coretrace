#include "ArgumentParser/ArgumentParserFactory.hpp"
#include "ArgumentParser/CLI11/CLI11ArgumentParser.hpp"
#include "ArgumentParser/GetOpt/GetOptArgumentParser.hpp"

std::unique_ptr<IArgumentParser> createArgumentParser()
{
#ifdef USE_GETOPT
    return std::make_unique<GetoptArgumentParser>();
#else
    return std::make_unique<CLI11ArgumentParser>();
#endif
}
