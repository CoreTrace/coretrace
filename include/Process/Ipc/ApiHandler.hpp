// SPDX-License-Identifier: Apache-2.0
#ifndef PROCESS_IPC_API_HANDLER_HPP
#define PROCESS_IPC_API_HANDLER_HPP

#include "Config/config.hpp"

#include <coretrace/logger.hpp>
#include <nlohmann/json.hpp>

#include <string>

class ILogger
{
  public:
    virtual ~ILogger() = default;
    virtual void info(const std::string& msg) = 0;
    virtual void error(const std::string& msg) = 0;
};

class ConsoleLogger : public ILogger
{
  public:
    void info(const std::string& msg) override
    {
        coretrace::log(coretrace::Level::Info, "{}\n", msg);
    }

    void error(const std::string& msg) override
    {
        coretrace::log(coretrace::Level::Error, "{}\n", msg);
    }

    void debug(const std::string& msg)
    {
        coretrace::log(coretrace::Level::Debug, "{}\n", msg);
    }

    void warn(const std::string& msg)
    {
        coretrace::log(coretrace::Level::Warn, "{}\n", msg);
    }
};

/// The coretrace-1.0 protocol: a JSON request document in, a JSON response document out.
/// Knows nothing about HTTP; HttpServer carries it over the wire.
class ApiHandler
{
  public:
    struct ParseError
    {
        std::string code;
        std::string message;
    };

    explicit ApiHandler(ILogger& logger) : logger_(logger) {}

    [[nodiscard]] nlohmann::json handle_request(const nlohmann::json& request);

    /// Translates the flat run_analysis parameters into the configuration through the config
    /// loader, so a request gets the same validation as a config file.
    [[nodiscard]] static bool build_config_from_params(const nlohmann::json& params,
                                                       ctrace::ProgramConfig& config,
                                                       ParseError& err);

  private:
    [[nodiscard]] nlohmann::json handle_run_analysis(nlohmann::json& baseResponse,
                                                     const nlohmann::json& params);

    ILogger& logger_;
};

#endif // PROCESS_IPC_API_HANDLER_HPP
