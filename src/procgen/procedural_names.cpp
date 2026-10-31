#include "eatscharted/procedural_names.hpp"
#include <array>
#include <cmath>
#include <sstream>

namespace eatscharted {

namespace {

[[nodiscard]] constexpr uint32_t mixHash(uint32_t h) noexcept {
    h ^= h >> 16u;
    h *= 0x85ebca6bu;
    h ^= h >> 13u;
    h *= 0xc2b2ae35u;
    h ^= h >> 16u;
    return h;
}

const std::array<const char*, 16> kDesertPrefixes = {
    "Dragon", "Sunfire", "Cinder", "Ashen", "Dune", "Brimstone", "Golden", "Dust",
    "Mirage", "Scorched", "Amber", "Redrock", "Searing", "Salt", "Vulture", "Obsidian"
};
const std::array<const char*, 8> kDesertSuffixes = {
    "Desert", "Sands", "Expanse", "Wastes", "Hollow", "Dunes", "Reach", "Barrens"
};

const std::array<const char*, 16> kForestPrefixes = {
    "Dusk", "Whispering", "Shadow", "Moss", "Gloom", "Elder", "Silver", "Mistveil",
    "Bramble", "Ironwood", "Witchwood", "Verdant", "Raven", "Emerald", "Timber", "Thorn"
};
const std::array<const char*, 8> kForestSuffixes = {
    "woods", "Forest", "Grove", "Thicket", "Canopy", "Pines", "Holt", "Deep"
};

const std::array<const char*, 14> kMountainPrefixes = {
    "Dragonbone", "Frostpeak", "Stormveil", "Ironhorn", "Cloudcrest", "Thunder", "Wyvern",
    "Grimpeak", "Shattered", "Stonecrag", "Highridge", "Silverpeak", "Shadowhorn", "Wolf"
};
const std::array<const char*, 6> kMountainSuffixes = {
    "Ridge", "Crags", "Peaks", "Pass", "Spire", "Heights"
};

const std::array<const char*, 16> kRealmNames = {
    "Kildland", "Silvervale Realm", "The Perilous Shores", "Crownlands", "Aethelgard",
    "Highprairie", "Oakhaven Reach", "Stormfell Domain", "Sunken Marches", "Valenreach",
    "Gilded Coast", "Westmarch", "Northcrag", "Dragonreach", "Ambervale", "Shadowmere"
};

const std::array<const char*, 16> kSettlementPrefixes = {
    "Low", "Cold", "Storm", "Oaken", "High", "Amber", "Iron", "Stone",
    "Wind", "Silver", "Gold", "Black", "Meadow", "Red", "West", "Frost"
};
const std::array<const char*, 16> kSettlementRoots = {
    "mill", "burg", "pool", "heart", "ford", "haven", "field", "keep",
    "port", "shire", "vale", "brook", "wick", "gate", "mere", "stead"
};

const std::array<const char*, 8> kDungeonNouns = {
    "Fort", "Palace", "Sanctum", "Crypt", "Tomb", "Delve", "Bastion", "Citadel"
};
const std::array<const char*, 10> kDungeonTitles = {
    "of the Undead-One", "of Shattered Souls", "of Whispers", "of the Sea-Wyrm",
    "of the Lost Sun", "of Cinders", "of the Sunken King", "of Shadows", "of Ruin", "of the Black Star"
};

} // namespace

std::string ProceduralNameGenerator::generateLocaleName(uint32_t seed, BiomeId biome, PoiCategory category) {
    uint32_t h0 = mixHash(seed ^ (static_cast<uint32_t>(biome) * 2654435761u));
    uint32_t h1 = mixHash(seed ^ 0x9e3779b9u);

    if (category == PoiCategory::DungeonPortal || category == PoiCategory::BossLair) {
        // Skull Dungeon Fort / Tomb (e.g. "Shattered Palace", "Fort of the Undead-One")
        if ((h0 & 3u) == 0u) {
            return std::string("SHATTERED PALACE");
        } else if ((h0 & 3u) == 1u) {
            return std::string("FORT OF THE UNDEAD-ONE");
        } else {
            const char* noun = kDungeonNouns[h0 % kDungeonNouns.size()];
            const char* title = kDungeonTitles[h1 % kDungeonTitles.size()];
            std::string res = std::string(noun) + " " + title;
            for (auto& c : res) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return res;
        }
    }

    if (category == PoiCategory::Windmill) {
        const char* pfx = kSettlementPrefixes[h0 % kSettlementPrefixes.size()];
        const char* root = (h1 % 2 == 0) ? "mill Grange" : "vale Mill";
        std::string res = std::string(pfx) + root;
        for (auto& c : res) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return res;
    }

    if (category == PoiCategory::Lighthouse) {
        const char* pfx = kSettlementPrefixes[h0 % kSettlementPrefixes.size()];
        const char* root = (h1 % 2 == 0) ? "pool Beacon" : "point Light";
        std::string res = std::string(pfx) + root;
        for (auto& c : res) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return res;
    }

    // Settlements (Keep, Hamlet, Harbor)
    const char* pfx = kSettlementPrefixes[h0 % kSettlementPrefixes.size()];
    const char* r1  = kSettlementRoots[h1 % kSettlementRoots.size()];
    uint32_t h2 = mixHash(seed ^ 0x85ebca6bu);
    const char* r2  = kSettlementRoots[h2 % kSettlementRoots.size()];

    std::string name;
    if ((h0 % 3) == 0) {
        // Compound: Cold + heart + field = Coldheartfield, Low + mill + burg = Lowmillburg
        name = std::string(pfx) + r1 + r2;
    } else {
        name = std::string(pfx) + r1;
    }

    for (auto& c : name) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return name;
}

std::string ProceduralNameGenerator::generateRegionTitle(uint32_t seed, BiomeId biome) {
    uint32_t h0 = mixHash(seed);
    uint32_t h1 = mixHash(seed ^ 0x3f58a742u);

    if (biome == BiomeId::AridSteppe || biome == BiomeId::BadlandsCanyon) {
        const char* pfx = kDesertPrefixes[h0 % kDesertPrefixes.size()];
        const char* sfx = kDesertSuffixes[h1 % kDesertSuffixes.size()];
        return std::string(pfx) + " " + sfx;
    }
    if (biome == BiomeId::DeciduousWoodland || biome == BiomeId::TaigaPines) {
        const char* pfx = kForestPrefixes[h0 % kForestPrefixes.size()];
        const char* sfx = kForestSuffixes[h1 % kForestSuffixes.size()];
        return std::string(pfx) + sfx;
    }
    if (biome == BiomeId::AlpinePeak || biome == BiomeId::SnowyTundra || biome == BiomeId::GlacialIce) {
        const char* pfx = kMountainPrefixes[h0 % kMountainPrefixes.size()];
        const char* sfx = kMountainSuffixes[h1 % kMountainSuffixes.size()];
        return std::string(pfx) + " " + sfx;
    }
    if (biome == BiomeId::OceanDeep || biome == BiomeId::CoastalShoals) {
        const std::array<const char*, 6> seas = {
            "The Shattered Sea", "The Perilous Shores", "Corsair Shoals",
            "Abyssal Reach", "Siren's Deep", "Merrow Bay"
        };
        return seas[h0 % seas.size()];
    }

    return kRealmNames[h0 % kRealmNames.size()];
}

std::string ProceduralNameGenerator::formatCatQuestTracking(std::string_view text, bool doubleSpace) {
    std::string out;
    out.reserve(text.size() * 3);
    for (size_t i = 0; i < text.size(); ++i) {
        out.push_back(text[i]);
        if (i + 1 < text.size() && text[i] != ' ' && text[i + 1] != ' ') {
            out.push_back(' ');
            if (doubleSpace) out.push_back(' ');
        }
    }
    return out;
}

CurvedMapBanner ProceduralNameGenerator::createCurvedBanner(
    const std::string& text,
    Vec2f center,
    float spanWidth,
    float angleRadians,
    float archCurvature,
    float letterSize,
    bool isRegionHeader,
    bool isWaterTitle
) {
    CurvedMapBanner b{};
    b.text = text;
    b.letterSize = letterSize;
    b.isRegionHeader = isRegionHeader;
    b.isWaterTitle = isWaterTitle;

    float halfW = spanWidth * 0.5f;
    float dirX = std::cos(angleRadians);
    float dirZ = std::sin(angleRadians);
    float normX = -dirZ;
    float normZ =  dirX;

    b.p0 = {center.x - dirX * halfW, center.y - dirZ * halfW};
    b.p2 = {center.x + dirX * halfW, center.y + dirZ * halfW};
    b.p1 = {center.x + normX * (spanWidth * archCurvature),
            center.y + normZ * (spanWidth * archCurvature)};

    return b;
}

} // namespace eatscharted
