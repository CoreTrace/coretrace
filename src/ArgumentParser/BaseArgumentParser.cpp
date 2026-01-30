#include "ArgumentParser/BaseArgumentParser.hpp"
#include <cstdio>

BaseArgumentParser::BaseArgumentParser() : _lastError(ErrorCode::SUCCESS), _errorMessage("") {}

void BaseArgumentParser::setError(ErrorCode code, const std::string& message)
{
    _lastError = code;
    _errorMessage = message;
}

ErrorCode BaseArgumentParser::getLastError() const
{
    return _lastError;
}

std::string BaseArgumentParser::getErrorMessage() const
{
    return _errorMessage;
}
