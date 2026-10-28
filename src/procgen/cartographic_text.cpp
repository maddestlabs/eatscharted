#include "eatscharted/cartographic_text.hpp"
#include "eatscharted/voronoi_atlas.hpp"
#include <cmath>
#include <algorithm>
#include <cctype>

namespace eatscharted {

namespace {

struct MeshPen {
    Vec2f center;   // Character center on ground
    Vec2f T;        // Tangent vector along path baseline
    Vec2f N;        // Normal vector across path (letter height)
    float cellW;    // Letter width in meters
    float cellH;    // Letter height in meters
    float strokeW;  // Base stroke width in meters
    const VoronoiAtlas& atlas;
    std::vector<ChartVertex>& outVerts;
    float minX, maxX, minZ, maxZ;
    uint32_t packedColor;
    float yOffset;
    Vec2f shadowShift;

    inline Vec2f mapPoint(float u, float v) const noexcept {
        // u in [0, 10], v in [0, 16]. Baseline at v = 14.0, cap top at v = 2.0
        float normU = (u - 5.0f) / 10.0f;
        float normV = (14.0f - v) / 12.0f;
        return center + shadowShift + T * (normU * cellW) + N * (normV * cellH);
    }

    void line(float u0, float v0, float u1, float v1, float widthMul = 1.0f) const {
        Vec2f p0 = mapPoint(u0, v0);
        Vec2f p1 = mapPoint(u1, v1);

        Vec2f d = p1 - p0;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 1e-4f) return;

        Vec2f perp{-d.y / len, d.x / len};
        float halfW = (strokeW * widthMul) * 0.5f;

        Vec2f c0 = p0 - perp * halfW;
        Vec2f c1 = p0 + perp * halfW;
        Vec2f c2 = p1 + perp * halfW;
        Vec2f c3 = p1 - perp * halfW;

        float qMinX = std::min({c0.x, c1.x, c2.x, c3.x});
        float qMaxX = std::max({c0.x, c1.x, c2.x, c3.x});
        float qMinZ = std::min({c0.y, c1.y, c2.y, c3.y});
        float qMaxZ = std::max({c0.y, c1.y, c2.y, c3.y});

        if (qMaxX < minX || qMinX > maxX || qMaxZ < minZ || qMinZ > maxZ) {
            return;
        }

        auto makeVertex = [&](Vec2f p) -> ChartVertex {
            float gy = std::max(0.0f, atlas.sampleAt(p.x, p.y).elevation);
            ChartVertex v{};
            v.pos[0] = p.x;
            v.pos[1] = gy + yOffset;
            v.pos[2] = p.y;
            v.sdfCoast = 10.0f;
            v.normal[0] = 0.0f; v.normal[1] = 1.0f; v.normal[2] = 0.0f;
            v.sdfRoad = 100.0f;
            v.sdfRiver = 100.0f;
            v.packedBiomes = packedColor;
            v.flags = ChartFlags::IsMapBanner;
            return v;
        };

        ChartVertex vert0 = makeVertex(c0);
        ChartVertex vert1 = makeVertex(c1);
        ChartVertex vert2 = makeVertex(c2);
        ChartVertex vert3 = makeVertex(c3);

        outVerts.push_back(vert0);
        outVerts.push_back(vert1);
        outVerts.push_back(vert2);

        outVerts.push_back(vert0);
        outVerts.push_back(vert2);
        outVerts.push_back(vert3);
    }

    void quad(float u0, float v0, float uc, float vc, float u1, float v1, float widthMul = 1.0f) const {
        float lastU = u0, lastV = v0;
        constexpr int kSteps = 3;
        for (int s = 1; s <= kSteps; ++s) {
            float t = static_cast<float>(s) / static_cast<float>(kSteps);
            float invT = 1.0f - t;
            float nu = invT * invT * u0 + 2.0f * invT * t * uc + t * t * u1;
            float nv = invT * invT * v0 + 2.0f * invT * t * vc + t * t * v1;
            line(lastU, lastV, nu, nv, widthMul);
            lastU = nu;
            lastV = nv;
        }
    }

    void loop(float left, float top, float right, float bottom, float widthMul = 1.1f) const {
        float cx = (left + right) * 0.5f;
        float cy = (top + bottom) * 0.5f;
        quad(cx, top,          left, top,          left, cy, widthMul);
        quad(left, cy,         left, bottom,       cx, bottom, widthMul);
        quad(cx, bottom,       right, bottom,      right, cy, widthMul);
        quad(right, cy,        right, top,         cx, top, widthMul);
    }
};

void drawCartographicGlyph(const MeshPen& p, char ch) {
    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    switch (c) {
        case 'A':
            p.line(5.0f, 2.5f, 1.8f, 14.0f, 1.20f);
            p.line(5.0f, 2.5f, 8.2f, 14.0f, 1.20f);
            p.line(2.8f, 9.8f, 7.2f, 9.8f, 0.90f);
            p.line(1.0f, 14.0f, 2.8f, 14.0f, 0.80f);
            p.line(7.2f, 14.0f, 9.0f, 14.0f, 0.80f);
            break;

        case 'B':
            p.line(2.4f, 2.5f, 2.4f, 14.0f, 1.25f);
            p.quad(2.4f, 2.5f, 8.0f, 4.8f, 2.4f, 8.0f, 1.05f);
            p.quad(2.4f, 8.0f, 8.4f, 10.8f, 2.4f, 14.0f, 1.10f);
            p.line(1.4f, 2.5f, 3.4f, 2.5f, 0.80f);
            p.line(1.4f, 14.0f, 3.4f, 14.0f, 0.80f);
            break;

        case 'C':
            p.quad(8.0f, 4.4f, 2.2f, 2.5f, 2.2f, 8.2f, 1.25f);
            p.quad(2.2f, 8.2f, 2.2f, 14.0f, 8.0f, 12.2f, 1.25f);
            p.line(7.2f, 3.6f, 8.4f, 5.0f, 0.85f);
            break;

        case 'D':
            p.line(2.4f, 2.5f, 2.4f, 14.0f, 1.25f);
            p.quad(2.4f, 2.5f, 8.6f, 8.2f, 2.4f, 14.0f, 1.20f);
            p.line(1.4f, 2.5f, 3.4f, 2.5f, 0.80f);
            p.line(1.4f, 14.0f, 3.4f, 14.0f, 0.80f);
            break;

        case 'E':
            p.line(2.4f, 2.5f, 2.4f, 14.0f, 1.25f);
            p.line(2.4f, 2.5f, 7.8f, 2.5f, 1.00f);
            p.line(7.8f, 2.5f, 7.8f, 4.2f, 0.80f);
            p.line(2.4f, 8.0f, 6.8f, 8.0f, 0.85f);
            p.line(2.4f, 14.0f, 8.0f, 14.0f, 1.05f);
            p.line(8.0f, 14.0f, 8.0f, 12.2f, 0.80f);
            p.line(1.4f, 2.5f, 3.4f, 2.5f, 0.80f);
            p.line(1.4f, 14.0f, 3.4f, 14.0f, 0.80f);
            break;

        case 'F':
            p.line(2.4f, 2.5f, 2.4f, 14.0f, 1.25f);
            p.line(2.4f, 2.5f, 7.8f, 2.5f, 1.00f);
            p.line(7.8f, 2.5f, 7.8f, 4.2f, 0.80f);
            p.line(2.4f, 8.0f, 6.8f, 8.0f, 0.85f);
            p.line(1.4f, 14.0f, 3.4f, 14.0f, 0.80f);
            break;

        case 'G':
            p.quad(8.0f, 4.4f, 2.2f, 2.5f, 2.2f, 8.2f, 1.25f);
            p.quad(2.2f, 8.2f, 2.2f, 14.0f, 7.8f, 13.0f, 1.25f);
            p.line(7.8f, 13.0f, 7.8f, 8.2f, 1.20f);
            p.line(5.2f, 8.2f, 7.8f, 8.2f, 0.90f);
            break;

        case 'H':
            p.line(2.4f, 2.5f, 2.4f, 14.0f, 1.20f);
            p.line(7.6f, 2.5f, 7.6f, 14.0f, 1.20f);
            p.line(2.4f, 8.2f, 7.6f, 8.2f, 0.90f);
            p.line(1.4f, 2.5f, 3.4f, 2.5f, 0.80f);
            p.line(1.4f, 14.0f, 3.4f, 14.0f, 0.80f);
            p.line(6.6f, 2.5f, 8.6f, 2.5f, 0.80f);
            p.line(6.6f, 14.0f, 8.6f, 14.0f, 0.80f);
            break;

        case 'I':
            p.line(5.0f, 2.5f, 5.0f, 14.0f, 1.25f);
            p.line(2.8f, 2.5f, 7.2f, 2.5f, 0.85f);
            p.line(2.8f, 14.0f, 7.2f, 14.0f, 0.85f);
            break;

        case 'J':
            p.line(4.2f, 2.5f, 7.8f, 2.5f, 0.85f);
            p.line(6.5f, 2.5f, 6.5f, 11.0f, 1.20f);
            p.quad(6.5f, 11.0f, 5.8f, 14.2f, 2.5f, 12.0f, 1.15f);
            break;

        case 'K':
            p.line(2.4f, 2.5f, 2.4f, 14.0f, 1.20f);
            p.line(7.6f, 2.5f, 2.6f, 8.5f, 1.05f);
            p.line(4.5f, 7.8f, 8.0f, 14.0f, 1.20f);
            p.line(1.4f, 2.5f, 3.4f, 2.5f, 0.80f);
            p.line(1.4f, 14.0f, 3.4f, 14.0f, 0.80f);
            p.line(7.0f, 14.0f, 8.8f, 14.0f, 0.80f);
            break;

        case 'L':
            p.line(2.4f, 2.5f, 2.4f, 14.0f, 1.25f);
            p.line(2.4f, 14.0f, 7.8f, 14.0f, 1.05f);
            p.line(7.8f, 14.0f, 7.8f, 12.2f, 0.80f);
            p.line(1.4f, 2.5f, 3.4f, 2.5f, 0.80f);
            break;

        case 'M':
            p.line(1.8f, 14.0f, 1.8f, 2.5f, 1.15f);
            p.line(1.8f, 2.5f, 5.0f, 11.0f, 1.10f);
            p.line(5.0f, 11.0f, 8.2f, 2.5f, 1.10f);
            p.line(8.2f, 2.5f, 8.2f, 14.0f, 1.15f);
            p.line(0.8f, 14.0f, 2.8f, 14.0f, 0.80f);
            p.line(7.2f, 14.0f, 9.2f, 14.0f, 0.80f);
            break;

        case 'N':
            p.line(2.2f, 14.0f, 2.2f, 2.5f, 1.20f);
            p.line(2.2f, 2.5f, 7.8f, 14.0f, 1.35f);
            p.line(7.8f, 14.0f, 7.8f, 2.5f, 1.05f);
            p.line(1.2f, 14.0f, 3.2f, 14.0f, 0.80f);
            p.line(6.8f, 2.5f, 8.8f, 2.5f, 0.80f);
            break;

        case 'O':
            p.loop(2.0f, 2.5f, 8.0f, 14.0f, 1.20f);
            break;

        case 'P':
            p.line(2.4f, 2.5f, 2.4f, 14.0f, 1.25f);
            p.quad(2.4f, 2.5f, 8.2f, 5.2f, 2.4f, 8.2f, 1.10f);
            p.line(1.4f, 2.5f, 3.4f, 2.5f, 0.80f);
            p.line(1.4f, 14.0f, 3.4f, 14.0f, 0.80f);
            break;

        case 'Q':
            p.loop(2.0f, 2.5f, 8.0f, 14.0f, 1.20f);
            p.line(5.5f, 11.0f, 8.8f, 15.2f, 1.30f);
            break;

        case 'R':
            p.line(2.4f, 2.5f, 2.4f, 14.0f, 1.25f);
            p.quad(2.4f, 2.5f, 8.2f, 5.2f, 2.4f, 8.0f, 1.10f);
            p.line(4.6f, 8.0f, 8.2f, 14.0f, 1.25f);
            p.line(1.4f, 2.5f, 3.4f, 2.5f, 0.80f);
            p.line(1.4f, 14.0f, 3.4f, 14.0f, 0.80f);
            p.line(7.2f, 14.0f, 9.0f, 14.0f, 0.80f);
            break;

        case 'S':
            p.quad(7.6f, 4.4f, 5.0f, 2.4f, 2.8f, 4.8f, 1.10f);
            p.line(2.8f, 4.8f, 7.4f, 10.0f, 1.25f);
            p.quad(7.4f, 10.0f, 5.2f, 14.2f, 2.4f, 12.2f, 1.10f);
            break;

        case 'T':
            p.line(1.4f, 2.5f, 8.6f, 2.5f, 1.05f);
            p.line(1.4f, 2.5f, 1.4f, 4.0f, 0.80f);
            p.line(8.6f, 2.5f, 8.6f, 4.0f, 0.80f);
            p.line(5.0f, 2.5f, 5.0f, 14.0f, 1.25f);
            p.line(3.5f, 14.0f, 6.5f, 14.0f, 0.80f);
            break;

        case 'U':
            p.line(2.4f, 2.5f, 2.4f, 10.0f, 1.20f);
            p.quad(2.4f, 10.0f, 5.0f, 14.2f, 7.6f, 10.0f, 1.20f);
            p.line(7.6f, 10.0f, 7.6f, 2.5f, 1.20f);
            p.line(1.4f, 2.5f, 3.4f, 2.5f, 0.80f);
            p.line(6.6f, 2.5f, 8.6f, 2.5f, 0.80f);
            break;

        case 'V':
            p.line(2.0f, 2.5f, 5.0f, 14.0f, 1.25f);
            p.line(5.0f, 14.0f, 8.0f, 2.5f, 1.05f);
            p.line(1.0f, 2.5f, 3.0f, 2.5f, 0.80f);
            p.line(7.0f, 2.5f, 9.0f, 2.5f, 0.80f);
            break;

        case 'W':
            p.line(1.6f, 2.5f, 3.4f, 14.0f, 1.15f);
            p.line(3.4f, 14.0f, 5.0f, 6.2f, 1.05f);
            p.line(5.0f, 6.2f, 6.6f, 14.0f, 1.15f);
            p.line(6.6f, 14.0f, 8.4f, 2.5f, 1.05f);
            break;

        case 'X':
            p.line(2.0f, 2.5f, 8.0f, 14.0f, 1.25f);
            p.line(8.0f, 2.5f, 2.0f, 14.0f, 1.05f);
            p.line(1.0f, 2.5f, 3.0f, 2.5f, 0.80f);
            p.line(7.0f, 14.0f, 9.0f, 14.0f, 0.80f);
            break;

        case 'Y':
            p.line(2.2f, 2.5f, 5.0f, 8.0f, 1.15f);
            p.line(7.8f, 2.5f, 5.0f, 8.0f, 1.15f);
            p.line(5.0f, 8.0f, 5.0f, 14.0f, 1.25f);
            p.line(3.5f, 14.0f, 6.5f, 14.0f, 0.80f);
            break;

        case 'Z':
            p.line(2.0f, 2.5f, 8.0f, 2.5f, 1.05f);
            p.line(8.0f, 2.5f, 2.0f, 14.0f, 1.35f);
            p.line(2.0f, 14.0f, 8.0f, 14.0f, 1.10f);
            break;

        case '0':
            p.loop(2.4f, 2.5f, 7.6f, 14.0f, 1.15f);
            p.line(3.2f, 12.5f, 6.8f, 4.0f, 0.80f);
            break;
        case '1':
            p.line(3.2f, 5.0f, 5.0f, 2.5f, 0.90f);
            p.line(5.0f, 2.5f, 5.0f, 14.0f, 1.25f);
            p.line(2.6f, 14.0f, 7.4f, 14.0f, 0.90f);
            break;
        case '2':
            p.quad(2.6f, 5.2f, 5.0f, 2.5f, 7.6f, 5.5f, 1.10f);
            p.line(7.6f, 5.5f, 2.2f, 14.0f, 1.25f);
            p.line(2.2f, 14.0f, 7.8f, 14.0f, 1.10f);
            break;
        case '3':
            p.quad(2.6f, 4.5f, 5.0f, 2.5f, 7.4f, 5.2f, 1.05f);
            p.line(7.4f, 5.2f, 4.8f, 8.0f, 0.95f);
            p.quad(4.8f, 8.0f, 7.8f, 10.5f, 2.6f, 13.5f, 1.15f);
            break;
        case '4':
            p.line(6.5f, 14.0f, 6.5f, 2.5f, 1.25f);
            p.line(6.5f, 2.5f, 1.8f, 10.0f, 1.10f);
            p.line(1.8f, 10.0f, 8.2f, 10.0f, 1.00f);
            break;
        case '5':
            p.line(7.4f, 2.5f, 3.0f, 2.5f, 1.00f);
            p.line(3.0f, 2.5f, 2.8f, 7.5f, 1.10f);
            p.quad(2.8f, 7.5f, 7.8f, 7.8f, 7.4f, 11.2f, 1.15f);
            p.quad(7.4f, 11.2f, 5.8f, 14.2f, 2.4f, 13.2f, 1.10f);
            break;
        case '6':
            p.quad(7.2f, 4.0f, 2.8f, 2.8f, 2.4f, 10.0f, 1.20f);
            p.loop(2.4f, 7.5f, 7.6f, 14.0f, 1.10f);
            break;
        case '7':
            p.line(2.2f, 2.5f, 7.8f, 2.5f, 1.05f);
            p.line(7.8f, 2.5f, 3.8f, 14.0f, 1.25f);
            break;
        case '8':
            p.loop(3.0f, 2.5f, 7.0f, 8.0f, 1.10f);
            p.loop(2.4f, 8.0f, 7.6f, 14.0f, 1.15f);
            break;
        case '9':
            p.loop(2.4f, 2.5f, 7.6f, 9.0f, 1.10f);
            p.quad(7.6f, 6.5f, 7.2f, 13.7f, 2.8f, 12.5f, 1.20f);
            break;

        case '-':
            p.line(2.5f, 8.5f, 7.5f, 8.5f, 1.10f);
            break;
        case '\'':
            p.line(5.0f, 2.5f, 4.0f, 5.5f, 1.20f);
            break;
        case '.':
            p.line(4.2f, 13.2f, 5.8f, 13.2f, 1.30f);
            break;
        case ',':
            p.line(5.0f, 12.5f, 3.8f, 15.0f, 1.20f);
            break;
        case '!':
            p.line(5.0f, 2.5f, 5.0f, 10.5f, 1.30f);
            p.line(4.4f, 13.2f, 5.6f, 13.2f, 1.30f);
            break;
        case '/':
            p.line(2.5f, 14.0f, 7.5f, 2.5f, 1.10f);
            break;

        default:
            break;
    }
}

float getGlyphWidth(char c, float letterH) {
    char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    switch (up) {
        case 'I': case '1': case '\'': case '.': case '!': case ',':
            return letterH * 0.42f;
        case 'M': case 'W':
            return letterH * 1.05f;
        case 'A': case 'O': case 'Q': case 'C': case 'D': case 'G':
            return letterH * 0.82f;
        case 'J': case 'L': case 'F': case 'T': case 'E':
            return letterH * 0.68f;
        case ' ':
            return letterH * 0.50f;
        default:
            return letterH * 0.74f;
    }
}

} // namespace

std::vector<PathPoint> CartographicPathText::generateArcPath(Vec2f center, float radius, float startAngleRad, float endAngleRad, int samples) {
    std::vector<PathPoint> pts;
    pts.reserve(samples);
    for (int i = 0; i < samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(samples - 1);
        float a = startAngleRad + (endAngleRad - startAngleRad) * t;
        float cosA = std::cos(a);
        float sinA = std::sin(a);
        Vec2f p{center.x + radius * cosA, center.y + radius * sinA};
        Vec2f tang{-sinA, cosA};
        Vec2f norm{cosA, sinA};
        pts.push_back(PathPoint{p, tang, norm});
    }
    return pts;
}

std::vector<PathPoint> CartographicPathText::generateBezierPath(Vec2f p0, Vec2f p1, Vec2f p2, int samples) {
    std::vector<PathPoint> pts;
    pts.reserve(samples);
    for (int i = 0; i < samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(samples - 1);
        float invT = 1.0f - t;
        Vec2f pos{
            invT * invT * p0.x + 2.0f * invT * t * p1.x + t * t * p2.x,
            invT * invT * p0.y + 2.0f * invT * t * p1.y + t * t * p2.y
        };
        Vec2f tang{
            2.0f * invT * (p1.x - p0.x) + 2.0f * t * (p2.x - p1.x),
            2.0f * invT * (p1.y - p0.y) + 2.0f * t * (p2.y - p1.y)
        };
        float len = std::sqrt(tang.x * tang.x + tang.y * tang.y);
        if (len > 1e-5f) {
            tang.x /= len;
            tang.y /= len;
        } else {
            tang = {1.0f, 0.0f};
        }
        Vec2f norm{-tang.y, tang.x};
        pts.push_back(PathPoint{pos, tang, norm});
    }
    return pts;
}

void CartographicPathText::emitTextAlongPath(
    const std::string& text,
    const std::vector<PathPoint>& path,
    float letterHeight,
    const VoronoiAtlas& atlas,
    std::vector<ChartVertex>& outVerts,
    float minChunkX, float maxChunkX,
    float minChunkZ, float maxChunkZ,
    bool isWaterTitle
) {
    if (text.empty() || path.size() < 2) return;

    // 1. Calculate cumulative path distances
    std::vector<float> cumDist(path.size(), 0.0f);
    for (size_t i = 1; i < path.size(); ++i) {
        float dx = path[i].pos.x - path[i - 1].pos.x;
        float dz = path[i].pos.y - path[i - 1].pos.y;
        cumDist[i] = cumDist[i - 1] + std::sqrt(dx * dx + dz * dz);
    }
    float totalPathLen = cumDist.back();
    if (totalPathLen < 0.1f) return;

    // 2. Measure total text length with Kerning Tracking
    float kerning = letterHeight * 0.16f;
    std::vector<float> charWidths(text.size());
    float totalTextLen = 0.0f;
    for (size_t i = 0; i < text.size(); ++i) {
        charWidths[i] = getGlyphWidth(text[i], letterHeight);
        totalTextLen += charWidths[i];
        if (i + 1 < text.size()) {
            totalTextLen += kerning;
        }
    }

    // 3. Center the string along the path
    float curDist = std::max(0.0f, (totalPathLen - totalTextLen) * 0.5f);

    auto samplePathAtDist = [&](float d) -> PathPoint {
        d = std::clamp(d, 0.0f, totalPathLen);
        auto it = std::lower_bound(cumDist.begin(), cumDist.end(), d);
        if (it == cumDist.begin()) return path.front();
        if (it == cumDist.end())   return path.back();
        size_t idx = std::distance(cumDist.begin(), it);
        float d0 = cumDist[idx - 1];
        float d1 = cumDist[idx];
        float t = (d1 > d0) ? ((d - d0) / (d1 - d0)) : 0.0f;
        const auto& p0 = path[idx - 1];
        const auto& p1 = path[idx];

        PathPoint res{};
        res.pos.x = std::lerp(p0.pos.x, p1.pos.x, t);
        res.pos.y = std::lerp(p0.pos.y, p1.pos.y, t);

        Vec2f tang{std::lerp(p0.tangent.x, p1.tangent.x, t), std::lerp(p0.tangent.y, p1.tangent.y, t)};
        float tlen = std::sqrt(tang.x * tang.x + tang.y * tang.y);
        if (tlen > 1e-5f) { tang.x /= tlen; tang.y /= tlen; } else { tang = {1.0f, 0.0f}; }
        res.tangent = tang;

        Vec2f norm{std::lerp(p0.normal.x, p1.normal.x, t), std::lerp(p0.normal.y, p1.normal.y, t)};
        float nlen = std::sqrt(norm.x * norm.x + norm.y * norm.y);
        if (nlen > 1e-5f) { norm.x /= nlen; norm.y /= nlen; } else { norm = {-tang.y, tang.x}; }
        res.normal = norm;
        return res;
    };

    uint32_t shadowColor = packBiomesAndLayer(BiomeId::OceanDeep, BiomeId::OceanDeep, InkLayer::InkCalligraphyInk);
    uint32_t primaryColor = packBiomesAndLayer(
        isWaterTitle ? BiomeId::OceanDeep : BiomeId::LushMeadow,
        isWaterTitle ? BiomeId::LushMeadow : BiomeId::OceanDeep,
        InkLayer::InkCalligraphyInk
    );

    float baseStroke = std::max(0.18f, letterHeight * 0.10f); // Robust ~0.22m stroke width

    // 4. Emit each glyph in two passes (Soft Ink Drop Shadow -> Bold India Ink)
    for (size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];
        float cw = charWidths[i];
        float centerDist = curDist + cw * 0.5f;
        curDist += cw + kerning;

        if (ch == ' ') continue;

        PathPoint pt = samplePathAtDist(centerDist);

        // Pass 1: Warm Translucent Ink Drop Shadow on Parchment (shifted +0.12m X, -0.12m Z)
        MeshPen shadowPen{
            pt.pos, pt.tangent, pt.normal,
            cw, letterHeight, baseStroke,
            atlas, outVerts,
            minChunkX, maxChunkX, minChunkZ, maxChunkZ,
            shadowColor, 0.035f,
            pt.tangent * 0.12f - pt.normal * 0.12f
        };
        drawCartographicGlyph(shadowPen, ch);

        // Pass 2: Crisp Dark India Ink Lettering
        MeshPen inkPen{
            pt.pos, pt.tangent, pt.normal,
            cw, letterHeight, baseStroke,
            atlas, outVerts,
            minChunkX, maxChunkX, minChunkZ, maxChunkZ,
            primaryColor, 0.085f,
            {0.0f, 0.0f}
        };
        drawCartographicGlyph(inkPen, ch);
    }
}

} // namespace eatscharted
