#include "eatscharted/voronoi_atlas.hpp"
#include "eatscharted/procedural_names.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

namespace eatscharted {

namespace {

[[nodiscard]] uint32_t hashU32(uint32_t x) noexcept {
    x ^= x >> 16u;
    x *= 0x7FEB352Du;
    x ^= x >> 15u;
    x *= 0x846CA68Bu;
    x ^= x >> 16u;
    return x;
}

[[nodiscard]] inline uint64_t packBlockCoord(int bx, int bz) noexcept {
    return (static_cast<uint64_t>(static_cast<uint32_t>(bx)) << 32u) |
           static_cast<uint64_t>(static_cast<uint32_t>(bz));
}

[[nodiscard]] float hashFloat(int x, int z, uint32_t seed) noexcept {
    uint32_t h = hashU32(static_cast<uint32_t>(x) * 374761393u +
                         static_cast<uint32_t>(z) * 668265263u +
                         seed * 1442695041u);
    return static_cast<float>(h & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

[[nodiscard]] float smoothValueNoise(float x, float z, uint32_t seed) noexcept {
    int ix = static_cast<int>(std::floor(x));
    int iz = static_cast<int>(std::floor(z));
    float fx = x - static_cast<float>(ix);
    float fz = z - static_cast<float>(iz);

    float u = fx * fx * (3.0f - 2.0f * fx);
    float v = fz * fz * (3.0f - 2.0f * fz);

    float n00 = hashFloat(ix,     iz,     seed);
    float n10 = hashFloat(ix + 1, iz,     seed);
    float n01 = hashFloat(ix,     iz + 1, seed);
    float n11 = hashFloat(ix + 1, iz + 1, seed);

    float nx0 = n00 + u * (n10 - n00);
    float nx1 = n01 + u * (n11 - n01);
    return (nx0 + v * (nx1 - nx0)) * 2.0f - 1.0f;
}

[[nodiscard]] float fbmNoise(float x, float z, uint32_t seed, int octaves = 5) noexcept {
    float sum = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * smoothValueNoise(x * freq, z * freq, seed + static_cast<uint32_t>(i) * 101u);
        freq *= 2.07f;
        amp *= 0.5f;
    }
    return sum;
}

[[nodiscard]] float pointToSegmentDist(Vec2f p, Vec2f a, Vec2f b, float& outT) noexcept {
    float vx = b.x - a.x;
    float vy = b.y - a.y;
    float wx = p.x - a.x;
    float wy = p.y - a.y;
    float lenSq = vx * vx + vy * vy;
    float t = (lenSq > 1e-6f) ? ((wx * vx + wy * vy) / lenSq) : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    outT = t;
    float dx = p.x - (a.x + t * vx);
    float dy = p.y - (a.y + t * vy);
    return std::sqrt(dx * dx + dy * dy);
}

[[nodiscard]] BiomeId classifyBiome(float elevMeters, float moisture, float coastDist, uint32_t seedHash) noexcept {
    if (elevMeters <= -2.2f) return BiomeId::OceanDeep;
    if (elevMeters <= 0.0f)  return BiomeId::CoastalShoals;
    if (coastDist < 3.4f && elevMeters < 1.35f) {
        return (moisture > 0.66f) ? BiomeId::SaltMarsh : BiomeId::SandyBeach;
    }
    if (elevMeters > 7.2f) {
        return BiomeId::AlpinePeak;
    }
    if (elevMeters > 4.2f) {
        return (moisture > 0.42f) ? BiomeId::TaigaPines : BiomeId::BadlandsCanyon;
    }
    if ((seedHash & 31u) == 0u && elevMeters > 2.2f && moisture < 0.35f) {
        return BiomeId::CalamityBlight;
    }
    if (moisture > 0.68f) return BiomeId::DeciduousWoodland;
    if (moisture > 0.36f) return BiomeId::LushMeadow;
    if (moisture > 0.18f) return BiomeId::AridSteppe;
    return BiomeId::BadlandsCanyon;
}

} // namespace

void VoronoiAtlas::generate(uint32_t seed, AtlasPreset preset) {
    seed_ = seed;
    preset_ = preset;
    cells_.clear();
    rivers_.clear();
    riverSpatialGrid_.clear();
    roads_.clear();
    pois_.clear();
    banners_.clear();
    dungeonRooms_.clear();
    generatedBlocks_.clear();
    generatedBlocks_.insert(packBlockCoord(0, 0));

    if (preset_ == AtlasPreset::OnePageDungeon) {
        generateOnePageDungeonLayout();
    } else {
        generateOverworldGraph();
    }
}

float VoronoiAtlas::rawElevationAt(float wx, float wz) const noexcept {
    if (preset_ == AtlasPreset::ContinentalPangaea) {
        return rawElevationContinental(wx, wz);
    }
    if (preset_ == AtlasPreset::SunscorchedWastes) {
        return rawElevationDesert(wx, wz);
    }
    // Domain-warped fractal perturbation for authentic Watabou Perilous Shores jagged coastlines
    float warpX = fbmNoise(wx * 0.018f, wz * 0.018f, seed_ + 17u, 3) * 14.0f;
    float warpZ = fbmNoise(wx * 0.018f + 9.3f, wz * 0.018f - 4.7f, seed_ + 31u, 3) * 14.0f;
    float qx = wx + warpX;
    float qz = wz + warpZ;

    float detail = fbmNoise(qx * 0.032f, qz * 0.032f, seed_ + 53u, 5);
    float ridge  = 1.0f - std::abs(fbmNoise(qx * 0.022f, qz * 0.022f, seed_ + 79u, 4));
    ridge = ridge * ridge;

    if (preset_ == AtlasPreset::PerilousPeninsula) {
        // Sweeping mainland peninsula entering from the North/East with a sheltered bay in the South-West
        // and offshore barrier islands & sea stacks
        float mainlandBias = (qz * 0.014f) + (qx * 0.008f) + 0.22f;
        // Carve a dramatic sheltered harbor bay around (-16, -12)
        float bayDx = (qx + 18.0f) / 34.0f;
        float bayDz = (qz + 14.0f) / 28.0f;
        float bayCarve = std::exp(-(bayDx * bayDx + bayDz * bayDz) * 1.4f) * 0.72f;

        // Offshore island chain in the South-West (-42, -36) & (26, -38)
        float isl1Dx = (qx + 38.0f) / 18.0f;
        float isl1Dz = (qz + 32.0f) / 16.0f;
        float isl1 = std::exp(-(isl1Dx * isl1Dx + isl1Dz * isl1Dz) * 1.6f) * 0.68f;

        float isl2Dx = (qx - 28.0f) / 16.0f;
        float isl2Dz = (qz + 34.0f) / 14.0f;
        float isl2 = std::exp(-(isl2Dx * isl2Dx + isl2Dz * isl2Dz) * 1.8f) * 0.62f;

        // Northern alpine ridge spine
        float spineDz = (qz - 42.0f) / 24.0f;
        float spine = std::exp(-(spineDz * spineDz)) * ridge * 1.25f;

        // Sorcery! tactile rolling mounds & foothills
        float mounds = std::pow(std::max(0.0f, fbmNoise(qx * 0.048f + 5.2f, qz * 0.048f - 3.1f, seed_ + 89u, 3)), 1.8f) * 0.72f;
        float crags = (1.0f - std::abs(fbmNoise(qx * 0.055f, qz * 0.055f, seed_ + 103u, 3))) * ridge * 0.55f;

        float hNorm = mainlandBias - bayCarve + isl1 + isl2 + spine + mounds + crags + detail * 0.42f;
        return hNorm * 6.5f;
    } else if (preset_ == AtlasPreset::ShatteredArchipelago) {
        // Multi-island nautical chart with central lagoon & surrounding coral/rocky islands
        float r = std::sqrt(qx * qx + qz * qz);
        float ring = std::exp(-std::pow((r - 36.0f) / 22.0f, 2.0f)) * 0.55f;
        float centerIsle = std::exp(-(qx * qx + qz * qz) / (18.0f * 18.0f)) * 0.75f;
        float mounds = std::pow(std::max(0.0f, fbmNoise(qx * 0.05f, qz * 0.05f, seed_ + 89u, 3)), 1.6f) * 0.55f;
        float archipelago = fbmNoise(qx * 0.028f, qz * 0.028f, seed_ + 113u, 5) * 0.85f;
        float hNorm = ring + centerIsle + archipelago + mounds - 0.18f;
        return hNorm * 6.2f;
    } else {
        // HighlandKingdom: Twin mountain ranges flanking a lush central river valley
        float valleyDist = std::abs(qx - std::sin(qz * 0.035f) * 18.0f);
        float valleyProfile = std::clamp((valleyDist - 10.0f) / 42.0f, -0.25f, 1.1f);
        float southernCoast = std::clamp((qz + 58.0f) / 25.0f, -0.6f, 0.4f);
        float crags = (1.0f - std::abs(fbmNoise(qx * 0.045f, qz * 0.045f, seed_ + 117u, 3))) * ridge * 0.65f;
        float mounds = std::pow(std::max(0.0f, fbmNoise(qx * 0.042f, qz * 0.042f, seed_ + 71u, 3)), 1.7f) * 0.60f;
        float hNorm = valleyProfile * 0.85f + southernCoast * 0.45f + detail * 0.42f + crags + mounds;
        return hNorm * 7.2f;
    }
}

float VoronoiAtlas::rawMoistureAt(float wx, float wz) const noexcept {
    if (preset_ == AtlasPreset::ContinentalPangaea) {
        return rawMoistureContinental(wx, wz);
    }
    if (preset_ == AtlasPreset::SunscorchedWastes) {
        return rawMoistureDesert(wx, wz);
    }
    if (preset_ == AtlasPreset::PerilousPeninsula) {
        // High moisture in northern/western peninsula interior (Duskwoods), arid in central/southern plains (Dragon Desert)
        float forestBias = std::exp(-((wx + 8.0f) * (wx + 8.0f) + (wz - 34.0f) * (wz - 34.0f)) / (22.0f * 22.0f)) * 0.78f;
        float desertBias = std::exp(-((wx - 14.0f) * (wx - 14.0f) + (wz + 6.0f) * (wz + 6.0f)) / (26.0f * 26.0f)) * 0.42f;
        float m = 0.42f + forestBias - desertBias + fbmNoise(wx * 0.021f, wz * 0.021f, seed_ + 211u, 3) * 0.20f;
        return std::clamp(m, 0.0f, 1.0f);
    }
    float m = 0.52f + fbmNoise(wx * 0.021f + 31.0f, wz * 0.021f - 19.0f, seed_ + 211u, 4) * 0.48f;
    return std::clamp(m, 0.0f, 1.0f);
}

float VoronoiAtlas::rawElevationDesert(float wx, float wz, Vec2f center, float radius) const noexcept {
    float qx = wx - center.x;
    float qz = wz - center.y;

    // Domain-warped coordinates for organic dune ripples and sinuous sandstone strata
    float warpX = fbmNoise(qx * 0.016f, qz * 0.016f, seed_ + 41u, 3) * 14.0f;
    float warpZ = fbmNoise(qx * 0.016f + 7.2f, qz * 0.016f - 5.4f, seed_ + 67u, 3) * 14.0f;
    float px = qx + warpX;
    float pz = qz + warpZ;

    // 1. Windblown Sand Dunes (Transverse waves with steep slip-faces)
    float dunePhase = px * 0.072f + pz * 0.038f + fbmNoise(px * 0.024f, pz * 0.024f, seed_ + 83u, 2) * 2.2f;
    float duneWave = std::sin(dunePhase);
    float dune = std::pow(std::max(0.0f, duneWave), 1.6f) * 0.95f;

    // 2. Terraced Sandstone Mesas & Buttes
    float mesaNoise = fbmNoise(px * 0.022f + 14.0f, pz * 0.022f - 9.0f, seed_ + 109u, 3);
    float mesaTier1 = std::clamp((mesaNoise - 0.18f) / 0.10f, 0.0f, 1.0f) * 2.4f;
    float mesaTier2 = std::clamp((mesaNoise - 0.48f) / 0.08f, 0.0f, 1.0f) * 2.2f;
    float mesaEdge = (1.0f - std::abs(fbmNoise(px * 0.045f, pz * 0.045f, seed_ + 131u, 2))) * 0.45f;
    float mesa = mesaTier1 + mesaTier2 + mesaEdge * (mesaTier1 > 0.1f ? 1.0f : 0.0f);

    // 3. Sunken Wadi / Arroyo (Dry wash meandering across desert)
    float wadiCurve = std::sin(px * 0.042f) * 16.0f - 4.0f;
    float wadiDist = std::abs(pz - wadiCurve);
    float wadiCarve = std::clamp(1.0f - (wadiDist / 7.5f), 0.0f, 1.0f);
    float wadi = -std::pow(wadiCarve, 1.6f) * 1.7f;

    // 4. Al-Zahra Oasis Depression (Sunken emerald pool at offset (12, -6) from desert center)
    float oasisDx = (qx - 12.0f) / 15.0f;
    float oasisDz = (qz + 6.0f) / 13.0f;
    float oasisDistSq = oasisDx * oasisDx + oasisDz * oasisDz;
    float oasisCarve = std::clamp(1.0f - std::sqrt(oasisDistSq), 0.0f, 1.0f);
    float oasisH = -std::pow(oasisCarve, 2.0f) * 1.85f;

    // 5. Desert scree & micro-topography
    float scree = fbmNoise(px * 0.055f, pz * 0.055f, seed_ + 173u, 3) * 0.35f;

    float h = 1.6f + dune + mesa + wadi + oasisH + scree;

    if (radius > 0.0f) {
        float r = std::sqrt(qx * qx + qz * qz);
        if (r > radius * 0.80f) {
            float fade = std::clamp(1.0f - (r - radius * 0.80f) / (radius * 0.20f), 0.0f, 1.0f);
            h = h * fade;
        }
    }
    return h;
}

float VoronoiAtlas::rawMoistureDesert(float wx, float wz, Vec2f center, float radius) const noexcept {
    float qx = wx - center.x;
    float qz = wz - center.y;

    // General desert aridity baseline: 0.08 to 0.16 (triggers AridSteppe & BadlandsCanyon)
    float baseM = 0.11f + fbmNoise(qx * 0.022f + 29.0f, qz * 0.022f - 17.0f, seed_ + 199u, 2) * 0.05f;

    // Al-Zahra Oasis spike: Concentrated lush moisture pulse centered at (12, -6)
    float oasisDx = (qx - 12.0f) / 14.0f;
    float oasisDz = (qz + 6.0f) / 12.0f;
    float oasisDistSq = oasisDx * oasisDx + oasisDz * oasisDz;
    float oasisMoisture = std::exp(-oasisDistSq * 2.2f) * 0.82f;

    // Slight moisture seepage along the dry wadi floor
    float wadiCurve = std::sin(qx * 0.042f) * 16.0f - 4.0f;
    float wadiDist = std::abs(qz - wadiCurve);
    float wadiMoisture = std::clamp(1.0f - (wadiDist / 9.0f), 0.0f, 1.0f) * 0.22f;

    float m = baseM + oasisMoisture + wadiMoisture;
    if (radius > 0.0f) {
        float r = std::sqrt(qx * qx + qz * qz);
        if (r > radius * 0.80f) {
            float fade = std::clamp((r - radius * 0.80f) / (radius * 0.20f), 0.0f, 1.0f);
            m = m * (1.0f - fade) + 0.50f * fade;
        }
    }
    return std::clamp(m, 0.0f, 1.0f);
}

float VoronoiAtlas::rawElevationContinental(float wx, float wz) const noexcept {
    const float rOriginSq = wx * wx + wz * wz;

    float hCore = 0.0f;
    if (rOriginSq < 280.0f * 280.0f) {
        // 1. Provincial centers and spatial spread parameters (Core Pangaea Continent, expanded scale)
        const Vec2f cHighlands{0.0f, 190.0f};
        const Vec2f cDesert{190.0f, 0.0f};
        const Vec2f cPeninsula{-190.0f, 0.0f};
        const Vec2f cArchipelago{0.0f, -190.0f};
        const Vec2f cCrossroads{0.0f, 0.0f};

        const float sigmaProv = 110.0f;
        const float sigmaCenter = 75.0f;
        const float inv2SigmaProvSq = 1.0f / (2.0f * sigmaProv * sigmaProv);
        const float inv2SigmaCenterSq = 1.0f / (2.0f * sigmaCenter * sigmaCenter);

        float dSqHighlands = (wx - cHighlands.x) * (wx - cHighlands.x) + (wz - cHighlands.y) * (wz - cHighlands.y);
        float dSqDesert = (wx - cDesert.x) * (wx - cDesert.x) + (wz - cDesert.y) * (wz - cDesert.y);
        float dSqPeninsula = (wx - cPeninsula.x) * (wx - cPeninsula.x) + (wz - cPeninsula.y) * (wz - cPeninsula.y);
        float dSqArchipelago = (wx - cArchipelago.x) * (wx - cArchipelago.x) + (wz - cArchipelago.y) * (wz - cArchipelago.y);
        float dSqCenter = (wx - cCrossroads.x) * (wx - cCrossroads.x) + (wz - cCrossroads.y) * (wz - cCrossroads.y);

        float wHighlands = std::exp(-dSqHighlands * inv2SigmaProvSq);
        float wDesert = std::exp(-dSqDesert * inv2SigmaProvSq);
        float wPeninsula = std::exp(-dSqPeninsula * inv2SigmaProvSq);
        float wArchipelago = std::exp(-dSqArchipelago * inv2SigmaProvSq);
        float wCenter = std::exp(-dSqCenter * inv2SigmaCenterSq);

        float totalW = wHighlands + wDesert + wPeninsula + wArchipelago + wCenter;
        if (totalW > 1e-5f) {
            float invW = 1.0f / totalW;
            wHighlands *= invW;
            wDesert *= invW;
            wPeninsula *= invW;
            wArchipelago *= invW;
            wCenter *= invW;
        } else {
            wCenter = 1.0f;
        }

        // 2. Evaluate each provincial elevation model in scaled relative coordinates
        // kCoordScale = 0.42f expands provincial geographical features ~2.38x in world space
        constexpr float kCoordScale = 0.42f;
        constexpr float kWeightGate = 1e-3f;

        // A. Highland Kingdom (North)
        if (wHighlands > kWeightGate) {
            float qxH = (wx - cHighlands.x) * kCoordScale;
            float qzH = (wz - cHighlands.y) * kCoordScale;
            float warpHX = fbmNoise(qxH * 0.018f, qzH * 0.018f, seed_ + 17u, 3) * 12.0f;
            float warpHZ = fbmNoise(qxH * 0.018f + 9.3f, qzH * 0.018f - 4.7f, seed_ + 31u, 3) * 12.0f;
            float pxH = qxH + warpHX;
            float pzH = qzH + warpHZ;
            float detailH = fbmNoise(pxH * 0.032f, pzH * 0.032f, seed_ + 53u, 5);
            float ridgeH = 1.0f - std::abs(fbmNoise(pxH * 0.022f, pzH * 0.022f, seed_ + 79u, 4));
            ridgeH = ridgeH * ridgeH;
            float valleyDist = std::abs(pxH - std::sin(pzH * 0.035f) * 18.0f);
            float valleyProfile = std::clamp((valleyDist - 8.0f) / 36.0f, -0.1f, 1.2f);
            float cragsH = (1.0f - std::abs(fbmNoise(pxH * 0.045f, pzH * 0.045f, seed_ + 117u, 3))) * ridgeH * 0.65f;
            float moundsH = std::pow(std::max(0.0f, fbmNoise(pxH * 0.042f, pzH * 0.042f, seed_ + 71u, 3)), 1.7f) * 0.60f;
            float hHighlands = (valleyProfile * 0.90f + detailH * 0.40f + cragsH + moundsH + 0.35f) * 6.8f;
            hCore += wHighlands * hHighlands;
        }

        // B. Sunscorched Wastes (East)
        if (wDesert > kWeightGate) {
            float qxD = (wx - cDesert.x) * kCoordScale;
            float qzD = (wz - cDesert.y) * kCoordScale;
            float hDesert = rawElevationDesert(cDesert.x + qxD, cDesert.y + qzD, cDesert, 0.0f);
            hCore += wDesert * hDesert;
        }

        // C. Perilous Peninsula (West)
        if (wPeninsula > kWeightGate) {
            float qxP = (wx - cPeninsula.x) * kCoordScale;
            float qzP = (wz - cPeninsula.y) * kCoordScale;
            float warpPX = fbmNoise(qxP * 0.018f, qzP * 0.018f, seed_ + 17u, 3) * 14.0f;
            float warpPZ = fbmNoise(qxP * 0.018f + 9.3f, qzP * 0.018f - 4.7f, seed_ + 31u, 3) * 14.0f;
            float pxP = qxP + warpPX;
            float pzP = qzP + warpPZ;
            float detailP = fbmNoise(pxP * 0.032f, pzP * 0.032f, seed_ + 53u, 5);
            float mainlandBias = (pzP * 0.014f) + (-pxP * 0.014f) + 0.30f;
            float bayDx = (pxP + 12.0f) / 28.0f;
            float bayDz = (pzP + 16.0f) / 24.0f;
            float bayCarve = std::exp(-(bayDx * bayDx + bayDz * bayDz) * 1.5f) * 0.85f;
            float isl1Dx = (pxP + 32.0f) / 16.0f;
            float isl1Dz = (pzP + 28.0f) / 14.0f;
            float isl1 = std::exp(-(isl1Dx * isl1Dx + isl1Dz * isl1Dz) * 1.6f) * 0.65f;
            float moundsP = std::pow(std::max(0.0f, fbmNoise(pxP * 0.048f + 5.2f, pzP * 0.048f - 3.1f, seed_ + 89u, 3)), 1.8f) * 0.72f;
            float hPeninsula = (mainlandBias - bayCarve + isl1 + moundsP + detailP * 0.35f) * 5.8f;
            hCore += wPeninsula * hPeninsula;
        }

        // D. Shattered Archipelago (South)
        if (wArchipelago > kWeightGate) {
            float qxA = (wx - cArchipelago.x) * kCoordScale;
            float qzA = (wz - cArchipelago.y) * kCoordScale;
            float warpAX = fbmNoise(qxA * 0.018f, qzA * 0.018f, seed_ + 17u, 3) * 14.0f;
            float warpAZ = fbmNoise(qxA * 0.018f + 9.3f, qzA * 0.018f - 4.7f, seed_ + 31u, 3) * 14.0f;
            float pxA = qxA + warpAX;
            float pzA = qzA + warpAZ;
            float rA = std::sqrt(pxA * pxA + pzA * pzA);
            float ringA = std::exp(-std::pow((rA - 32.0f) / 18.0f, 2.0f)) * 0.65f;
            float centerIsleA = std::exp(-(pxA * pxA + pzA * pzA) / (16.0f * 16.0f)) * 0.80f;
            float moundsA = std::pow(std::max(0.0f, fbmNoise(pxA * 0.05f, pzA * 0.05f, seed_ + 89u, 3)), 1.6f) * 0.55f;
            float archipelagoNoise = fbmNoise(pxA * 0.028f, pzA * 0.028f, seed_ + 113u, 5) * 0.80f;
            float hArchipelago = (ringA + centerIsleA + archipelagoNoise + moundsA - 0.22f) * 5.6f;
            hCore += wArchipelago * hArchipelago;
        }

        // E. Central Crossroads Plateau (Center)
        if (wCenter > kWeightGate) {
            float cDetail = fbmNoise(wx * 0.012f, wz * 0.012f, seed_ + 151u, 3) * 0.55f;
            float cMounds = std::pow(std::max(0.0f, fbmNoise(wx * 0.020f, wz * 0.020f, seed_ + 167u, 2)), 1.5f) * 0.70f;
            float hCenter = 1.75f + cDetail + cMounds;
            hCore += wCenter * hCenter;
        }

        if (rOriginSq <= 210.0f * 210.0f) {
            return hCore;
        }
    }

    // 4. Infinite Procedural Multi-Biome Continuum Beyond the Core Continent
    // Generates continuous continental landmasses, rolling wilderness, mountain ridges, and coastal waters
    float contNoise = fbmNoise(wx * 0.0032f, wz * 0.0032f, seed_ + 331u, 4);
    // Baseline continental bias: roughly 65% land, 35% oceans/straits
    float continentField = contNoise + 0.16f;

    float hProc = 0.0f;
    if (continentField > 0.0f) {
        // Procedural Landmass: Rolling hills, plains, and tectonic mountain ridges
        float baseH = 1.4f + std::pow(continentField, 0.85f) * 1.8f;
        float roll = fbmNoise(wx * 0.014f, wz * 0.014f, seed_ + 607u, 4) * 1.5f;
        float ridge = 1.0f - std::abs(fbmNoise(wx * 0.0075f, wz * 0.0075f, seed_ + 709u, 4));
        ridge = ridge * ridge * ridge;
        float crags = ridge * 6.5f + fbmNoise(wx * 0.028f, wz * 0.028f, seed_ + 733u, 3) * 0.6f;
        hProc = baseH + roll + crags;
    } else {
        // Procedural Waters: Coastal shoals down to deep oceans
        float depth = std::clamp(-continentField * 7.5f, 0.0f, 3.8f);
        hProc = -0.5f - depth;
    }

    if (rOriginSq >= 280.0f * 280.0f) {
        return hProc;
    }

    float rOrigin = std::sqrt(rOriginSq);
    float t = (rOrigin - 210.0f) / 70.0f;
    float sCurve = t * t * (3.0f - 2.0f * t);
    return hCore * (1.0f - sCurve) + hProc * sCurve;
}

float VoronoiAtlas::rawMoistureContinental(float wx, float wz) const noexcept {
    const float rOriginSq = wx * wx + wz * wz;

    float mCore = 0.0f;
    if (rOriginSq < 280.0f * 280.0f) {
        const Vec2f cHighlands{0.0f, 190.0f};
        const Vec2f cDesert{190.0f, 0.0f};
        const Vec2f cPeninsula{-190.0f, 0.0f};
        const Vec2f cArchipelago{0.0f, -190.0f};
        const Vec2f cCrossroads{0.0f, 0.0f};

        const float sigmaProv = 110.0f;
        const float sigmaCenter = 75.0f;
        const float inv2SigmaProvSq = 1.0f / (2.0f * sigmaProv * sigmaProv);
        const float inv2SigmaCenterSq = 1.0f / (2.0f * sigmaCenter * sigmaCenter);

        float dSqHighlands = (wx - cHighlands.x) * (wx - cHighlands.x) + (wz - cHighlands.y) * (wz - cHighlands.y);
        float dSqDesert = (wx - cDesert.x) * (wx - cDesert.x) + (wz - cDesert.y) * (wz - cDesert.y);
        float dSqPeninsula = (wx - cPeninsula.x) * (wx - cPeninsula.x) + (wz - cPeninsula.y) * (wz - cPeninsula.y);
        float dSqArchipelago = (wx - cArchipelago.x) * (wx - cArchipelago.x) + (wz - cArchipelago.y) * (wz - cArchipelago.y);
        float dSqCenter = (wx - cCrossroads.x) * (wx - cCrossroads.x) + (wz - cCrossroads.y) * (wz - cCrossroads.y);

        float wHighlands = std::exp(-dSqHighlands * inv2SigmaProvSq);
        float wDesert = std::exp(-dSqDesert * inv2SigmaProvSq);
        float wPeninsula = std::exp(-dSqPeninsula * inv2SigmaProvSq);
        float wArchipelago = std::exp(-dSqArchipelago * inv2SigmaProvSq);
        float wCenter = std::exp(-dSqCenter * inv2SigmaCenterSq);

        float totalW = wHighlands + wDesert + wPeninsula + wArchipelago + wCenter;
        if (totalW > 1e-5f) {
            float invW = 1.0f / totalW;
            wHighlands *= invW;
            wDesert *= invW;
            wPeninsula *= invW;
            wArchipelago *= invW;
            wCenter *= invW;
        } else {
            wCenter = 1.0f;
        }

        constexpr float kCoordScale = 0.42f;
        constexpr float kWeightGate = 1e-3f;
        if (wHighlands > kWeightGate) {
            float mHighlands = 0.72f + fbmNoise(wx * 0.010f, wz * 0.010f, seed_ + 211u, 3) * 0.15f;
            mCore += wHighlands * mHighlands;
        }
        if (wDesert > kWeightGate) {
            float qxD = (wx - cDesert.x) * kCoordScale;
            float qzD = (wz - cDesert.y) * kCoordScale;
            float mDesert = rawMoistureDesert(cDesert.x + qxD, cDesert.y + qzD, cDesert, 0.0f);
            mCore += wDesert * mDesert;
        }
        if (wPeninsula > kWeightGate) {
            float mPeninsula = 0.58f + fbmNoise(wx * 0.010f, wz * 0.010f, seed_ + 223u, 3) * 0.15f;
            mCore += wPeninsula * mPeninsula;
        }
        if (wArchipelago > kWeightGate) {
            float mArchipelago = 0.62f + fbmNoise(wx * 0.010f, wz * 0.010f, seed_ + 239u, 3) * 0.15f;
            mCore += wArchipelago * mArchipelago;
        }
        if (wCenter > kWeightGate) {
            float mCenter = 0.52f + fbmNoise(wx * 0.010f, wz * 0.010f, seed_ + 251u, 3) * 0.12f;
            mCore += wCenter * mCenter;
        }

        if (rOriginSq <= 210.0f * 210.0f) {
            return std::clamp(mCore, 0.0f, 1.0f);
        }
    }

    // Procedural continuum moisture: continental rain patterns + climate gradients
    float mNoise = fbmNoise(wx * 0.0038f + 19.0f, wz * 0.0038f - 11.0f, seed_ + 503u, 3);
    float mProc = std::clamp(0.52f + mNoise * 0.40f, 0.08f, 0.92f);

    if (rOriginSq >= 280.0f * 280.0f) {
        return std::clamp(mProc, 0.0f, 1.0f);
    }

    float rOrigin = std::sqrt(rOriginSq);
    float t = (rOrigin - 210.0f) / 70.0f;
    float sCurve = t * t * (3.0f - 2.0f * t);
    return std::clamp(mCore * (1.0f - sCurve) + mProc * sCurve, 0.0f, 1.0f);
}

void VoronoiAtlas::addWindingRiver(Vec2f start, Vec2f mid, Vec2f mouth, float startWidth, float endWidth) {
    constexpr int kSteps = 18;
    Vec2f prev = start;
    for (int i = 1; i <= kSteps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kSteps);
        float u = 1.0f - t;
        Vec2f pt{
            u * u * start.x + 2.0f * u * t * mid.x + t * t * mouth.x,
            u * u * start.y + 2.0f * u * t * mid.y + t * t * mouth.y
        };
        float meander = std::sin(t * 12.56f + static_cast<float>(seed_ % 7u)) * (1.0f - t * 0.5f) * 2.8f;
        pt.x += meander;
        float w0 = startWidth + (endWidth - startWidth) * (static_cast<float>(i - 1) / static_cast<float>(kSteps));
        float w1 = startWidth + (endWidth - startWidth) * t;
        RiverSegment seg{prev, pt, w0, w1};
        seg.minX = std::min(prev.x, pt.x) - 6.0f;
        seg.maxX = std::max(prev.x, pt.x) + 6.0f;
        seg.minZ = std::min(prev.y, pt.y) - 6.0f;
        seg.maxZ = std::max(prev.y, pt.y) + 6.0f;
        rivers_.push_back(seg);
        indexRiverSegment(static_cast<uint32_t>(rivers_.size() - 1));
        prev = pt;
    }
}

void VoronoiAtlas::indexRiverSegment(uint32_t segIdx) {
    const auto& rv = rivers_[segIdx];
    int minCX = static_cast<int>(std::floor(rv.minX / kRiverSpatialCell));
    int maxCX = static_cast<int>(std::floor(rv.maxX / kRiverSpatialCell));
    int minCZ = static_cast<int>(std::floor(rv.minZ / kRiverSpatialCell));
    int maxCZ = static_cast<int>(std::floor(rv.maxZ / kRiverSpatialCell));

    for (int cz = minCZ; cz <= maxCZ; ++cz) {
        for (int cx = minCX; cx <= maxCX; ++cx) {
            riverSpatialGrid_[packBlockCoord(cx, cz)].push_back(segIdx);
        }
    }
}

void VoronoiAtlas::traceHydrologicalRiver(Vec2f source, float startWidth, float terminalElevation, float maxReach) {
    Vec2f curr = source;
    float currH = rawElevationAt(curr.x, curr.y);
    if (currH <= terminalElevation) return;

    constexpr float kStep = 3.5f;
    Vec2f momentum{0.0f, 0.0f};
    float totalDist = 0.0f;
    int maxSteps = static_cast<int>(maxReach / kStep);

    for (int step = 0; step < maxSteps; ++step) {
        constexpr float kEps = 0.8f;
        float hL = rawElevationAt(curr.x - kEps, curr.y);
        float hR = rawElevationAt(curr.x + kEps, curr.y);
        float hD = rawElevationAt(curr.x, curr.y - kEps);
        float hU = rawElevationAt(curr.x, curr.y + kEps);
        float gx = (hR - hL) * 0.5f;
        float gz = (hU - hD) * 0.5f;
        float gradLen = std::sqrt(gx * gx + gz * gz);

        Vec2f downhill{0.0f, 0.0f};
        if (gradLen > 0.015f) {
            downhill = {-gx / gradLen, -gz / gradLen};
        } else {
            // Local flat or saddle: probe 8 surrounding samples to escape micro-depression
            float bestH = currH;
            Vec2f bestDir{0.0f, 0.0f};
            constexpr int kAngles = 8;
            for (int a = 0; a < kAngles; ++a) {
                float theta = static_cast<float>(a) * (6.2831853f / static_cast<float>(kAngles));
                Vec2f probe{curr.x + std::cos(theta) * 6.0f, curr.y + std::sin(theta) * 6.0f};
                float ph = rawElevationAt(probe.x, probe.y);
                if (ph < bestH) {
                    bestH = ph;
                    bestDir = {std::cos(theta), std::sin(theta)};
                }
            }
            if (bestH < currH) {
                downhill = bestDir;
            } else if (momentum.x * momentum.x + momentum.y * momentum.y > 0.01f) {
                downhill = momentum;
            } else {
                // Genuine lake / endorheic basin: river terminates into lake pool
                break;
            }
        }

        // Apply inertia/momentum to smooth natural course
        momentum.x = momentum.x * 0.40f + downhill.x * 0.60f;
        momentum.y = momentum.y * 0.40f + downhill.y * 0.60f;
        float mLen = std::sqrt(momentum.x * momentum.x + momentum.y * momentum.y);
        Vec2f dir = (mLen > 0.001f) ? Vec2f{momentum.x / mLen, momentum.y / mLen} : downhill;

        // Dynamic meander: higher sinuosity in flat lowlands, straighter down steep slopes
        float slopeFactor = std::clamp(gradLen / 0.28f, 0.0f, 1.0f);
        float meanderAmp = (1.0f - slopeFactor) * 0.55f;
        float meander = std::sin(totalDist * 0.075f + static_cast<float>((seed_ + step) % 11u)) * meanderAmp;
        Vec2f perp{-dir.y, dir.x};
        Vec2f stepDir{dir.x + perp.x * meander, dir.y + perp.y * meander};
        float sLen = std::sqrt(stepDir.x * stepDir.x + stepDir.y * stepDir.y);
        if (sLen > 0.001f) {
            stepDir.x /= sLen;
            stepDir.y /= sLen;
        }

        Vec2f nextPt{curr.x + stepDir.x * kStep, curr.y + stepDir.y * kStep};
        float nextH = rawElevationAt(nextPt.x, nextPt.y);

        // Leopold-Maddock hydraulic width progression: w(d) = w0 + k * sqrt(d)
        float w0 = startWidth + 0.16f * std::sqrt(totalDist);
        float w1 = startWidth + 0.16f * std::sqrt(totalDist + kStep);
        w0 = std::min(w0, 5.2f);
        w1 = std::min(w1, 5.2f);

        // As it approaches the sea, widen into an estuary delta
        if (nextH <= 0.25f) {
            float estuaryFactor = std::max(0.0f, 1.0f - (nextH / 0.25f));
            w1 = std::max(w1, 3.6f + estuaryFactor * 2.2f);
        }

        RiverSegment seg{curr, nextPt, w0, w1};
        float maxW = std::max(w0, w1) * 2.6f + 3.0f;
        seg.minX = std::min(curr.x, nextPt.x) - maxW;
        seg.maxX = std::max(curr.x, nextPt.x) + maxW;
        seg.minZ = std::min(curr.y, nextPt.y) - maxW;
        seg.maxZ = std::max(curr.y, nextPt.y) + maxW;

        uint32_t segIdx = static_cast<uint32_t>(rivers_.size());
        rivers_.push_back(seg);
        indexRiverSegment(segIdx);

        // Terminate at sea level (drained to open ocean/sea)
        if (nextH <= terminalElevation) {
            // Push one delta mouth segment extending into the shallow sea
            Vec2f oceanPt{nextPt.x + stepDir.x * kStep * 1.2f, nextPt.y + stepDir.y * kStep * 1.2f};
            RiverSegment deltaSeg{nextPt, oceanPt, w1, w1 * 1.35f};
            float deltaW = w1 * 1.35f * 2.6f + 3.0f;
            deltaSeg.minX = std::min(nextPt.x, oceanPt.x) - deltaW;
            deltaSeg.maxX = std::max(nextPt.x, oceanPt.x) + deltaW;
            deltaSeg.minZ = std::min(nextPt.y, oceanPt.y) - deltaW;
            deltaSeg.maxZ = std::max(nextPt.y, oceanPt.y) + deltaW;
            uint32_t deltaIdx = static_cast<uint32_t>(rivers_.size());
            rivers_.push_back(deltaSeg);
            indexRiverSegment(deltaIdx);
            break;
        }

        curr = nextPt;
        currH = nextH;
        totalDist += kStep;
    }
}

void VoronoiAtlas::addCurvedRoad(Vec2f a, Vec2f ctrl, Vec2f b, bool isHighway) {
    constexpr int kSegs = 14;
    Vec2f prev = a;
    float accumArc = 0.0f;
    for (int i = 1; i <= kSegs; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kSegs);
        float u = 1.0f - t;
        Vec2f pt{
            u * u * a.x + 2.0f * u * t * ctrl.x + t * t * b.x,
            u * u * a.y + 2.0f * u * t * ctrl.y + t * t * b.y
        };
        float segLen = std::sqrt((pt.x - prev.x) * (pt.x - prev.x) + (pt.y - prev.y) * (pt.y - prev.y));
        RoadSegment seg{prev, pt, accumArc, accumArc + segLen, isHighway};
        seg.minX = std::min(prev.x, pt.x) - 5.0f;
        seg.maxX = std::max(prev.x, pt.x) + 5.0f;
        seg.minZ = std::min(prev.y, pt.y) - 5.0f;
        seg.maxZ = std::max(prev.y, pt.y) + 5.0f;
        roads_.push_back(seg);
        accumArc += segLen;
        prev = pt;
    }
}

void VoronoiAtlas::generateOverworldGraph() {
    // 1. Generate relaxed Voronoi sites across domain
    const int kGridN = (preset_ == AtlasPreset::ContinentalPangaea) ? 24 : 12;
    const float kDomainMin = (preset_ == AtlasPreset::ContinentalPangaea) ? -240.0f : -96.0f;
    const float kDomainMax = (preset_ == AtlasPreset::ContinentalPangaea) ?  240.0f :  96.0f;
    const float kCellStep = (kDomainMax - kDomainMin) / static_cast<float>(kGridN);

    cells_.reserve(kGridN * kGridN);
    int idCounter = 0;
    for (int gz = 0; gz < kGridN; ++gz) {
        for (int gx = 0; gx < kGridN; ++gx) {
            float jx = (hashFloat(gx, gz, seed_ + 1u) - 0.5f) * kCellStep * 0.72f;
            float jz = (hashFloat(gx, gz, seed_ + 2u) - 0.5f) * kCellStep * 0.72f;
            VoronoiRegionCell cell{};
            cell.id = idCounter++;
            cell.site = {
                kDomainMin + (static_cast<float>(gx) + 0.5f) * kCellStep + jx,
                kDomainMin + (static_cast<float>(gz) + 0.5f) * kCellStep + jz
            };
            cells_.push_back(cell);
        }
    }

    // Build approximate Delaunay adjacency (8-neighborhood in jittered grid + distance threshold)
    for (int gz = 0; gz < kGridN; ++gz) {
        for (int gx = 0; gx < kGridN; ++gx) {
            int idx = gz * kGridN + gx;
            for (int dz = -1; dz <= 1; ++dz) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dz == 0) continue;
                    int nx = gx + dx;
                    int nz = gz + dz;
                    if (nx >= 0 && nx < kGridN && nz >= 0 && nz < kGridN) {
                        cells_[idx].neighbors.push_back(nz * kGridN + nx);
                    }
                }
            }
        }
    }

    // Evaluate cell elevations, moisture, and downhill topology
    for (auto& c : cells_) {
        c.elevation = rawElevationAt(c.site.x, c.site.y);
        c.moisture  = rawMoistureAt(c.site.x, c.site.y);
        c.isWater   = (c.elevation <= 0.0f);
    }

    for (auto& c : cells_) {
        float minDistToFlip = 200.0f;
        for (const auto& other : cells_) {
            if (other.isWater != c.isWater) {
                float dx = c.site.x - other.site.x;
                float dy = c.site.y - other.site.y;
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < minDistToFlip) minDistToFlip = d;
            }
        }
        c.coastDist = c.isWater ? -minDistToFlip : minDistToFlip;
        c.biome = classifyBiome(c.elevation, c.moisture, std::abs(c.coastDist), hashU32(static_cast<uint32_t>(c.id) + seed_));

        float lowestElev = c.elevation;
        int lowestNbr = -1;
        for (int nIdx : c.neighbors) {
            if (cells_[nIdx].elevation < lowestElev) {
                lowestElev = cells_[nIdx].elevation;
                lowestNbr = nIdx;
            }
        }
        c.downhillNeighbor = lowestNbr;
    }

    // 2. Synthesize Watabou-style Winding Rivers from high hills down to the coast
    if (preset_ == AtlasPreset::ContinentalPangaea) {
        // The Great Silver River: Northern snowmelt peaks descending through Highland valley to the Crossroads
        addWindingRiver({-10.0f, 235.0f}, {0.0f, 120.0f}, {0.0f, 20.0f}, 1.8f, 3.6f);
        // Western River Fork: Crossroads through Kildland meadows into Lowmillburg Bay
        addWindingRiver({0.0f, 20.0f}, {-90.0f, 10.0f}, {-205.0f, -32.0f}, 2.2f, 4.4f);
        // Southern River Fork: Crossroads down to the coastal delta into the Shattered Sea
        addWindingRiver({0.0f, 20.0f}, {10.0f, -75.0f}, {15.0f, -195.0f}, 2.2f, 4.8f);
        // High Alpine Glacier Tributary: Northwest Silvervale peak descending to meet Great Silver River
        traceHydrologicalRiver({-35.0f, 230.0f}, 1.0f, 0.05f, 200.0f);
        // Wadi Al-Zahra: Dry desert arroyo carving from northeast dunes into oasis depression
        addWindingRiver({230.0f, 42.0f}, {210.0f, 8.0f}, {188.0f, -12.0f}, 0.8f, 1.8f);
    } else if (preset_ == AtlasPreset::SunscorchedWastes) {
        addWindingRiver({16.0f, -3.0f}, {12.0f, -6.0f}, {9.0f, -8.0f}, 0.5f, 1.1f);
    } else
    if (preset_ == AtlasPreset::PerilousPeninsula) {
        // Meandering river starting in alpine mountains past Shattered Palace and Dragon Desert to the sea
        addWindingRiver({-12.0f, 6.0f}, {8.0f, -2.0f}, {32.0f, -8.0f}, 0.85f, 2.2f);
    } else if (preset_ == AtlasPreset::HighlandKingdom) {
        addWindingRiver({-4.0f, 70.0f}, {2.0f, 10.0f},   {-6.0f, -55.0f}, 1.2f, 3.2f);
        addWindingRiver({-44.0f, 38.0f}, {-20.0f, 14.0f},{-1.0f, 4.0f},   0.8f, 1.6f);
    } else {
        addWindingRiver({4.0f, 16.0f},  {0.0f, 4.0f},    {-10.0f, -12.0f},0.7f, 1.6f);
    }

    // 3. Place Cat Quest & Perilous Shores Landmarks / Settlements (POIs - Reference: Kildland Map)
    auto addPoi = [&](PoiCategory cat, float x, float z, const char* name, const char* sub, int lvl) {
        ChartPoi p{};
        p.category   = cat;
        p.pos        = {x, z};
        p.elevation  = std::max(0.4f, rawElevationAt(x, z));
        p.name       = name;
        p.subtitle   = sub;
        p.seed       = hashU32(static_cast<uint32_t>(pois_.size()) + seed_);
        p.questLevel = lvl;
        pois_.push_back(p);
    };

    if (preset_ == AtlasPreset::ContinentalPangaea) {
        // 1. Highland Kingdom (North Sector)
        addPoi(PoiCategory::CitadelKeep,      0.0f,  195.0f, "HIGHGUARD KEEP",    "Twin-Bridge Mountain Fortress", 3);
        addPoi(PoiCategory::MarketHamlet,   -45.0f,  135.0f, "MEADOWBROOK",       "Valley River Trading Post",     1);
        addPoi(PoiCategory::AlpineMonastery, 60.0f,  230.0f, "FROSTPEAK EYRIE",   "Cloud-Capped Astrologer Spire", 6);
        addPoi(PoiCategory::DungeonPortal,  -65.0f,  210.0f, "BLACKHOLLOW DELVE", "Lv. 5 Deep Mine Portal",       5);

        // 2. The Perilous Peninsula (West Sector)
        addPoi(PoiCategory::CitadelKeep,   -175.0f,  -10.0f, "LOWMILLBURG",       "Western Walled Castle & Port",  3);
        addPoi(PoiCategory::Windmill,      -190.0f,  -20.0f, "LOWMILL GRANGE",    "Farmland Grain Terraces",       1);
        addPoi(PoiCategory::Windmill,      -162.0f,  -22.0f, "WESTVALE MILL",     "Coastal Valley Windmill",       1);
        addPoi(PoiCategory::MarketHamlet,  -145.0f,   55.0f, "COLDHEARTFIELD",    "Terracotta Hamlet & Fields",    2);
        addPoi(PoiCategory::PortHarbor,    -125.0f,  -55.0f, "STORMPOOL",         "Cape Harbor & Anchorage",       2);
        addPoi(PoiCategory::Lighthouse,    -112.0f,  -50.0f, "STORMPOOL BEACON",  "Watchtower of the Shoals",      2);
        addPoi(PoiCategory::DungeonPortal, -205.0f,   35.0f, "SHATTERED PALACE",  "Ancient Sunken Necropolis",     4);
        addPoi(PoiCategory::AlpineMonastery,-180.0f, 110.0f, "STARFALL ABBEY",    "Highland Astrologer Spire",     5);

        // 3. The Sunscorched Wastes (East Sector)
        addPoi(PoiCategory::MarketHamlet,   180.0f,  -10.0f, "AL-ZAHRA OASIS",    "Emerald Spring & Caravanserai", 1);
        addPoi(PoiCategory::DungeonPortal,  215.0f,   65.0f, "TOMB OF SAND KINGS","Lv. 6 Sunken Sepulcher",       6);
        addPoi(PoiCategory::CitadelKeep,    160.0f,   80.0f, "DUSTDEVIL OUTPOST", "Dune Garrison & Watchpost",    3);
        addPoi(PoiCategory::AlpineMonastery,135.0f,  -55.0f, "SUNWATCH MINARET",  "Desert Astrologer Spire",      4);
        addPoi(PoiCategory::StandingStones, 170.0f,  -80.0f, "THE CRACKED ARCH",  "Sun-Baked Monoliths",           2);
        addPoi(PoiCategory::BossLair,       235.0f,  -70.0f, "SCORPION'S GULCH",  "Lv. 8 Chitin Behemoth Lair",   8);

        // 4. The Shattered Archipelago (South Sector)
        addPoi(PoiCategory::PortHarbor,       0.0f, -190.0f, "CORSAIR'S HAVEN",   "Pirate Atoll Anchorage",       2);
        addPoi(PoiCategory::Lighthouse,     -70.0f, -220.0f, "STORMGLASS TOWER",  "Western Reef Beacon",          3);
        addPoi(PoiCategory::DungeonPortal,   65.0f, -150.0f, "ABYSSAL GROTTO",    "Lv. 5 Flooded Vault",          5);
        addPoi(PoiCategory::SunkenRuin,     -55.0f, -135.0f, "SIREN'S ARCH",      "Coral-Draped Temple",          4);
        addPoi(PoiCategory::BossLair,        80.0f, -230.0f, "LEVIATHAN REEF",    "Lv. 10 Mauler of the Deep",   10);

        // 5. The Imperial Crossroads (Center)
        addPoi(PoiCategory::MarketHamlet,     0.0f,    0.0f, "IMPERIAL CROSSROADS","Grand Trade Center & Inn",     1);
    } else if (preset_ == AtlasPreset::SunscorchedWastes) {
        std::string oasisName  = (seed_ == 1066u) ? "AL-ZAHRA OASIS" : ProceduralNameGenerator::generateLocaleName(seed_ + 1u, BiomeId::DeciduousWoodland, PoiCategory::MarketHamlet);
        std::string tombName   = (seed_ == 1066u) ? "TOMB OF THE SAND KINGS" : ProceduralNameGenerator::generateLocaleName(seed_ + 2u, BiomeId::BadlandsCanyon, PoiCategory::DungeonPortal);
        std::string outName    = (seed_ == 1066u) ? "DUSTDEVIL OUTPOST" : ProceduralNameGenerator::generateLocaleName(seed_ + 3u, BiomeId::AridSteppe, PoiCategory::CitadelKeep);
        std::string spireName  = (seed_ == 1066u) ? "SUNWATCH MINARET" : ProceduralNameGenerator::generateLocaleName(seed_ + 4u, BiomeId::BadlandsCanyon, PoiCategory::AlpineMonastery);

        addPoi(PoiCategory::MarketHamlet,    12.0f,  -6.0f, oasisName.c_str(),  "Emerald Spring & Caravanserai", 1);
        addPoi(PoiCategory::DungeonPortal,   24.0f,  26.0f, tombName.c_str(),   "Lv. 6 Sunken Sepulcher",        6);
        addPoi(PoiCategory::CitadelKeep,     -6.0f,  32.0f, outName.c_str(),    "Northern Dune Garrison",        3);
        addPoi(PoiCategory::AlpineMonastery,-28.0f, -22.0f, spireName.c_str(),  "Desert Astrologer Spire",       4);
        addPoi(PoiCategory::StandingStones, -18.0f,   8.0f, "THE CRACKED ARCH", "Ancient Sun-Baked Monoliths",   2);
        addPoi(PoiCategory::BossLair,        32.0f, -28.0f, "SCORPION'S GULCH", "Lv. 8 Chitin Behemoth Lair",    8);
    } else if (preset_ == AtlasPreset::PerilousPeninsula) {
        std::string townName   = (seed_ == 1492u) ? "LOWMILLBURG" : ProceduralNameGenerator::generateLocaleName(seed_ + 1u, BiomeId::LushMeadow, PoiCategory::CitadelKeep);
        std::string hamletName = (seed_ == 1492u) ? "COLDHEARTFIELD" : ProceduralNameGenerator::generateLocaleName(seed_ + 2u, BiomeId::LushMeadow, PoiCategory::MarketHamlet);
        std::string portName   = (seed_ == 1492u) ? "STORMPOOL" : ProceduralNameGenerator::generateLocaleName(seed_ + 3u, BiomeId::CoastalShoals, PoiCategory::PortHarbor);
        std::string dungeon1   = (seed_ == 1492u) ? "SHATTERED PALACE" : ProceduralNameGenerator::generateLocaleName(seed_ + 4u, BiomeId::AlpinePeak, PoiCategory::DungeonPortal);
        std::string dungeon2   = (seed_ == 1492u) ? "FORT OF THE UNDEAD-ONE" : ProceduralNameGenerator::generateLocaleName(seed_ + 5u, BiomeId::CoastalShoals, PoiCategory::DungeonPortal);

        addPoi(PoiCategory::CitadelKeep,      6.0f, -34.0f, townName.c_str(),         "Southern Walled Citadel & Castle", 3);
        addPoi(PoiCategory::Windmill,         1.0f, -38.0f, "LOWMILL GRANGE",         "Farmland Grain Terraces",          1);
        addPoi(PoiCategory::Windmill,        11.0f, -38.0f, "WESTVALE MILL",          "Valley Windmill",                  1);
        addPoi(PoiCategory::MarketHamlet,    18.0f,  12.0f, hamletName.c_str(),       "Terracotta Hamlet & Fields",       2);
        addPoi(PoiCategory::PortHarbor,      32.0f, -12.0f, portName.c_str(),         "Eastern Nautical Cape & Harbor",   1);
        addPoi(PoiCategory::Lighthouse,      37.0f, -10.0f, "STORMPOOL BEACON",      "Watchtower of the Eastern Shoals", 2);
        addPoi(PoiCategory::DungeonPortal,  -14.0f,   4.0f, dungeon1.c_str(),         "Ancient Sunken Necropolis",        4);
        addPoi(PoiCategory::DungeonPortal,  -26.0f, -16.0f, dungeon2.c_str(),         "Coastal Fortress of Whispers",    5);
        addPoi(PoiCategory::AlpineMonastery, -2.0f,  46.0f, "STARFALL ABBEY",         "Highland Astrologer Spire",        5);
        addPoi(PoiCategory::SunkenRuin,      26.0f, -32.0f, "DROWNED OBELISK",        "Shattered Sea-King Ruins",         6);
    } else if (preset_ == AtlasPreset::ShatteredArchipelago) {
        addPoi(PoiCategory::PortHarbor,       0.0f,  -6.0f, "CORSAIR'S HAVEN",   "Central Atoll Anchorage",     2);
        addPoi(PoiCategory::Lighthouse,     -32.0f, -18.0f, "STORMGLASS TOWER",  "Western Reef Beacon",         3);
        addPoi(PoiCategory::DungeonPortal,   30.0f,  22.0f, "ABYSSAL GROTTO",    "Lv. 5 Flooded Vault",         5);
        addPoi(PoiCategory::SunkenRuin,     -24.0f,  28.0f, "SIREN'S ARCH",      "Coral-Draped Temple",         4);
        addPoi(PoiCategory::BossLair,        34.0f, -24.0f, "LEVIATHAN REEF",    "Lv. 10 Mauler of the Deep",  10);
    } else {
        addPoi(PoiCategory::CitadelKeep,      0.0f,   6.0f, "HIGHGUARD KEEP",    "Twin-Bridge Valley Fortress", 3);
        addPoi(PoiCategory::MarketHamlet,   -22.0f, -16.0f, "MEADOWBROOK",       "River Crossing Trading Post", 1);
        addPoi(PoiCategory::AlpineMonastery, 32.0f,  36.0f, "FROSTPEAK EYRIE",   "Cloud-Capped Sanctuary",      6);
        addPoi(PoiCategory::DungeonPortal,  -30.0f,  28.0f, "BLACKHOLLOW DELVE", "Lv. 5 Deep Mine Portal",      5);
    }

    // 4. Synthesize Spline Trade Roads & Dashed Wilderness Trails between POIs
    if (preset_ == AtlasPreset::ContinentalPangaea) {
        // Imperial Highway North: Highguard Keep -> Crossroads
        addCurvedRoad({0.0f, 195.0f},  {0.0f, 100.0f},   {0.0f, 0.0f},   true);
        // Imperial Highway South: Crossroads -> Southern Ferry Cape
        addCurvedRoad({0.0f, 0.0f},    {0.0f, -65.0f},   {0.0f, -130.0f}, true);

        // Imperial Highway West: Lowmillburg -> Crossroads
        addCurvedRoad({-175.0f, -10.0f}, {-88.0f, -5.0f},  {0.0f, 0.0f},   true);
        // Imperial Highway East: Crossroads -> Al-Zahra Oasis
        addCurvedRoad({0.0f, 0.0f},    {90.0f, -5.0f},   {180.0f, -10.0f}, true);

        // Highland Trail: Highguard Keep -> Frostpeak Eyrie -> Whispering Pass -> Dustdevil Outpost
        addCurvedRoad({0.0f, 195.0f},  {30.0f, 212.0f},  {60.0f, 230.0f}, false);
        addCurvedRoad({60.0f, 230.0f}, {110.0f, 155.0f}, {160.0f, 80.0f}, false);

        // Peninsula Coastal Road: Lowmillburg -> Westvale Mill -> Stormpool
        addCurvedRoad({-175.0f, -10.0f},{-170.0f, -18.0f}, {-162.0f, -22.0f}, true);
        addCurvedRoad({-162.0f, -22.0f},{-145.0f, -40.0f}, {-125.0f, -55.0f}, false);

        // Peninsula Northern Road: Lowmillburg -> Coldheartfield -> Starfall Abbey
        addCurvedRoad({-175.0f, -10.0f},{-160.0f, 22.0f},  {-145.0f, 55.0f}, false);
        addCurvedRoad({-145.0f, 55.0f}, {-162.0f, 82.0f},  {-180.0f, 110.0f}, false);

        // Desert Pilgrimage Trails:
        addCurvedRoad({180.0f, -10.0f}, {197.0f, 27.0f},  {215.0f, 65.0f}, false);
        addCurvedRoad({180.0f, -10.0f}, {157.0f, -32.0f}, {135.0f, -55.0f}, false);
        addCurvedRoad({135.0f, -55.0f}, {152.0f, -68.0f}, {170.0f, -80.0f}, false);

        // Southern Sea Channel (Ferry Cape -> Corsair's Haven)
        addCurvedRoad({0.0f, -130.0f}, {0.0f, -160.0f},  {0.0f, -190.0f}, false);
    } else if (preset_ == AtlasPreset::SunscorchedWastes) {
        // Northern garrison down to Al-Zahra Oasis
        addCurvedRoad({-6.0f, 32.0f},  {4.0f, 14.0f},    {12.0f, -6.0f},  false);
        // Al-Zahra Oasis southwest to Sunwatch Minaret
        addCurvedRoad({12.0f, -6.0f},  {-8.0f, -14.0f},  {-28.0f, -22.0f},false);
        // Al-Zahra Oasis northeast ascending to Tomb of the Sand Kings
        addCurvedRoad({12.0f, -6.0f},  {19.0f, 10.0f},   {24.0f, 26.0f},  false);
        // Desert trade trail past the Cracked Arch
        addCurvedRoad({-28.0f, -22.0f},{-22.0f, -6.0f},  {-18.0f, 8.0f},  false);
    } else if (preset_ == AtlasPreset::PerilousPeninsula) {
        // Reference Dotted Crimson Route: Coldheartfield -> Stormpool -> Lowmillburg
        addCurvedRoad({18.0f, 12.0f},  {26.0f, 0.0f},   {32.0f, -12.0f}, false);
        addCurvedRoad({32.0f, -12.0f}, {22.0f, -24.0f}, {6.0f, -34.0f},  false);
        // Wilderness trail to Shattered Palace and Fort Of The Undead-One
        addCurvedRoad({18.0f, 12.0f},  {2.0f, 8.0f},    {-14.0f, 4.0f},  false);
        addCurvedRoad({-14.0f, 4.0f},  {-20.0f, -4.0f}, {-26.0f, -16.0f},false);
        // Royal highway around Lowmillburg castle & windmills
        addCurvedRoad({6.0f, -34.0f},  {1.0f, -38.0f},  {11.0f, -38.0f}, true);
    } else if (preset_ == AtlasPreset::HighlandKingdom) {
        addCurvedRoad({-22.0f, -16.0f},{-10.0f, -4.0f},{0.0f, 6.0f},   true);
        addCurvedRoad({0.0f, 6.0f},    {18.0f, 20.0f}, {32.0f, 36.0f}, false);
        addCurvedRoad({0.0f, 6.0f},    {-16.0f, 18.0f},{-30.0f, 28.0f},false);
    }

    // 5. Place Curved Calligraphic Map Banners (Watabou Perilous Shores + Cat Quest region titles)
    if (preset_ == AtlasPreset::ContinentalPangaea) {
        // Northern Highlands Grand Banner
        banners_.push_back(CurvedMapBanner{
            "THE REALM OF SILVERVALE",
            {-70.0f, 255.0f}, {0.0f, 265.0f}, {70.0f, 255.0f},
            4.2f, true, false
        });
        // Eastern Desert Grand Banner
        banners_.push_back(CurvedMapBanner{
            "THE SUNSCORCHED EXPANSE",
            {130.0f, 115.0f}, {190.0f, 125.0f}, {250.0f, 115.0f},
            4.0f, true, false
        });
        // Western Peninsula Grand Banner
        banners_.push_back(CurvedMapBanner{
            "KILDLAND & THE PERILOUS SHORES",
            {-240.0f, 80.0f}, {-180.0f, 90.0f}, {-120.0f, 80.0f},
            4.0f, true, false
        });
        // Southern Archipelago Grand Banner (isWaterTitle = true)
        banners_.push_back(CurvedMapBanner{
            "THE SHATTERED SEA",
            {-90.0f, -245.0f}, {0.0f, -260.0f}, {90.0f, -245.0f},
            4.2f, true, true
        });
        // Central Imperial Crossroads Banner
        banners_.push_back(CurvedMapBanner{
            "THE IMPERIAL CROSSROADS",
            {-40.0f, -18.0f}, {0.0f, -25.0f}, {40.0f, -18.0f},
            3.2f, true, false
        });
    } else if (preset_ == AtlasPreset::SunscorchedWastes) {
        banners_.push_back(CurvedMapBanner{
            "THE SUNSCORCHED EXPANSE",
            {-28.0f, 44.0f}, {0.0f, 52.0f}, {28.0f, 44.0f},
            2.6f, true, false
        });
        banners_.push_back(CurvedMapBanner{
            "AL-ZAHRA OASIS",
            {2.0f, -15.0f}, {12.0f, -17.5f}, {22.0f, -15.0f},
            1.6f, false, false
        });
        banners_.push_back(CurvedMapBanner{
            "TOMB OF THE SAND KINGS",
            {14.0f, 34.0f}, {24.0f, 36.5f}, {34.0f, 34.0f},
            1.5f, false, false
        });
    } else if (preset_ == AtlasPreset::PerilousPeninsula) {
        if (seed_ == 1492u) {
            banners_.push_back(CurvedMapBanner{
                "KILDLAND",
                {24.0f, 50.0f}, {40.0f, 54.0f}, {56.0f, 50.0f},
                2.8f, true, false
            });
            banners_.push_back(CurvedMapBanner{
                "DUSKWOODS",
                {-24.0f, 26.0f}, {-14.0f, 30.0f}, {-4.0f, 26.0f},
                2.6f, true, false
            });
            banners_.push_back(CurvedMapBanner{
                "DRAGON DESERT",
                {10.0f, -4.0f}, {22.0f, 0.0f}, {34.0f, -4.0f},
                2.6f, true, false
            });
            banners_.push_back(CurvedMapBanner{
                "THE PERILOUS SHORES",
                {-32.0f, -20.0f}, {-6.0f, -24.0f}, {20.0f, -20.0f},
                2.6f, true, true
            });
        } else {
            std::string realmName  = ProceduralNameGenerator::generateRegionTitle(seed_, BiomeId::LushMeadow);
            std::string forestName = ProceduralNameGenerator::generateRegionTitle(seed_ + 17u, BiomeId::DeciduousWoodland);
            std::string desertName = ProceduralNameGenerator::generateRegionTitle(seed_ + 37u, BiomeId::AridSteppe);

            banners_.push_back(CurvedMapBanner{
                realmName,
                {24.0f, 50.0f}, {40.0f, 54.0f}, {56.0f, 50.0f},
                2.8f, true, false
            });
            banners_.push_back(CurvedMapBanner{
                forestName,
                {-24.0f, 26.0f}, {-14.0f, 30.0f}, {-4.0f, 26.0f},
                2.6f, true, false
            });
            banners_.push_back(CurvedMapBanner{
                desertName,
                {10.0f, -4.0f}, {22.0f, 0.0f}, {34.0f, -4.0f},
                2.6f, true, false
            });
            banners_.push_back(CurvedMapBanner{
                "THE PERILOUS SHORES",
                {-32.0f, -20.0f}, {-6.0f, -24.0f}, {20.0f, -20.0f},
                2.6f, true, true
            });
        }
    } else if (preset_ == AtlasPreset::ShatteredArchipelago) {
        banners_.push_back(CurvedMapBanner{
            "THE SHATTERED SEA",
            {-34.0f, -14.0f}, {0.0f, -22.0f}, {34.0f, -14.0f},
            2.6f, true, true
        });
        banners_.push_back(CurvedMapBanner{
            "CORSAIR ATOLL",
            {-18.0f, 8.0f}, {0.0f, 14.0f}, {18.0f, 8.0f},
            1.9f, true, false
        });
    } else {
        banners_.push_back(CurvedMapBanner{
            "SILVERVALE REALM",
            {-28.0f, -6.0f}, {0.0f, 0.0f}, {28.0f, -6.0f},
            2.4f, true, false
        });
    }
}

void VoronoiAtlas::clearDungeon() {
    dungeonRooms_.clear();
    dungeonCorridors_.clear();
    dungeonWaterPools_.clear();
    pois_.clear();
    banners_.clear();
}

void VoronoiAtlas::addDungeonRoom(int id, float minX, float minZ, float maxX, float maxZ, std::string_view label, bool hasWaterMoat, bool isBossRoom) {
    DungeonRoom rm{};
    rm.id = id;
    rm.minX = std::min(minX, maxX);
    rm.maxX = std::max(minX, maxX);
    rm.minZ = std::min(minZ, maxZ);
    rm.maxZ = std::max(minZ, maxZ);
    rm.label = std::string(label);
    rm.hasWaterMoat = hasWaterMoat;
    rm.isBossRoom = isBossRoom;
    dungeonRooms_.push_back(rm);
}

void VoronoiAtlas::addDungeonCorridor(float x1, float z1, float x2, float z2, float width) {
    DungeonCorridor c{};
    c.p0 = {x1, z1};
    c.p1 = {x2, z2};
    c.halfWidth = std::max(0.8f, width * 0.5f);
    dungeonCorridors_.push_back(c);
}

void VoronoiAtlas::addDungeonWaterPool(float minX, float minZ, float maxX, float maxZ) {
    DungeonWaterPool wp{};
    wp.minX = std::min(minX, maxX);
    wp.maxX = std::max(minX, maxX);
    wp.minZ = std::min(minZ, maxZ);
    wp.maxZ = std::max(minZ, maxZ);
    dungeonWaterPools_.push_back(wp);
}

void VoronoiAtlas::generateWatabouProcedural(uint32_t seed, DungeonTheme theme) {
    clearDungeon();
    preset_ = AtlasPreset::OnePageDungeon;
    seed_ = seed;

    if (theme == DungeonTheme::Auto) {
        uint32_t thChoice = ((seed >> 4) ^ (seed >> 12)) % 4;
        theme = static_cast<DungeonTheme>(thChoice + 1);
    }
    dungeonTheme_ = theme;

    uint32_t s0 = hashU32(seed ^ 0xA5A5A5A5u);
    int topology = (s0 >> 6) % 3; // 0 = Symmetrical Cross, 1 = Linear Gauntlet, 2 = Cloister Loop

    if (topology == 0) {
        // Topology 0: Symmetrical Cross (Classic Watabou One-Page Dungeon)
        float entW = 8.0f + static_cast<float>(s0 & 3u);
        addDungeonRoom(0, -entW, -36.0f, entW, -24.0f, "I. ATRIUM OF ASCENT", false, false);
        pois_.push_back(ChartPoi{PoiCategory::DungeonPortal, {0.0f, -32.0f}, 0.25f, "OVERWORLD EXIT", "Ascend to Surface", 1, 1});

        addDungeonCorridor(0.0f, -24.0f, 0.0f, -14.0f, 3.4f);

        float naveW = 12.0f + static_cast<float>((s0 >> 2) & 3u) * 1.5f;
        addDungeonRoom(1, -naveW, -14.0f, naveW, 12.0f, "II. THE PILLARED NAVE", false, false);

        addDungeonCorridor(-naveW, -1.0f, -naveW - 10.0f, -1.0f, 3.2f);
        float westMinX = -naveW - 28.0f;
        float westMaxX = -naveW - 10.0f;
        bool westPool = (theme == DungeonTheme::SunkenSanctum || theme == DungeonTheme::MoltenVault);
        addDungeonRoom(2, westMinX, -14.0f, westMaxX, 10.0f,
                       westPool ? (theme == DungeonTheme::MoltenVault ? "III. CRUCIBLE OF EMBERS" : "III. DROWNED VAULT") : "III. WEST SEPULCHRE",
                       westPool, false);
        if (westPool) {
            addDungeonWaterPool(westMinX + 3.0f, -11.0f, westMaxX - 3.0f, 7.0f);
        }
        pois_.push_back(ChartPoi{PoiCategory::StandingStones, {(westMinX + westMaxX) * 0.5f, -1.0f}, 0.25f,
                                 westPool ? "SACRED BASIN" : "ANCIENT SARCOPHAGUS", "Chamber Feature", 2, 2});

        addDungeonCorridor(naveW, -1.0f, naveW + 10.0f, -1.0f, 3.2f);
        float eastMinX = naveW + 10.0f;
        float eastMaxX = naveW + 28.0f;
        addDungeonRoom(3, eastMinX, -14.0f, eastMaxX, 10.0f,
                       (theme == DungeonTheme::GildedReliquary) ? "IV. GRAND RELIQUARY" : "IV. ANCIENT TREASURY", false, false);
        pois_.push_back(ChartPoi{PoiCategory::SunkenRuin, {(eastMinX + eastMaxX) * 0.5f, -1.0f}, 0.25f, "TRANSMUTATION FONT", "Gilded Chest & Relics", 3, 3});

        addDungeonCorridor(0.0f, 12.0f, 0.0f, 20.0f, 3.4f);
        addDungeonRoom(4, -10.0f, 20.0f, 10.0f, 34.0f, "V. HALL OF OSSUARIES", false, false);

        addDungeonCorridor(0.0f, 34.0f, 0.0f, 42.0f, 4.0f);
        addDungeonRoom(5, -26.0f, 42.0f, 26.0f, 86.0f, "VI. GRAND SANCTUM", false, true);
        pois_.push_back(ChartPoi{PoiCategory::BossLair, {0.0f, 60.0f}, 0.25f,
                                 (theme == DungeonTheme::SunkenSanctum) ? "THE SUNKEN WYRM" :
                                 (theme == DungeonTheme::MoltenVault) ? "IGNIS DRAKE" :
                                 (theme == DungeonTheme::WhisperingCrypt) ? "CRYPT REVENANT" : "TITAN SENTINEL",
                                 "Chamber Guardian", 4, 6});
    } else if (topology == 1) {
        // Topology 1: Linear Gauntlet (The Deep Descent)
        addDungeonRoom(0, -9.0f, -40.0f, 9.0f, -28.0f, "I. ATRIUM OF ASCENT", false, false);
        pois_.push_back(ChartPoi{PoiCategory::DungeonPortal, {0.0f, -34.0f}, 0.25f, "OVERWORLD EXIT", "Ascend to Surface", 1, 1});

        addDungeonCorridor(0.0f, -28.0f, 0.0f, -18.0f, 3.4f);
        addDungeonRoom(1, -14.0f, -18.0f, 14.0f, -2.0f, "II. MARTIAL VESTIBULE", false, false);

        addDungeonCorridor(0.0f, -2.0f, 0.0f, 8.0f, 3.6f);
        bool hasPool = (theme == DungeonTheme::SunkenSanctum || theme == DungeonTheme::MoltenVault);
        addDungeonRoom(2, -20.0f, 8.0f, 20.0f, 32.0f, hasPool ? "III. SUNKEN ROTUNDA" : "III. HALL OF PILLARS", hasPool, false);
        if (hasPool) {
            addDungeonWaterPool(-12.0f, 14.0f, 12.0f, 26.0f);
        }
        pois_.push_back(ChartPoi{PoiCategory::StandingStones, {0.0f, 20.0f}, 0.25f, "ROTUNDA DAIS", "Central Chamber", 2, 3});

        addDungeonCorridor(0.0f, 32.0f, 0.0f, 42.0f, 3.6f);
        addDungeonRoom(3, -12.0f, 42.0f, 12.0f, 54.0f, "IV. THE THRESHOLD", false, false);

        addDungeonCorridor(0.0f, 54.0f, 0.0f, 62.0f, 4.0f);
        addDungeonRoom(4, -26.0f, 62.0f, 26.0f, 96.0f, "V. ARCH-SANCTUM", false, true);
        pois_.push_back(ChartPoi{PoiCategory::BossLair, {0.0f, 78.0f}, 0.25f,
                                 (theme == DungeonTheme::SunkenSanctum) ? "THE SUNKEN WYRM" :
                                 (theme == DungeonTheme::MoltenVault) ? "IGNIS DRAKE" :
                                 (theme == DungeonTheme::WhisperingCrypt) ? "CRYPT REVENANT" : "TITAN SENTINEL",
                                 "Chamber Guardian", 4, 6});
    } else {
        // Topology 2: Cloister Loop (The Ring Crypt)
        addDungeonRoom(0, -12.0f, -36.0f, 12.0f, -22.0f, "I. ATRIUM OF ASCENT", false, false);
        pois_.push_back(ChartPoi{PoiCategory::DungeonPortal, {0.0f, -30.0f}, 0.25f, "OVERWORLD EXIT", "Ascend to Surface", 1, 1});

        addDungeonCorridor(-8.0f, -22.0f, -24.0f, -12.0f, 3.2f);
        addDungeonCorridor(8.0f, -22.0f, 24.0f, -12.0f, 3.2f);

        bool westPool = (theme == DungeonTheme::SunkenSanctum || theme == DungeonTheme::MoltenVault);
        addDungeonRoom(1, -38.0f, -12.0f, -16.0f, 22.0f, westPool ? "II. FLOODED CLOISTER" : "II. WEST AMBULATORY", westPool, false);
        if (westPool) {
            addDungeonWaterPool(-35.0f, -7.0f, -19.0f, 17.0f);
        }

        addDungeonRoom(2, 16.0f, -12.0f, 38.0f, 22.0f, "III. EAST VAULT GALLERY", false, false);

        addDungeonCorridor(-24.0f, 22.0f, -10.0f, 34.0f, 3.4f);
        addDungeonCorridor(24.0f, 22.0f, 10.0f, 34.0f, 3.4f);

        addDungeonRoom(3, -26.0f, 34.0f, 26.0f, 82.0f, "IV. APEX SANCTUM", false, true);
        pois_.push_back(ChartPoi{PoiCategory::BossLair, {0.0f, 56.0f}, 0.25f,
                                 (theme == DungeonTheme::SunkenSanctum) ? "THE SUNKEN WYRM" :
                                 (theme == DungeonTheme::MoltenVault) ? "IGNIS DRAKE" :
                                 (theme == DungeonTheme::WhisperingCrypt) ? "CRYPT REVENANT" : "TITAN SENTINEL",
                                 "Chamber Guardian", 4, 6});
    }
}

void VoronoiAtlas::generateOnePageDungeonLayout() {
    generateWatabouProcedural(seed_, dungeonTheme_);
}

void VoronoiAtlas::ensureRegionBlocks(float centerWorldX, float centerWorldZ, float radius) {
    if (preset_ == AtlasPreset::OnePageDungeon) return;

    constexpr float kBlockSize = 80.0f;
    int minBx = static_cast<int>(std::floor((centerWorldX - radius) / kBlockSize));
    int maxBx = static_cast<int>(std::ceil((centerWorldX + radius) / kBlockSize));
    int minBz = static_cast<int>(std::floor((centerWorldZ - radius) / kBlockSize));
    int maxBz = static_cast<int>(std::ceil((centerWorldZ + radius) / kBlockSize));

    for (int bz = minBz; bz <= maxBz; ++bz) {
        for (int bx = minBx; bx <= maxBx; ++bx) {
            uint64_t key = packBlockCoord(bx, bz);
            if (generatedBlocks_.insert(key).second) {
                generateMacroBlock(bx, bz);
            }
        }
    }
}

void VoronoiAtlas::generateMacroBlock(int bx, int bz) {
    if (preset_ == AtlasPreset::ContinentalPangaea) {
        // Handcrafted Core Continent POIs cover blocks [-2, 2] in both axes ([-200m, +200m])
        if (std::abs(bx) <= 2 && std::abs(bz) <= 2) return;
    } else {
        if (bx == 0 && bz == 0) return;
    }

    constexpr float kBlockSize = 80.0f;
    float cx = static_cast<float>(bx) * kBlockSize;
    float cz = static_cast<float>(bz) * kBlockSize;

    uint32_t bHash = hashU32(static_cast<uint32_t>(bx) * 73856093u ^
                             static_cast<uint32_t>(bz) * 19349663u ^
                             seed_);

    float jx = (static_cast<float>((bHash >> 4) & 0xFFu) / 255.0f - 0.5f) * (kBlockSize * 0.44f);
    float jz = (static_cast<float>((bHash >> 12) & 0xFFu) / 255.0f - 0.5f) * (kBlockSize * 0.44f);
    float pox = cx + jx;
    float poz = cz + jz;

    float h = rawElevationAt(pox, poz);
    float m = rawMoistureAt(pox, poz);

    ChartPoi poi{};
    poi.pos = {pox, poz};
    poi.elevation = std::max(0.2f, h);
    poi.seed = bHash;
    poi.questLevel = std::clamp(static_cast<int>(std::sqrt(pox * pox + poz * poz) / 30.0f) + 1, 1, 15);

    bool isWater = (h <= 0.0f);
    if (isWater) {
        if (h > -1.8f && (bHash % 3u != 0u)) {
            uint32_t sel = bHash % 3u;
            if (sel == 1u) {
                poi.category = PoiCategory::Lighthouse;
                poi.subtitle = "Shoals Beacon & Anchorage";
            } else {
                poi.category = PoiCategory::SunkenRuin;
                poi.subtitle = "Sunken Coral Vault";
            }
            poi.name = ProceduralNameGenerator::generateLocaleName(bHash, BiomeId::CoastalShoals, poi.category);
            pois_.push_back(poi);
        } else {
            // Dynamic oceanic title banner
            std::string oceanTitle = ProceduralNameGenerator::generateRegionTitle(bHash, BiomeId::OceanDeep);
            banners_.push_back(CurvedMapBanner{
                oceanTitle,
                {pox - 22.0f, poz - 8.0f}, {pox, poz - 16.0f}, {pox + 22.0f, poz - 8.0f},
                2.8f, true, true
            });
            return;
        }
    } else {
        BiomeId biome = classifyBiome(h, m, 12.0f, bHash);
        if (h > 4.6f) {
            poi.category = (bHash % 3u == 0u) ? PoiCategory::AlpineMonastery : PoiCategory::DungeonPortal;
            poi.subtitle = (poi.category == PoiCategory::AlpineMonastery) ? "Cloud-Capped Hermitage" : "Ancient Mountain Barrow";
        } else if (biome == BiomeId::DeciduousWoodland || biome == BiomeId::TaigaPines) {
            uint32_t sel = bHash % 3u;
            if (sel == 0u) {
                poi.category = PoiCategory::MarketHamlet;
                poi.subtitle = (biome == BiomeId::TaigaPines) ? "Pine Valley Hamlet" : "Forest Timber Village";
            } else if (sel == 1u) {
                poi.category = PoiCategory::Windmill;
                poi.subtitle = "Hillside Grain Mill";
            } else {
                poi.category = PoiCategory::DungeonPortal;
                poi.subtitle = "Overgrown Barrow Crypt";
            }
        } else if (biome == BiomeId::AridSteppe || biome == BiomeId::BadlandsCanyon) {
            uint32_t sel = bHash % 3u;
            if (sel == 0u) {
                poi.category = PoiCategory::CitadelKeep;
                poi.subtitle = "Sun-Baked Sandstone Fortress";
            } else if (sel == 1u) {
                poi.category = PoiCategory::StandingStones;
                poi.subtitle = "Sun-Bleached Monoliths";
            } else {
                poi.category = PoiCategory::SunkenRuin;
                poi.subtitle = "Buried Desert Sepulcher";
            }
        } else if (biome == BiomeId::SandyBeach || biome == BiomeId::SaltMarsh) {
            poi.category = (bHash % 2u == 0u) ? PoiCategory::PortHarbor : PoiCategory::Lighthouse;
            poi.subtitle = (poi.category == PoiCategory::PortHarbor) ? "Shoal Haven Anchorage" : "Coastal Watchtower Beacon";
        } else {
            uint32_t sel = bHash % 4u;
            if (sel == 0u) {
                poi.category = PoiCategory::CitadelKeep;
                poi.subtitle = "Walled Overland Keep";
            } else if (sel == 1u) {
                poi.category = PoiCategory::MarketHamlet;
                poi.subtitle = "Verdant Village & Orchards";
            } else if (sel == 2u) {
                poi.category = PoiCategory::StandingStones;
                poi.subtitle = "Ancient Sunken Megalith Ring";
            } else {
                poi.category = PoiCategory::Windmill;
                poi.subtitle = "Hillside Flour Mill";
            }
        }
        poi.name = ProceduralNameGenerator::generateLocaleName(bHash, biome, poi.category);
        pois_.push_back(poi);

        if ((bHash % 3u == 0u)) {
            std::string rTitle = ProceduralNameGenerator::generateRegionTitle(bHash + 99u, biome);
            banners_.push_back(CurvedMapBanner{
                ProceduralNameGenerator::formatCatQuestTracking(rTitle),
                {pox - 22.0f, poz + 16.0f}, {pox, poz + 24.0f}, {pox + 22.0f, poz + 16.0f},
                2.8f, true, false
            });
        }

        // Connect to nearest existing land POI within trade-route distance
        float bestDist = 160.0f;
        int bestIdx = -1;
        for (size_t i = 0; i + 1 < pois_.size(); ++i) {
            float dx = pox - pois_[i].pos.x;
            float dz = poz - pois_[i].pos.y;
            float d = std::sqrt(dx * dx + dz * dz);
            if (d > 25.0f && d < bestDist && pois_[i].elevation > 0.05f) {
                bestDist = d;
                bestIdx = static_cast<int>(i);
            }
        }
        if (bestIdx >= 0) {
            Vec2f pA = pois_[bestIdx].pos;
            Vec2f pB = poi.pos;
            Vec2f mid{(pA.x + pB.x) * 0.5f, (pA.y + pB.y) * 0.5f};
            float jxRoad = (static_cast<float>((bHash >> 8) & 0x1Fu) / 31.0f - 0.5f) * 16.0f;
            float jzRoad = (static_cast<float>((bHash >> 13) & 0x1Fu) / 31.0f - 0.5f) * 16.0f;
            mid.x += jxRoad;
            mid.y += jzRoad;
            bool highway = (poi.category == PoiCategory::CitadelKeep && pois_[bestIdx].category == PoiCategory::CitadelKeep);
            addCurvedRoad(pA, mid, pB, highway);
        }

        // 1. Mountain Peak / Alpine Cirque / Glacier Melt Headwaters
        if (h > 3.5f && m > 0.35f && (bHash % 3u == 0u)) {
            traceHydrologicalRiver({pox, poz}, 1.0f, 0.02f, 420.0f);
        }
        // 2. Karst Springs / Subterranean Aquifer Outlets in Mid-Altitude Moisture Basins
        else if (h > 1.8f && h < 3.2f && m > 0.62f && (bHash % 4u == 0u)) {
            traceHydrologicalRiver({pox, poz}, 1.2f, 0.02f, 320.0f);
        }
    }
}

ChartFieldSample VoronoiAtlas::sampleAtKnown(
    float worldX, float worldZ,
    float hC, float mC,
    float gradX, float gradZ,
    float hOff, float mOff
) const noexcept {
    if (preset_ == AtlasPreset::OnePageDungeon) {
        return sampleAt(worldX, worldZ);
    }

    (void)hOff;
    (void)mOff;
    ChartFieldSample s{};
    s.gradX = gradX;
    s.gradZ = gradZ;

    // 1. Evaluate River spline distance and carve river channels (with spatial grid)
    float minRiverDist = 100.0f;
    float activeRiverWidth = 1.0f;
    int cellX = static_cast<int>(std::floor(worldX / kRiverSpatialCell));
    int cellZ = static_cast<int>(std::floor(worldZ / kRiverSpatialCell));
    auto rIt = riverSpatialGrid_.find(packBlockCoord(cellX, cellZ));
    if (rIt != riverSpatialGrid_.end()) {
        for (uint32_t rIdx : rIt->second) {
            const auto& rv = rivers_[rIdx];
            if (worldX < rv.minX || worldX > rv.maxX || worldZ < rv.minZ || worldZ > rv.maxZ) continue;

            float t = 0.0f;
            float d = pointToSegmentDist({worldX, worldZ}, rv.p0, rv.p1, t);
            float w = rv.width0 + (rv.width1 - rv.width0) * t;
            if (d - w < minRiverDist - activeRiverWidth) {
                minRiverDist = d;
                activeRiverWidth = w;
            }
        }
    }
    s.sdfRiver = minRiverDist;
    s.riverWidth = activeRiverWidth;

    if (hC > 0.0f && minRiverDist < activeRiverWidth * 2.4f) {
        float carve = std::max(0.0f, 1.0f - (minRiverDist / (activeRiverWidth * 2.4f)));
        hC -= carve * carve * 0.85f;
    }

    // 2. Evaluate Road spline distance & bridge crossings (with precomputed AABB culling)
    float minRoadDist = 100.0f;
    float bestRoadArc = 0.0f;
    bool isHighway = false;
    for (const auto& rd : roads_) {
        if (worldX < rd.minX || worldX > rd.maxX || worldZ < rd.minZ || worldZ > rd.maxZ) continue;

        float t = 0.0f;
        float d = pointToSegmentDist({worldX, worldZ}, rd.p0, rd.p1, t);
        if (d < minRoadDist) {
            minRoadDist = d;
            bestRoadArc = rd.arcStart + (rd.arcEnd - rd.arcStart) * t;
            isHighway   = rd.isRoyalHighway;
        }
    }
    s.sdfRoad = minRoadDist;
    s.roadArc = isHighway ? bestRoadArc : -bestRoadArc;

    // Flatten terrain slightly under settlements (with AABB culling)
    for (const auto& p : pois_) {
        float dx = worldX - p.pos.x;
        if (std::abs(dx) > 6.0f) continue;
        float dz = worldZ - p.pos.y;
        if (std::abs(dz) > 6.0f) continue;
        float d  = std::sqrt(dx * dx + dz * dz);
        if (d < 5.5f && (p.category == PoiCategory::CitadelKeep ||
                         p.category == PoiCategory::PortHarbor ||
                         p.category == PoiCategory::MarketHamlet)) {
            s.flags |= ChartFlags::IsSettlement;
        }
    }

    // 3. Compute signed coastline distance (sdfCoast)
    float gradMag = std::sqrt(s.gradX * s.gradX + s.gradZ * s.gradZ);
    float metricScale = 1.0f / std::clamp(gradMag, 0.14f, 0.55f);
    s.sdfCoast = hC * metricScale * 2.2f;

    if (hC <= 0.0f) {
        s.elevation = 0.0f;
        s.flags |= ChartFlags::IsWater;
    } else {
        // Authentic Sorcery! 3D Parchment Relief Elevation Scaling:
        // Diverse tiered terrain: low coastal beaches -> gentle rolling meadows/mounds ->
        // terraced plateaus -> soaring alpine mountain crags & jagged ridges
        float baseH = hC;
        float hScaled = 0.0f;
        if (baseH < 1.2f) {
            // Gentle coastal incline from sea level (0.0m to 0.75m)
            float t = baseH / 1.2f;
            hScaled = t * t * 0.75f;
        } else if (baseH < 3.8f) {
            // Sorcery! rolling green mounds & plains (0.75m to 3.2m)
            float t = (baseH - 1.2f) / 2.6f;
            hScaled = 0.75f + (t * (2.0f - t)) * 2.45f;
        } else if (baseH < 5.4f) {
            // Stepped plateaus, badlands & foothills (3.2m to 5.8m)
            float t = (baseH - 3.8f) / 1.6f;
            hScaled = 3.20f + t * 2.60f;
        } else {
            // Soaring alpine peaks & rugged mountain crags (5.8m to 10.5m)
            float t = (baseH - 5.4f);
            hScaled = 5.80f + t * 2.10f + std::pow(std::max(0.0f, t), 1.25f) * 0.85f;
        }

        // River valley carving: rivers flow through naturally carved valleys
        if (minRiverDist < activeRiverWidth * 2.8f) {
            float rNorm = minRiverDist / (activeRiverWidth * 2.8f);
            float riverCarve = (1.0f - rNorm * rNorm) * std::min(1.4f, hScaled * 0.45f);
            hScaled = std::max(0.12f, hScaled - riverCarve);
        }

        s.elevation = hScaled;
    }

    if (minRoadDist < 1.35f && minRiverDist < activeRiverWidth * 1.25f && s.sdfCoast > -1.0f) {
        s.flags |= ChartFlags::IsBridge;
        s.elevation = std::max(s.elevation, 0.45f);
    }

    // 4. Continuous Biome classification & smooth watercolor transition blending
    uint32_t cellSeed = hashU32(static_cast<uint32_t>(std::floor(worldX * 0.1f)) * 73u +
                                static_cast<uint32_t>(std::floor(worldZ * 0.1f)) * 199u + seed_);
    float coastDist = std::abs(s.sdfCoast);

    if (hC <= 0.0f) {
        // Underwater: Ocean Deep to Coastal Shoals
        s.biomePrimary = (hC <= -2.2f) ? BiomeId::OceanDeep : BiomeId::CoastalShoals;
        s.biomeSecondary = (s.sdfCoast > -3.5f) ? BiomeId::SandyBeach : BiomeId::OceanDeep;
        s.biomeBlend = (s.sdfCoast > -3.5f) ? std::clamp((s.sdfCoast + 3.5f) / 3.5f, 0.0f, 1.0f) : 0.0f;
    } else {
        // Landmass: Evaluate inland base biome
        BiomeId inlandBiome = BiomeId::LushMeadow;
        BiomeId inlandSecondary = BiomeId::LushMeadow;
        float inlandBlend = 0.0f;

        if (hC > 7.2f) {
            inlandBiome = BiomeId::AlpinePeak;
            inlandSecondary = (mC > 0.42f) ? BiomeId::TaigaPines : BiomeId::BadlandsCanyon;
            inlandBlend = std::clamp((7.6f - hC) / 0.8f, 0.0f, 1.0f);
        } else if (hC > 4.2f) {
            if (mC > 0.42f) {
                inlandBiome = BiomeId::TaigaPines;
                inlandSecondary = (hC > 6.4f) ? BiomeId::AlpinePeak : BiomeId::DeciduousWoodland;
                inlandBlend = (hC > 6.4f) ? std::clamp((hC - 6.4f) / 0.8f, 0.0f, 1.0f) : std::clamp((4.6f - hC) / 0.8f, 0.0f, 1.0f);
            } else {
                inlandBiome = BiomeId::BadlandsCanyon;
                inlandSecondary = (hC > 6.4f) ? BiomeId::AlpinePeak : BiomeId::AridSteppe;
                inlandBlend = (hC > 6.4f) ? std::clamp((hC - 6.4f) / 0.8f, 0.0f, 1.0f) : std::clamp((4.6f - hC) / 0.8f, 0.0f, 1.0f);
            }
        } else {
            // Lowlands / Rolling Hills
            if (mC > 0.68f) {
                inlandBiome = BiomeId::DeciduousWoodland;
                inlandSecondary = BiomeId::LushMeadow;
                inlandBlend = std::clamp((0.74f - mC) / 0.12f, 0.0f, 1.0f);
            } else if (mC > 0.35f) {
                inlandBiome = BiomeId::LushMeadow;
                if (mC > 0.58f) {
                    inlandSecondary = BiomeId::DeciduousWoodland;
                    inlandBlend = std::clamp((mC - 0.58f) / 0.10f, 0.0f, 1.0f);
                } else if (mC < 0.42f) {
                    inlandSecondary = BiomeId::AridSteppe;
                    inlandBlend = std::clamp((0.42f - mC) / 0.10f, 0.0f, 1.0f);
                } else {
                    inlandSecondary = BiomeId::LushMeadow;
                    inlandBlend = 0.0f;
                }
            } else {
                inlandBiome = BiomeId::AridSteppe;
                inlandSecondary = BiomeId::LushMeadow;
                inlandBlend = std::clamp((mC - 0.28f) / 0.10f, 0.0f, 1.0f);
            }
        }

        // Calamity Blight pockets
        if ((cellSeed & 31u) == 0u && hC > 2.2f && mC < 0.35f) {
            inlandBiome = BiomeId::CalamityBlight;
        }

        // Coastal Transition: Smooth blend from SandyBeach/SaltMarsh into the inland biome
        if (coastDist < 4.2f && hC < 1.8f) {
            BiomeId coastBiome = (mC > 0.66f) ? BiomeId::SaltMarsh : BiomeId::SandyBeach;
            if (coastDist < 1.8f) {
                s.biomePrimary = coastBiome;
                s.biomeSecondary = inlandBiome;
                s.biomeBlend = std::clamp((coastDist - 0.8f) / 1.0f * 0.25f, 0.0f, 0.25f);
            } else {
                s.biomePrimary = coastBiome;
                s.biomeSecondary = inlandBiome;
                s.biomeBlend = std::clamp((coastDist - 1.8f) / 2.4f, 0.0f, 1.0f);
            }
        } else {
            s.biomePrimary = inlandBiome;
            s.biomeSecondary = inlandSecondary;
            s.biomeBlend = inlandBlend;
        }
    }

    return s;
}

ChartFieldSample VoronoiAtlas::sampleAt(float worldX, float worldZ) const noexcept {
    if (preset_ == AtlasPreset::OnePageDungeon) {
        ChartFieldSample s{};
        // Evaluate signed distance to carved One-Page Dungeon rooms & corridors
        float minSignedDist = 100.0f; // < 0 inside room floor, > 0 in hatched stone wall
        bool inMoatRoom = false;
        for (const auto& rm : dungeonRooms_) {
            float cx = 0.5f * (rm.minX + rm.maxX);
            float cz = 0.5f * (rm.minZ + rm.maxZ);
            float hx = 0.5f * (rm.maxX - rm.minX);
            float hz = 0.5f * (rm.maxZ - rm.minZ);
            float dx = std::abs(worldX - cx) - hx;
            float dz = std::abs(worldZ - cz) - hz;
            float outsideDist = std::sqrt(std::max(dx, 0.0f) * std::max(dx, 0.0f) +
                                          std::max(dz, 0.0f) * std::max(dz, 0.0f));
            float insideDist = std::min(std::max(dx, dz), 0.0f);
            float d = outsideDist + insideDist;
            if (d < minSignedDist) {
                minSignedDist = d;
                inMoatRoom = rm.hasWaterMoat;
            }
        }

        for (const auto& corr : dungeonCorridors_) {
            float vx = corr.p1.x - corr.p0.x;
            float vz = corr.p1.y - corr.p0.y;
            float lenSq = vx * vx + vz * vz;
            float t = 0.0f;
            if (lenSq > 1e-4f) {
                t = std::clamp(((worldX - corr.p0.x) * vx + (worldZ - corr.p0.y) * vz) / lenSq, 0.0f, 1.0f);
            }
            float px = corr.p0.x + t * vx;
            float pz = corr.p0.y + t * vz;
            float distToCenter = std::sqrt((worldX - px) * (worldX - px) + (worldZ - pz) * (worldZ - pz));
            float d = distToCenter - corr.halfWidth;
            if (d < minSignedDist) {
                minSignedDist = d;
            }
        }

        float minWaterDist = 100.0f;
        for (const auto& wp : dungeonWaterPools_) {
            float cx = 0.5f * (wp.minX + wp.maxX);
            float cz = 0.5f * (wp.minZ + wp.maxZ);
            float hx = 0.5f * (wp.maxX - wp.minX);
            float hz = 0.5f * (wp.maxZ - wp.minZ);
            float dx = std::abs(worldX - cx) - hx;
            float dz = std::abs(worldZ - cz) - hz;
            float outsideDist = std::sqrt(std::max(dx, 0.0f) * std::max(dx, 0.0f) +
                                          std::max(dz, 0.0f) * std::max(dz, 0.0f));
            float insideDist = std::min(std::max(dx, dz), 0.0f);
            float d = outsideDist + insideDist;
            if (d < minWaterDist) {
                minWaterDist = d;
            }
        }
        bool inWater = (minWaterDist <= 0.0f);

        s.flags = ChartFlags::IsDungeonMode;
        // 2.5D Elevation Profile: Smooth analytical Hermite S-curve for raised walls and sunken pool basin
        float elev = 0.25f;
        if (minSignedDist > 0.0f) {
            // Walls: rise smoothly from floor (0.25m) to wall parapet (1.65m) across 1.4m transition
            float t = std::clamp(minSignedDist / 1.4f, 0.0f, 1.0f);
            float sCurve = t * t * (3.0f - 2.0f * t);
            elev = 0.25f + 1.40f * sCurve;
        } else if (inWater) {
            // Sunken Water Pool: dips smoothly down to -0.20m across 0.8m basin
            float t = std::clamp(-minWaterDist / 0.8f, 0.0f, 1.0f);
            float sCurve = t * t * (3.0f - 2.0f * t);
            elev = 0.25f - 0.45f * sCurve;
        }
        s.elevation = elev;
        s.sdfRiver  = minWaterDist; // Vector signed distance to water pool (< 0 inside pool, > 0 outside)

        if (minSignedDist <= 0.0f) {
            // Inside carved flagstone room or corridor
            s.sdfCoast  = -minSignedDist;
            s.biomePrimary   = BiomeId::DungeonFlagstone;
            s.biomeSecondary = BiomeId::DungeonFlagstone;
            if (inWater) {
                s.flags |= ChartFlags::IsWater;
                s.biomePrimary = BiomeId::CoastalShoals;
            }
        } else {
            // Solid cross-hatched subterranean rock perimeter
            s.sdfCoast  = -minSignedDist;
            s.biomePrimary   = BiomeId::DungeonHatchedWall;
            s.biomeSecondary = BiomeId::DungeonFlagstone;
        }
        return s;
    }

    constexpr float kEps = 0.65f;
    float hC = rawElevationAt(worldX, worldZ);
    float hX = rawElevationAt(worldX + kEps, worldZ);
    float hZ = rawElevationAt(worldX, worldZ + kEps);
    float gradX = (hX - hC) / kEps;
    float gradZ = (hZ - hC) / kEps;

    float mC = rawMoistureAt(worldX, worldZ);

    return sampleAtKnown(worldX, worldZ, hC, mC, gradX, gradZ);
}

} // namespace eatscharted
