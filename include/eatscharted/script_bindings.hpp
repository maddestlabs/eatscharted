#ifndef EATSCHARTED_SCRIPT_BINDINGS_HPP
#define EATSCHARTED_SCRIPT_BINDINGS_HPP

#include "eatscharted/chart_world.hpp"
#include "eatsbits/eatscript/evaluator.hpp"
#include <string>
#include <vector>

namespace eatscharted {

/**
 * Binds Eatscharted's declarative Voronoi Cartography, InkCanvas Standees,
 * Curved Map Banners, and Cat Quest Hero API to Eatscript (.eats).
 */
class ChartScriptEngine {
public:
    explicit ChartScriptEngine(ChartWorld& world);

    bool executeScript(const std::string& sourceCode);
    bool executeFile(const std::string& filePath);

    [[nodiscard]] const std::string& lastError() const noexcept { return lastError_; }
    [[nodiscard]] const std::vector<std::string>& logs() const noexcept { return evaluator_.getLogs(); }

private:
    void registerChartBindings();

    ChartWorld& world_;
    eatsbits::eatscript::Evaluator evaluator_;
    std::string lastError_;
};

} // namespace eatscharted

#endif // EATSCHARTED_SCRIPT_BINDINGS_HPP
