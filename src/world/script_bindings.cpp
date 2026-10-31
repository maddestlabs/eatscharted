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

    std::map<std::string, Value> dungeonObj;
    dungeonObj["name"] = Value("OnePageDungeonEngine");
    evaluator_.setVariable("dungeon", Value(dungeonObj));

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

    evaluator_.registerMemberFunction("chart", "build_sunscorched_wastes", [this](const auto& args, const auto& kwargs) -> Value {
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 0, "seed", Value(1066)).asInt());
        world_.buildSunscorchedWastes(seed);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "build_frostfell_peaks", [this](const auto& args, const auto& kwargs) -> Value {
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 0, "seed", Value(1845)).asInt());
        world_.buildFrostfellPeaks(seed);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "build_continental_overworld", [this](const auto& args, const auto& kwargs) -> Value {
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 0, "seed", Value(2026)).asInt());
        world_.buildContinentalPangaea(seed);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "build_one_page_dungeon", [this](const auto& args, const auto& kwargs) -> Value {
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 0, "seed", Value(999)).asInt());
        world_.buildOnePageDungeon(seed);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "derive_sub_seed", [this](const auto& args, const auto& kwargs) -> Value {
        uint32_t masterSeed = static_cast<uint32_t>(resolveArg(args, kwargs, 0, "master_seed", Value(world_.voronoi().seed())).asInt());
        uint32_t featureId  = static_cast<uint32_t>(resolveArg(args, kwargs, 1, "feature_id", Value(1)).asInt());
        uint32_t floor      = static_cast<uint32_t>(resolveArg(args, kwargs, 2, "floor", Value(1)).asInt());
        uint32_t subSeed    = VoronoiAtlas::deriveSubSeed(masterSeed, featureId, floor);
        return Value(subSeed);
    });

    evaluator_.registerMemberFunction("chart", "get_world_seed", [this](const auto&, const auto&) -> Value {
        return Value(world_.voronoi().seed());
    });

    evaluator_.registerMemberFunction("chart", "build_watabou_dungeon", [this](const auto& args, const auto& kwargs) -> Value {
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 0, "seed", Value(999)).asInt());
        std::string themeStr = resolveArg(args, kwargs, 1, "theme", Value("auto")).asString();
        DungeonTheme th = DungeonTheme::Auto;
        if (themeStr == "sunken") th = DungeonTheme::SunkenSanctum;
        else if (themeStr == "crypt") th = DungeonTheme::WhisperingCrypt;
        else if (themeStr == "molten") th = DungeonTheme::MoltenVault;
        else if (themeStr == "gilded") th = DungeonTheme::GildedReliquary;
        world_.buildWatabouDungeon(seed, th);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "set_weather", [this](const auto& args, const auto& kwargs) -> Value {
        std::string typeStr = resolveArg(args, kwargs, 0, "type", Value("clear")).asString();
        float intensity = resolveArg(args, kwargs, 1, "intensity", Value(1.0)).asFloat();
        WeatherType wt = WeatherType::Clear;
        if (typeStr == "overcast") wt = WeatherType::Overcast;
        else if (typeStr == "drizzle") wt = WeatherType::Drizzle;
        else if (typeStr == "downpour" || typeStr == "rain") wt = WeatherType::Downpour;
        else if (typeStr == "thunderstorm" || typeStr == "storm") wt = WeatherType::Thunderstorm;
        else if (typeStr == "flurries" || typeStr == "snow_flurries") wt = WeatherType::Flurries;
        else if (typeStr == "light_snow") { wt = WeatherType::Flurries; intensity = 0.20f; }
        else if (typeStr == "blizzard" || typeStr == "snow" || typeStr == "heavy_snow") wt = WeatherType::Blizzard;
        else if (typeStr == "sandstorm" || typeStr == "dust") wt = WeatherType::Sandstorm;
        world_.setWeatherType(wt, intensity);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "set_weather_cycle", [this](const auto& args, const auto& kwargs) -> Value {
        bool enable = resolveArg(args, kwargs, 0, "enable", Value(true)).asBoolean();
        float speed = resolveArg(args, kwargs, 1, "speed", Value(1.0f / 240.0f)).asFloat();
        auto w = world_.weather();
        w.autoCycle = enable;
        w.cycleSpeed = speed;
        world_.setWeather(w);
        return Value(true);
    });

    evaluator_.registerMemberFunction("chart", "get_weather", [this](const auto&, const auto&) -> Value {
        std::map<std::string, Value> wDict;
        auto localW = world_.evaluateLocalWeather(world_.hero().pos.x, world_.hero().pos.z);
        wDict["type"] = Value(weatherTypeName(localW.type));
        wDict["description"] = Value(localW.description);
        wDict["temperature"] = Value(static_cast<double>(localW.temperature));
        wDict["intensity"] = Value(static_cast<double>(localW.intensity));
        wDict["precipitation"] = Value(static_cast<double>(localW.precipitation));
        wDict["wetness"] = Value(static_cast<double>(world_.weather().wetness));
        wDict["snow_cover"] = Value(static_cast<double>(world_.weather().snowCover));
        wDict["storm_surge"] = Value(static_cast<double>(world_.weather().stormSurge));
        return Value(wDict);
    });

    // Watabou One-Page Dungeon Engine API bindings
    evaluator_.registerMemberFunction("dungeon", "init", [this](const auto& args, const auto& kwargs) -> Value {
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 0, "seed", Value(999)).asInt());
        std::string title = resolveArg(args, kwargs, 1, "title", Value("THE SUNKEN SANCTUM")).asString();
        std::string subtitle = resolveArg(args, kwargs, 2, "subtitle", Value("Watabou One-Page Dungeon")).asString();
        world_.initDungeon(seed, title, subtitle);
        return Value(true);
    });

    evaluator_.registerMemberFunction("dungeon", "set_sunlight", [this](const auto& args, const auto& kwargs) -> Value {
        float r = resolveArg(args, kwargs, 0, "r", Value(1.0)).asFloat();
        float g = resolveArg(args, kwargs, 1, "g", Value(0.9)).asFloat();
        float b = resolveArg(args, kwargs, 2, "b", Value(0.8)).asFloat();
        float intensity = resolveArg(args, kwargs, 3, "intensity", Value(1.0)).asFloat();
        auto atm = world_.atmosphere();
        atm.sunColor = {r * intensity, g * intensity, b * intensity};
        world_.setAtmosphere(atm);
        return Value(true);
    });

    evaluator_.registerMemberFunction("dungeon", "enable_day_night", [this](const auto& args, const auto& kwargs) -> Value {
        bool enable = resolveArg(args, kwargs, 0, "enable", Value(true)).asBoolean();
        world_.setUseDayNightCycle(enable);
        return Value(true);
    });

    evaluator_.registerMemberFunction("dungeon", "set_ambient", [this](const auto& args, const auto& kwargs) -> Value {
        float r = resolveArg(args, kwargs, 0, "r", Value(0.02)).asFloat();
        float g = resolveArg(args, kwargs, 1, "g", Value(0.02)).asFloat();
        float b = resolveArg(args, kwargs, 2, "b", Value(0.03)).asFloat();
        auto atm = world_.atmosphere();
        atm.skyZenith = {r, g, b};
        atm.skyHorizon = {r * 1.5f, g * 1.5f, b * 1.5f};
        world_.setAtmosphere(atm);
        return Value(true);
    });

    evaluator_.registerMemberFunction("dungeon", "add_room", [this](const auto& args, const auto& kwargs) -> Value {
        float minX = resolveArg(args, kwargs, 0, "min_x", Value(-10.0)).asFloat();
        float minZ = resolveArg(args, kwargs, 1, "min_z", Value(-10.0)).asFloat();
        float maxX = resolveArg(args, kwargs, 2, "max_x", Value(10.0)).asFloat();
        float maxZ = resolveArg(args, kwargs, 3, "max_z", Value(10.0)).asFloat();
        std::string label = resolveArg(args, kwargs, 4, "label", Value("")).asString();
        bool water = resolveArg(args, kwargs, 5, "water", Value(false)).asBoolean();
        bool boss = resolveArg(args, kwargs, 6, "boss", Value(false)).asBoolean();
        world_.addDungeonRoom(minX, minZ, maxX, maxZ, label, water, boss);
        return Value(true);
    });

    evaluator_.registerMemberFunction("dungeon", "add_corridor", [this](const auto& args, const auto& kwargs) -> Value {
        float x1 = resolveArg(args, kwargs, 0, "x1", Value(0.0)).asFloat();
        float z1 = resolveArg(args, kwargs, 1, "z1", Value(0.0)).asFloat();
        float x2 = resolveArg(args, kwargs, 2, "x2", Value(0.0)).asFloat();
        float z2 = resolveArg(args, kwargs, 3, "z2", Value(10.0)).asFloat();
        float width = resolveArg(args, kwargs, 4, "width", Value(3.2)).asFloat();
        world_.addDungeonCorridor(x1, z1, x2, z2, width);
        return Value(true);
    });

    evaluator_.registerMemberFunction("dungeon", "add_water_pool", [this](const auto& args, const auto& kwargs) -> Value {
        float minX = resolveArg(args, kwargs, 0, "min_x", Value(-5.0)).asFloat();
        float minZ = resolveArg(args, kwargs, 1, "min_z", Value(-5.0)).asFloat();
        float maxX = resolveArg(args, kwargs, 2, "max_x", Value(5.0)).asFloat();
        float maxZ = resolveArg(args, kwargs, 3, "max_z", Value(5.0)).asFloat();
        world_.addDungeonWaterPool(minX, minZ, maxX, maxZ);
        return Value(true);
    });

    evaluator_.registerMemberFunction("dungeon", "add_torch", [this](const auto& args, const auto& kwargs) -> Value {
        float x = resolveArg(args, kwargs, 0, "x", Value(0.0)).asFloat();
        float z = resolveArg(args, kwargs, 1, "z", Value(0.0)).asFloat();
        float r = resolveArg(args, kwargs, 2, "r", Value(1.0)).asFloat();
        float g = resolveArg(args, kwargs, 3, "g", Value(0.72)).asFloat();
        float b = resolveArg(args, kwargs, 4, "b", Value(0.36)).asFloat();
        float radius = resolveArg(args, kwargs, 5, "radius", Value(14.0)).asFloat();
        float intensity = resolveArg(args, kwargs, 6, "intensity", Value(1.35)).asFloat();
        world_.addDungeonTorch({x, 0.25f, z}, {r, g, b}, radius, intensity);
        return Value(true);
    });

    evaluator_.registerMemberFunction("dungeon", "add_pillar", [this](const auto& args, const auto& kwargs) -> Value {
        float x = resolveArg(args, kwargs, 0, "x", Value(0.0)).asFloat();
        float z = resolveArg(args, kwargs, 1, "z", Value(0.0)).asFloat();
        world_.addDungeonPillar({x, 0.25f, z});
        return Value(true);
    });

    evaluator_.registerMemberFunction("dungeon", "add_pillar_row", [this](const auto& args, const auto& kwargs) -> Value {
        float x1 = resolveArg(args, kwargs, 0, "x1", Value(-10.0)).asFloat();
        float z1 = resolveArg(args, kwargs, 1, "z1", Value(0.0)).asFloat();
        float x2 = resolveArg(args, kwargs, 2, "x2", Value(10.0)).asFloat();
        float z2 = resolveArg(args, kwargs, 3, "z2", Value(0.0)).asFloat();
        int count = resolveArg(args, kwargs, 4, "count", Value(4)).asInt();
        if (count < 2) count = 2;
        for (int i = 0; i < count; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(count - 1);
            float px = x1 + t * (x2 - x1);
            float pz = z1 + t * (z2 - z1);
            world_.addDungeonPillar({px, 0.25f, pz});
        }
        return Value(true);
    });

    evaluator_.registerMemberFunction("dungeon", "add_chest", [this](const auto& args, const auto& kwargs) -> Value {
        float x = resolveArg(args, kwargs, 0, "x", Value(0.0)).asFloat();
        float z = resolveArg(args, kwargs, 1, "z", Value(0.0)).asFloat();
        world_.addDungeonChest({x, 0.25f, z});
        return Value(true);
    });

    evaluator_.registerMemberFunction("dungeon", "add_exit_portal", [this](const auto& args, const auto& kwargs) -> Value {
        float x = resolveArg(args, kwargs, 0, "x", Value(0.0)).asFloat();
        float z = resolveArg(args, kwargs, 1, "z", Value(-30.0)).asFloat();
        world_.addDungeonExitPortal({x, 0.25f, z});
        return Value(true);
    });

    evaluator_.registerMemberFunction("dungeon", "add_enemy", [this](const auto& args, const auto& kwargs) -> Value {
        float x = resolveArg(args, kwargs, 0, "x", Value(0.0)).asFloat();
        float z = resolveArg(args, kwargs, 1, "z", Value(0.0)).asFloat();
        std::string type = resolveArg(args, kwargs, 2, "type", Value("slime")).asString();
        std::string name = resolveArg(args, kwargs, 3, "name", Value("")).asString();
        int hp = resolveArg(args, kwargs, 4, "hp", Value(50)).asInt();
        bool boss = resolveArg(args, kwargs, 5, "boss", Value(false)).asBoolean();

        uint16_t layer = InkLayer::StandeeMonsterSlime;
        float telegraphRadius = 3.5f;
        uint32_t telegraphType = TelegraphType::EnemyCircle;

        if (type == "boss" || type == "wyvern" || boss) {
            layer = InkLayer::StandeeMonsterWyvern;
            if (name.empty()) name = "The Elder Wyvern";
            if (hp <= 50) hp = 220;
            telegraphRadius = 6.4f;
            telegraphType = TelegraphType::EnemyCircle;
            boss = true;
        } else if (type == "bandit" || type == "marauder" || type == "skeleton") {
            layer = InkLayer::StandeeMonsterBandit;
            if (name.empty()) name = "Crypt Marauder";
            if (hp <= 50) hp = 65;
            telegraphRadius = 4.0f;
            telegraphType = TelegraphType::EnemyCone;
        } else {
            layer = InkLayer::StandeeMonsterSlime;
            if (name.empty()) name = "Rune Slime";
            telegraphRadius = 3.2f;
            telegraphType = TelegraphType::EnemyCircle;
        }

        world_.addDungeonEnemy({x, 0.25f, z}, name, layer, hp, boss, telegraphRadius, telegraphType);
        return Value(true);
    });

    evaluator_.registerMemberFunction("dungeon", "build_watabou", [this](const auto& args, const auto& kwargs) -> Value {
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 0, "seed", Value(999)).asInt());
        std::string themeStr = resolveArg(args, kwargs, 1, "theme", Value("auto")).asString();
        DungeonTheme th = DungeonTheme::Auto;
        if (themeStr == "sunken") th = DungeonTheme::SunkenSanctum;
        else if (themeStr == "crypt") th = DungeonTheme::WhisperingCrypt;
        else if (themeStr == "molten") th = DungeonTheme::MoltenVault;
        else if (themeStr == "gilded") th = DungeonTheme::GildedReliquary;
        world_.buildWatabouDungeon(seed, th);
        return Value(true);
    });

    evaluator_.registerMemberFunction("dungeon", "build_procedural", [this](const auto& args, const auto& kwargs) -> Value {
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 0, "seed", Value(999)).asInt());
        std::string themeStr = resolveArg(args, kwargs, 1, "theme", Value("auto")).asString();
        DungeonTheme th = DungeonTheme::Auto;
        if (themeStr == "sunken") th = DungeonTheme::SunkenSanctum;
        else if (themeStr == "crypt") th = DungeonTheme::WhisperingCrypt;
        else if (themeStr == "molten") th = DungeonTheme::MoltenVault;
        else if (themeStr == "gilded") th = DungeonTheme::GildedReliquary;
        world_.buildWatabouDungeon(seed, th);
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
