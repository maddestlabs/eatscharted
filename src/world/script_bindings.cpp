#include "eatscharted/script_bindings.hpp"
#include "eatscharted/procedural_names.hpp"
#include "eatsbits/eatscript/stdlib.hpp"
#include "eatsbits/eatscript/lexer.hpp"
#include "eatsbits/eatscript/parser.hpp"
#include <fstream>
#include <sstream>

namespace eatscharted {

using eatsbits::eatscript::Value;
using eatsbits::eatscript::resolveArg;

ChartScriptEngine::ChartScriptEngine(ChartWorld& world)
    : world_(world) {
    eatsbits::eatscript::registerStandardLibrary(evaluator_, nullptr);
    registerChartBindings();
}

void ChartScriptEngine::registerChartBindings() {
    std::map<std::string, Value> inkDict;
    inkDict["MountainPeak"]   = Value(static_cast<int>(InkLayer::StandeeMountainPeak));
    inkDict["MountainRidge"]  = Value(static_cast<int>(InkLayer::StandeeMountainRidge));
    inkDict["RockPillar"]     = Value(static_cast<int>(InkLayer::StandeeRockPillar));
    inkDict["RockOutcrop"]    = Value(static_cast<int>(InkLayer::StandeeRockOutcrop));
    inkDict["SeaStack"]       = Value(static_cast<int>(InkLayer::StandeeSeaStack));
    inkDict["ForestOak"]      = Value(static_cast<int>(InkLayer::StandeeForestOak));
    inkDict["ForestPine"]     = Value(static_cast<int>(InkLayer::StandeeForestPine));
    inkDict["ForestPalm"]     = Value(static_cast<int>(InkLayer::StandeeForestPalm));
    inkDict["GrassPatch"]     = Value(static_cast<int>(InkLayer::StandeeGrassPatch));
    inkDict["MarshReeds"]     = Value(static_cast<int>(InkLayer::StandeeMarshReeds));
    inkDict["TownKeep"]       = Value(static_cast<int>(InkLayer::StandeeTownKeep));
    inkDict["HamletCottage"]  = Value(static_cast<int>(InkLayer::StandeeHamletCottage));
    inkDict["Windmill"]       = Value(static_cast<int>(InkLayer::StandeeWindmill));
    inkDict["Lighthouse"]     = Value(static_cast<int>(InkLayer::StandeeLighthouse));
    inkDict["Monastery"]      = Value(static_cast<int>(InkLayer::StandeeMonastery));
    inkDict["PortHarbor"]     = Value(static_cast<int>(InkLayer::StandeePortHarbor));
    inkDict["DungeonArch"]    = Value(static_cast<int>(InkLayer::StandeeDungeonArch));
    inkDict["SunkenRuin"]     = Value(static_cast<int>(InkLayer::StandeeSunkenRuin));
    inkDict["StandingStone"]  = Value(static_cast<int>(InkLayer::StandeeStandingStone));
    inkDict["TreasureChest"]  = Value(static_cast<int>(InkLayer::StandeeTreasureChest));
    inkDict["Campfire"]       = Value(static_cast<int>(InkLayer::StandeeCampfire));
    inkDict["HeroCat"]        = Value(static_cast<int>(InkLayer::StandeeHeroCat));
    inkDict["ShipCaravel"]    = Value(static_cast<int>(InkLayer::StandeeShipCaravel));
    inkDict["MonsterSlime"]   = Value(static_cast<int>(InkLayer::StandeeMonsterSlime));
    inkDict["MonsterBandit"]  = Value(static_cast<int>(InkLayer::StandeeMonsterBandit));
    inkDict["MonsterWyvern"]  = Value(static_cast<int>(InkLayer::StandeeMonsterWyvern));
    evaluator_.setVariable("Ink", Value(inkDict));

    std::map<std::string, Value> chartObj;
    chartObj["name"] = Value("Eatscharted25DCore");
    evaluator_.setVariable("chart", Value(chartObj));

    evaluator_.registerMemberFunction("chart", "clear", [this](const auto&, const auto&) -> Value {
        world_.clear();
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "build_perilous_peninsula", [this](const auto& args, const auto& kwargs) -> Value {
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 0, "seed", Value(1492)).asInt());
        world_.buildPerilousPeninsula(seed);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "build_shattered_archipelago", [this](const auto& args, const auto& kwargs) -> Value {
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 0, "seed", Value(1715)).asInt());
        world_.buildShatteredArchipelago(seed);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "build_highland_kingdom", [this](const auto& args, const auto& kwargs) -> Value {
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 0, "seed", Value(1337)).asInt());
        world_.buildHighlandKingdom(seed);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "build_one_page_dungeon", [this](const auto& args, const auto& kwargs) -> Value {
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 0, "seed", Value(999)).asInt());
        world_.buildOnePageDungeon(seed);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "add_standee", [this](const auto& args, const auto& kwargs) -> Value {
        float x = resolveArg(args, kwargs, 0, "x", Value(0.0)).asFloat();
        float y = resolveArg(args, kwargs, 1, "y", Value(0.5)).asFloat();
        float z = resolveArg(args, kwargs, 2, "z", Value(0.0)).asFloat();
        float w = resolveArg(args, kwargs, 3, "w", Value(2.8)).asFloat();
        float h = resolveArg(args, kwargs, 4, "h", Value(3.0)).asFloat();
        uint16_t layer = static_cast<uint16_t>(resolveArg(args, kwargs, 5, "layer", Value(static_cast<int>(InkLayer::StandeeTownKeep))).asInt());
        world_.addCustomStandee({x, y, z}, w, h, layer, StandeeFlags::PopUpTilt);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "add_banner", [this](const auto& args, const auto& kwargs) -> Value {
        std::string text = resolveArg(args, kwargs, 0, "text", Value("UNCHARTED REACH")).asString();
        float x0 = resolveArg(args, kwargs, 1, "x0", Value(-16.0)).asFloat();
        float z0 = resolveArg(args, kwargs, 2, "z0", Value(0.0)).asFloat();
        float x1 = resolveArg(args, kwargs, 3, "x1", Value(0.0)).asFloat();
        float z1 = resolveArg(args, kwargs, 4, "z1", Value(4.0)).asFloat();
        float x2 = resolveArg(args, kwargs, 5, "x2", Value(16.0)).asFloat();
        float z2 = resolveArg(args, kwargs, 6, "z2", Value(0.0)).asFloat();
        float sz = resolveArg(args, kwargs, 7, "size", Value(2.2)).asFloat();
        world_.addCustomBanner(text, {x0, z0}, {x1, z1}, {x2, z2}, sz);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "add_curved_banner", [this](const auto& args, const auto& kwargs) -> Value {
        std::string text = resolveArg(args, kwargs, 0, "text", Value("Duskwoods")).asString();
        float cx = resolveArg(args, kwargs, 1, "cx", Value(0.0)).asFloat();
        float cz = resolveArg(args, kwargs, 2, "cz", Value(0.0)).asFloat();
        float span = resolveArg(args, kwargs, 3, "span", Value(28.0)).asFloat();
        float angleDeg = resolveArg(args, kwargs, 4, "angle", Value(0.0)).asFloat();
        float curvature = resolveArg(args, kwargs, 5, "curve", Value(0.18)).asFloat();
        float sz = resolveArg(args, kwargs, 6, "size", Value(2.5)).asFloat();
        bool track = resolveArg(args, kwargs, 7, "track", Value(true)).asBoolean();

        std::string bannerText = track ? ProceduralNameGenerator::formatCatQuestTracking(text) : text;
        float rad = angleDeg * (3.14159265f / 180.0f);
        CurvedMapBanner b = ProceduralNameGenerator::createCurvedBanner(bannerText, {cx, cz}, span, rad, curvature, sz, true, false);
        world_.addCustomBanner(b.text, b.p0, b.p1, b.p2, b.letterSize);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "generate_name", [](const auto& args, const auto& kwargs) -> Value {
        int seed = resolveArg(args, kwargs, 0, "seed", Value(1492)).asInt();
        std::string type = resolveArg(args, kwargs, 1, "type", Value("settlement")).asString();
        if (type == "desert") {
            return Value(ProceduralNameGenerator::generateRegionTitle(static_cast<uint32_t>(seed), BiomeId::AridSteppe));
        } else if (type == "forest") {
            return Value(ProceduralNameGenerator::generateRegionTitle(static_cast<uint32_t>(seed), BiomeId::DeciduousWoodland));
        } else if (type == "mountain") {
            return Value(ProceduralNameGenerator::generateRegionTitle(static_cast<uint32_t>(seed), BiomeId::AlpinePeak));
        } else if (type == "dungeon") {
            return Value(ProceduralNameGenerator::generateLocaleName(static_cast<uint32_t>(seed), BiomeId::AlpinePeak, PoiCategory::DungeonPortal));
        } else {
            return Value(ProceduralNameGenerator::generateLocaleName(static_cast<uint32_t>(seed), BiomeId::LushMeadow, PoiCategory::CitadelKeep));
        }
    });

    evaluator_.registerMemberFunction("chart", "format_tracking", [](const auto& args, const auto& kwargs) -> Value {
        std::string text = resolveArg(args, kwargs, 0, "text", Value("")).asString();
        bool dbl = resolveArg(args, kwargs, 1, "double_space", Value(false)).asBoolean();
        return Value(ProceduralNameGenerator::formatCatQuestTracking(text, dbl));
    });

    evaluator_.registerMemberFunction("chart", "set_hero_pos", [this](const auto& args, const auto& kwargs) -> Value {
        float x = resolveArg(args, kwargs, 0, "x", Value(-4.0)).asFloat();
        float z = resolveArg(args, kwargs, 1, "z", Value(-2.0)).asFloat();
        ChartFieldSample s = world_.voronoi().sampleAt(x, z);
        world_.hero().pos = {x, std::max(0.0f, s.elevation), z};
        world_.camera().target   = world_.hero().pos;
        world_.camera().position = {x, 28.0f, z - 25.0f};
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "set_camera", [this](const auto& args, const auto& kwargs) -> Value {
        auto& cam = world_.camera();
        cam.position.x    = resolveArg(args, kwargs, 0, "x", Value(cam.position.x)).asFloat();
        cam.position.y    = resolveArg(args, kwargs, 1, "y", Value(cam.position.y)).asFloat();
        cam.position.z    = resolveArg(args, kwargs, 2, "z", Value(cam.position.z)).asFloat();
        cam.target.x      = resolveArg(args, kwargs, 3, "tx", Value(cam.target.x)).asFloat();
        cam.target.y      = resolveArg(args, kwargs, 4, "ty", Value(cam.target.y)).asFloat();
        cam.target.z      = resolveArg(args, kwargs, 5, "tz", Value(cam.target.z)).asFloat();
        cam.fovDegrees    = resolveArg(args, kwargs, 6, "fov", Value(cam.fovDegrees)).asFloat();
        cam.horizonCurve  = resolveArg(args, kwargs, 7, "curve", Value(cam.horizonCurve)).asFloat();
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "set_day_night_cycle", [this](const auto& args, const auto& kwargs) -> Value {
        bool enable = resolveArg(args, kwargs, 0, "enabled", Value(true)).asBoolean();
        world_.setUseDayNightCycle(enable);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "set_time_of_day", [this](const auto& args, const auto& kwargs) -> Value {
        float t = resolveArg(args, kwargs, 0, "time", Value(world_.timeOfDay())).asFloat();
        world_.setTimeOfDay(t);
        return Value(true);
    });
}

bool ChartScriptEngine::executeScript(const std::string& sourceCode) {
    lastError_.clear();
    evaluator_.evaluateSource(sourceCode);
    if (!evaluator_.getLastError().empty()) {
        lastError_ = evaluator_.getLastError();
        return false;
    }

    world_.rebuildWorldGeometry();
    return true;
}

bool ChartScriptEngine::executeFile(const std::string& filePath) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        lastError_ = "Could not open script file: " + filePath;
        return false;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    return executeScript(ss.str());
}

} // namespace eatscharted
