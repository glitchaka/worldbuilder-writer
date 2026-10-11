#include "ui/MapTerrainEngine.h"

#include <QSet>
#include <algorithm>
#include <cmath>

namespace wbw {
namespace {
float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
}

MapTerrainEngine::MapTerrainEngine(int width, int height, quint32 seed)
    : width_(std::max(8, width)), height_(std::max(8, height)), seed_(seed ? seed : 1), heights_(width_ * height_, 0.0f) {}

void MapTerrainEngine::clear(float value) { std::fill(heights_.begin(), heights_.end(), clamp01(value)); }

float MapTerrainEngine::at(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return 0.0f;
    return heights_.at(index(x, y));
}

float MapTerrainEngine::noise(int x, int y, quint32 salt) const {
    quint32 h = seed_ ^ salt ^ static_cast<quint32>(x * 374761393u) ^ static_cast<quint32>(y * 668265263u);
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= h >> 16u;
    return static_cast<float>(h & 0xffffu) / 65535.0f;
}

void MapTerrainEngine::generateWorld(int continentCount, int islandCount, float roughness) {
    continentCount = std::clamp(continentCount, 1, 8);
    islandCount = std::clamp(islandCount, 0, 40);
    roughness = std::clamp(roughness, 0.05f, 1.0f);
    clear(0.04f);

    const float minDim = static_cast<float>(std::min(width_, height_));
    for (int i = 0; i < continentCount; ++i) {
        const float x = width_ * (0.16f + noise(i, 3, 0x51ed270bu) * 0.68f);
        const float y = height_ * (0.16f + noise(i, 9, 0x9e3779b9u) * 0.68f);
        const float radius = minDim * (0.18f + noise(i, 17, 0x85ebca6bu) * 0.13f);
        addHill(QPointF(x, y), radius, 0.62f + noise(i, 23, 0xc2b2ae35u) * 0.22f);

        const int lobes = 3 + static_cast<int>(noise(i, 31, 0x27d4eb2fu) * 4.0f);
        for (int l = 0; l < lobes; ++l) {
            const float angle = noise(i * 17 + l, 5, 0x165667b1u) * 6.2831853f;
            const float distance = radius * (0.28f + noise(i * 19 + l, 7, 0xd3a2646cu) * 0.56f);
            const QPointF p(x + std::cos(angle) * distance, y + std::sin(angle) * distance);
            addHill(p, radius * (0.42f + noise(i + l, 41, 0xfd7046c5u) * 0.42f), 0.28f + roughness * 0.28f);
        }

        const float a = noise(i, 53, 0xb55a4f09u) * 6.2831853f;
        const QPointF start(x - std::cos(a) * radius * .7f, y - std::sin(a) * radius * .7f);
        const QPointF end(x + std::cos(a) * radius * .7f, y + std::sin(a) * radius * .7f);
        addRange(start, end, radius * .18f, .16f + roughness * .24f, .22f + roughness * .22f);
    }

    for (int i = 0; i < islandCount; ++i) {
        const float x = width_ * (0.05f + noise(i, 61, 0x94d049bbu) * 0.90f);
        const float y = height_ * (0.05f + noise(i, 67, 0x369dea0fu) * 0.90f);
        const float radius = minDim * (0.025f + noise(i, 71, 0xa24baed5u) * 0.065f);
        addHill(QPointF(x, y), radius, .38f + noise(i, 73, 0x9fb21c65u) * .32f);
    }

    // Keep the world bounded by ocean. This avoids the "painted rectangle" look and gives
    // generated continents coastlines on all sides.
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const float nx = std::min(x, width_ - 1 - x) / std::max(1.0f, width_ * .15f);
            const float ny = std::min(y, height_ - 1 - y) / std::max(1.0f, height_ * .15f);
            const float edge = clamp01(std::min(nx, ny));
            float& h = heights_[index(x, y)];
            const float largeNoise = (noise(x / 5, y / 5, 0x243f6a88u) - .5f) * roughness * .16f;
            const float smallNoise = (noise(x, y, 0x85a308d3u) - .5f) * roughness * .06f;
            h = clamp01((h + largeNoise + smallNoise) * (.32f + .68f * edge));
        }
    }

    smooth(2);
    erode(2, .16f + roughness * .08f);
}

void MapTerrainEngine::applyDisc(const QPointF& center, float radius, float amount, bool subtract) {
    if (radius <= 0.5f || amount <= 0.0f) return;
    const int minX = std::max(0, static_cast<int>(std::floor(center.x() - radius)));
    const int maxX = std::min(width_ - 1, static_cast<int>(std::ceil(center.x() + radius)));
    const int minY = std::max(0, static_cast<int>(std::floor(center.y() - radius)));
    const int maxY = std::min(height_ - 1, static_cast<int>(std::ceil(center.y() + radius)));
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float dx = static_cast<float>(x - center.x());
            const float dy = static_cast<float>(y - center.y());
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d > radius) continue;
            const float t = 1.0f - d / radius;
            const float falloff = t * t * (3.0f - 2.0f * t);
            const float variation = 0.92f + noise(x, y, 0x9e3779b9u) * 0.16f;
            float& h = heights_[index(x, y)];
            h = clamp01(h + (subtract ? -1.0f : 1.0f) * amount * falloff * variation);
        }
    }
}

void MapTerrainEngine::addHill(const QPointF& center, float radius, float height) { applyDisc(center, radius, height, false); }
void MapTerrainEngine::addPit(const QPointF& center, float radius, float depth) { applyDisc(center, radius, depth, true); }

void MapTerrainEngine::addRange(const QPointF& start, const QPointF& end, float radius, float height, float jitterAmount) {
    const QPointF delta = end - start;
    const float length = std::max(1.0, std::hypot(delta.x(), delta.y()));
    const int steps = std::max(2, static_cast<int>(length / std::max(1.0f, radius * 0.35f)));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        QPointF p = start + delta * t;
        const float nx = noise(i, 11, 0x51ed270bu) - 0.5f;
        const float ny = noise(i, 29, 0x7f4a7c15u) - 0.5f;
        p += QPointF(nx * radius * jitterAmount, ny * radius * jitterAmount);
        const float ridge = height * (0.88f + noise(i, 7, 0x94d049bbu) * 0.24f);
        applyDisc(p, radius * (0.82f + noise(i, 3, 0x369dea0fu) * 0.36f), ridge, false);
    }
}

void MapTerrainEngine::addTrough(const QPointF& start, const QPointF& end, float radius, float depth, float jitterAmount) {
    const QPointF delta = end - start;
    const float length = std::max(1.0, std::hypot(delta.x(), delta.y()));
    const int steps = std::max(2, static_cast<int>(length / std::max(1.0f, radius * 0.35f)));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        QPointF p = start + delta * t;
        p += QPointF((noise(i, 5, 0xa24baed5u) - 0.5f) * radius * jitterAmount,
                     (noise(i, 17, 0x9fb21c65u) - 0.5f) * radius * jitterAmount);
        applyDisc(p, radius, depth, true);
    }
}

void MapTerrainEngine::carveStrait(bool vertical, float center, float width, float depth) {
    const float half = std::max(1.0f, width * 0.5f);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const float axis = vertical ? static_cast<float>(x) : static_cast<float>(y);
            const float waviness = (vertical ? noise(y, 1, 0x243f6a88u) : noise(x, 1, 0x85a308d3u)) - 0.5f;
            const float localCenter = center + waviness * half * 0.9f;
            const float d = std::abs(axis - localCenter);
            if (d > half) continue;
            const float t = 1.0f - d / half;
            float& h = heights_[index(x, y)];
            h = clamp01(h - depth * t * t);
        }
    }
}

void MapTerrainEngine::smooth(int passes) {
    passes = std::clamp(passes, 1, 12);
    QVector<float> next(heights_.size());
    for (int pass = 0; pass < passes; ++pass) {
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                float sum = 0.0f, weight = 0.0f;
                for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox) {
                    const int px = x + ox, py = y + oy;
                    if (px < 0 || py < 0 || px >= width_ || py >= height_) continue;
                    const float w = (ox == 0 && oy == 0) ? 4.0f : ((ox == 0 || oy == 0) ? 2.0f : 1.0f);
                    sum += heights_.at(index(px, py)) * w; weight += w;
                }
                next[index(x, y)] = sum / std::max(1.0f, weight);
            }
        }
        heights_.swap(next);
    }
}

void MapTerrainEngine::erode(int passes, float strength) {
    passes = std::clamp(passes, 1, 16);
    strength = std::clamp(strength, 0.01f, 0.45f);
    QVector<float> delta(heights_.size());
    for (int pass = 0; pass < passes; ++pass) {
        std::fill(delta.begin(), delta.end(), 0.0f);
        for (int y = 1; y < height_ - 1; ++y) {
            for (int x = 1; x < width_ - 1; ++x) {
                const int i = index(x, y);
                float current = heights_.at(i);
                int lowX = x, lowY = y;
                float low = current;
                for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox) {
                    if (!ox && !oy) continue;
                    const float candidate = at(x + ox, y + oy);
                    if (candidate < low) { low = candidate; lowX = x + ox; lowY = y + oy; }
                }
                const float slope = current - low;
                if (slope < 0.035f) continue;
                const float moved = std::min(slope * strength, current * .08f);
                delta[i] -= moved;
                delta[index(lowX, lowY)] += moved * .82f;
            }
        }
        for (int i = 0; i < heights_.size(); ++i) heights_[i] = clamp01(heights_[i] + delta[i]);
    }
}

void MapTerrainEngine::normalize() {
    if (heights_.isEmpty()) return;
    const auto [minIt, maxIt] = std::minmax_element(heights_.cbegin(), heights_.cend());
    const float lo = *minIt, hi = *maxIt, span = std::max(0.0001f, hi - lo);
    for (float& h : heights_) h = clamp01((h - lo) / span);
}

QPointF MapTerrainEngine::steepestNeighbor(int x, int y) const {
    float best = at(x, y);
    QPointF result(x, y);
    for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox) {
        if (!ox && !oy) continue;
        const int nx = x + ox, ny = y + oy;
        if (nx < 0 || ny < 0 || nx >= width_ || ny >= height_) continue;
        const float h = at(nx, ny);
        if (h < best) { best = h; result = QPointF(nx, ny); }
    }
    return result;
}

QJsonArray MapTerrainEngine::traceRivers(int count, float seaLevel) const {
    count = std::clamp(count, 0, 80);
    seaLevel = std::clamp(seaLevel, 0.05f, 0.85f);
    struct Candidate { int x; int y; float score; };
    QVector<Candidate> candidates;
    for (int y = 2; y < height_ - 2; y += 2) for (int x = 2; x < width_ - 2; x += 2) {
        const float h = at(x, y);
        if (h < seaLevel + .18f) continue;
        const float score = h + noise(x, y, 0x7f4a7c15u) * .12f;
        candidates.append({x, y, score});
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b){ return a.score > b.score; });

    QJsonArray rivers;
    QSet<quint64> occupied;
    for (const Candidate& c : candidates) {
        if (rivers.size() >= count) break;
        bool tooClose = false;
        for (int oy = -4; oy <= 4 && !tooClose; ++oy) for (int ox = -4; ox <= 4; ++ox) {
            const quint64 key = (static_cast<quint64>(c.x + ox) << 32) | static_cast<quint32>(c.y + oy);
            if (occupied.contains(key)) { tooClose = true; break; }
        }
        if (tooClose) continue;

        QJsonArray points;
        int x = c.x, y = c.y;
        QSet<quint64> visited;
        for (int step = 0; step < width_ + height_; ++step) {
            const quint64 key = (static_cast<quint64>(x) << 32) | static_cast<quint32>(y);
            if (visited.contains(key)) break;
            visited.insert(key); occupied.insert(key);
            if (step % 2 == 0) points.append(QJsonObject{{QStringLiteral("x"), static_cast<double>(x) / std::max(1, width_ - 1)}, {QStringLiteral("y"), static_cast<double>(y) / std::max(1, height_ - 1)}});
            if (at(x, y) <= seaLevel || x <= 1 || y <= 1 || x >= width_ - 2 || y >= height_ - 2) break;
            const QPointF next = steepestNeighbor(x, y);
            if (static_cast<int>(next.x()) == x && static_cast<int>(next.y()) == y) break;
            x = static_cast<int>(next.x()); y = static_cast<int>(next.y());
        }
        if (points.size() >= 5) rivers.append(points);
    }
    return rivers;
}

QJsonObject MapTerrainEngine::toJson() const {
    QJsonArray values;
    values.reserve(heights_.size());
    for (float h : heights_) values.append(static_cast<int>(std::round(clamp01(h) * 255.0f)));
    return QJsonObject{{QStringLiteral("width"), width_}, {QStringLiteral("height"), height_}, {QStringLiteral("seed"), static_cast<qint64>(seed_)}, {QStringLiteral("values"), values}, {QStringLiteral("seaLevel"), 0.42}};
}

MapTerrainEngine MapTerrainEngine::fromJson(const QJsonObject& object) {
    MapTerrainEngine engine(object.value(QStringLiteral("width")).toInt(256), object.value(QStringLiteral("height")).toInt(144), static_cast<quint32>(object.value(QStringLiteral("seed")).toInteger(1)));
    const QJsonArray values = object.value(QStringLiteral("values")).toArray();
    const int count = std::min(values.size(), engine.heights_.size());
    for (int i = 0; i < count; ++i) engine.heights_[i] = clamp01(static_cast<float>(values.at(i).toInt()) / 255.0f);
    return engine;
}

} // namespace wbw
