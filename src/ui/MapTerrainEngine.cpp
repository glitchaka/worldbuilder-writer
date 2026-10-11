#include "ui/MapTerrainEngine.h"

#include <algorithm>
#include <cmath>

namespace wbw {

namespace {
float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
}

MapTerrainEngine::MapTerrainEngine(int width, int height, quint32 seed)
    : width_(std::max(8, width)), height_(std::max(8, height)), seed_(seed ? seed : 1), heights_(width_ * height_, 0.0f) {}

void MapTerrainEngine::clear(float value) {
    std::fill(heights_.begin(), heights_.end(), clamp01(value));
}

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

void MapTerrainEngine::addHill(const QPointF& center, float radius, float height) {
    applyDisc(center, radius, height, false);
}

void MapTerrainEngine::addPit(const QPointF& center, float radius, float depth) {
    applyDisc(center, radius, depth, true);
}

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
                float sum = 0.0f;
                float weight = 0.0f;
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        const int px = x + ox;
                        const int py = y + oy;
                        if (px < 0 || py < 0 || px >= width_ || py >= height_) continue;
                        const float w = (ox == 0 && oy == 0) ? 4.0f : ((ox == 0 || oy == 0) ? 2.0f : 1.0f);
                        sum += heights_.at(index(px, py)) * w;
                        weight += w;
                    }
                }
                next[index(x, y)] = sum / std::max(1.0f, weight);
            }
        }
        heights_.swap(next);
    }
}

void MapTerrainEngine::normalize() {
    if (heights_.isEmpty()) return;
    const auto [minIt, maxIt] = std::minmax_element(heights_.cbegin(), heights_.cend());
    const float lo = *minIt;
    const float hi = *maxIt;
    const float span = std::max(0.0001f, hi - lo);
    for (float& h : heights_) h = clamp01((h - lo) / span);
}

QJsonObject MapTerrainEngine::toJson() const {
    QJsonArray values;
    for (float h : heights_) values.append(static_cast<int>(std::round(clamp01(h) * 255.0f)));
    return QJsonObject{{QStringLiteral("width"), width_},
                       {QStringLiteral("height"), height_},
                       {QStringLiteral("seed"), static_cast<qint64>(seed_)},
                       {QStringLiteral("values"), values},
                       {QStringLiteral("seaLevel"), 0.42}};
}

MapTerrainEngine MapTerrainEngine::fromJson(const QJsonObject& object) {
    MapTerrainEngine engine(object.value(QStringLiteral("width")).toInt(256),
                            object.value(QStringLiteral("height")).toInt(144),
                            static_cast<quint32>(object.value(QStringLiteral("seed")).toInteger(1)));
    const QJsonArray values = object.value(QStringLiteral("values")).toArray();
    const int count = std::min(values.size(), engine.heights_.size());
    for (int i = 0; i < count; ++i) engine.heights_[i] = clamp01(static_cast<float>(values.at(i).toInt()) / 255.0f);
    return engine;
}

} // namespace wbw
