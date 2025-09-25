#include "ArgumentParser/ArgumentManager.hpp"
#include <iostream>

ArgumentManager::ArgumentManager(std::unique_ptr<IArgumentParser> parser)
    : _parser(std::move(parser)) {}

void ArgumentManager::addOption(const std::string& name, bool hasArgument, char shortName) {
    _parser->addOption(name, hasArgument, shortName);
}

void ArgumentManager::addFlag(const std::string& name, char shortName) {
    _parser->addFlag(name, shortName);
}

void ArgumentManager::parse(int argc, char* argv[]) {
    if (!_parser->parse(argc, argv))
    {
        std::cerr << "Error: " << _parser->getErrorMessage() << std::endl;
        std::exit(static_cast<int>(_parser->getLastError()));
    }
    if (argc <= 1)
    {
        _hasNoOption = true;
    }
}

bool ArgumentManager::hasOption(const std::string& name) const {
    return _parser->hasOption(name);
}

bool ArgumentManager::hasNoOption() const
{
    return _hasNoOption;
}

std::string ArgumentManager::getOptionValue(const std::string& name) const {
    return _parser->getOptionValue(name);
}
