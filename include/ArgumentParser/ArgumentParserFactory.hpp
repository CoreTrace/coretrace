#ifndef ARGUMENTPARSERFACTORY_HPP
#define ARGUMENTPARSERFACTORY_HPP

#include "ArgumentParser/IArgumentParser.hpp"
#include <memory>

/**
 * @brief Factory function to create an argument parser.
 *
 * The `createArgumentParser` function is responsible for creating and returning
 * an instance of a class that implements the `IArgumentParser` interface.
 *
 * @return A `std::unique_ptr<IArgumentParser>` pointing to the created argument parser.
 *
 * @note The specific implementation of the argument parser is determined by the
 *       factory function and may vary depending on the platform or configuration.
 */
std::unique_ptr<IArgumentParser> createArgumentParser();

#endif // ARGUMENTPARSERFACTORY_HPP
