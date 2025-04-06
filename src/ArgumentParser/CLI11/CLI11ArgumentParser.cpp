#include "ArgumentParser/BaseArgumentParser.hpp"
#include "ArgumentParser/OptionInfo.hpp"
#include "../../../external/CLI11.hpp"
#include <vector>

class CLI11ArgumentParser : public BaseArgumentParser {
private:
    CLI::App app{"CLI11 Argument Parser"};
    std::vector<OptionInfo> _options;
    std::vector<std::pair<std::string, bool*>> _flags;
    std::vector<std::pair<std::string, std::string*>> _values;

    bool optionExists(const std::string& name, char shortName) const {
        for (const auto& opt : _options) {
            if (opt.name == name || (shortName != '\0' && opt.shortName == shortName)) {
                return true;
            }
        }
        return false;
    }

    static std::string stripDashes(const std::string& name) {
        std::string stripped = name;
        while (!stripped.empty() && stripped[0] == '-') {
            stripped.erase(0, 1);
        }
        return stripped;
    }

public:
    CLI11ArgumentParser()
    {
        // std::cout << "CLI11 is called" << std::endl;
        app.set_help_flag("");
    }

    void addOption(const std::string& name, bool hasArgument, char shortName) override {
        if (optionExists(name, shortName)) {
            setError(ErrorCode::INVALID_OPTION, "Option already exists: " + name);
            return;
        }

        OptionInfo opt;
        opt.name = name;
        opt.shortName = shortName;
        opt.hasArgument = hasArgument;
        opt.isSet = false;

        std::string longOpt = stripDashes(name);
        std::string shortOpt = shortName != '\0' ? std::string(1, shortName) : "";

        if (hasArgument) {
            auto* value = new std::string();
            _values.emplace_back(longOpt, value);
            auto* cliOption = app.add_option("-" + shortOpt + "," + name, *value, "Option with argument");
            if (!cliOption) {
                setError(ErrorCode::INVALID_OPTION, "Failed to add option: " + name);
            }
        } else {
            auto* flag = new bool(false);
            _flags.emplace_back(longOpt, flag);
            auto* cliFlag = app.add_flag("-" + shortOpt + "," + name, *flag, "Flag option");
            if (!cliFlag) {
                setError(ErrorCode::INVALID_OPTION, "Failed to add flag: " + name);
            }
        }

        _options.push_back(opt);
    }

    void addFlag(const std::string& name, char shortName) override {
        addOption(name, false, shortName);
    }

    bool parse(int argc, char* argv[]) override {
        setError(ErrorCode::SUCCESS);

        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            if (e.get_exit_code() != 0) {
                setError(ErrorCode::INVALID_OPTION, e.what());
                return false;
            }
        }

        for (auto& opt : _options) {
            std::string longOpt = stripDashes(opt.name);
            if (opt.hasArgument) {
                for (const auto& [name, value] : _values) {
                    if (name == longOpt && !value->empty()) {
                        opt.isSet = true;
                        opt.value = *value;
                        break;
                    }
                }
            } else {
                for (const auto& [name, flag] : _flags) {
                    if (name == longOpt && *flag) {
                        opt.isSet = true;
                        break;
                    }
                }
            }
        }

        return true;
    }

    bool hasOption(const std::string& name) const override {
        for (const auto& opt : _options) {
            if (opt.name == name || (opt.shortName != '\0' && std::string(1, opt.shortName) == name)) {
                return opt.isSet;
            }
        }
        return false;
    }

    std::string getOptionValue(const std::string& name) const override {
        for (const auto& opt : _options) {
            if (opt.name == name || (opt.shortName != '\0' && std::string(1, opt.shortName) == name)) {
                return opt.value;
            }
        }
        return "";
    }

    ~CLI11ArgumentParser() override {
        for (auto& [_, flag] : _flags) {
            delete flag;
        }
        for (auto& [_, value] : _values) {
            delete value;
        }
    }
};