#include "ui/MapTerrainEngine.h"

#include <QSet>
#include <algorithm>
#include <cmath>

namespace wbw {
namespace {
float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }
}

MapTerrainEngine::MapTerrainEngine(int width, int height, quint32 seed)
    : width_(std::max(8, width)),
      height_(std::max(8, height)),
      seed_(seed ? seed : 1u),
      heights_(width_ * height_, 0.0f) {}

void MapTerrainEngine::clear(float value) {
    std::fill(heights_.begin(), heights_.end(), clamp01(value));
}

float MapTerrainEngine::at(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return 0.0f;
    return heights_.at(index(x, y));
}

float MapTerrainEngine::noise(int x, int y, quint32 salt) const {
    quint32 hash = seed_ ^ salt ^ static_cast<quint32>(x * 374761393u) ^ static_cast<quint32>(y * 668265263u);
    hash = (hash ^ (hash >> 13u)) * 1274126177u;
    hash ^= hash >> 16u;
    return static_cast<float>(hash & 0xffffu) / 65535.0f;
}

void MapTerrainEngine::applyDisc(const QPointF& center, float radius, float amount, bool subtract) {
    if (radius <= .5f || amount <= 0.0f) return;
    const int minX = std::max(0, static_cast<int>(std::floor(center.x() - radius)));
    const int maxX = std::min(width_ - 1, static_cast<int>(std::ceil(center.x() + radius)));
    const int minY = std::max(0, static_cast<int>(std::floor(center.y() - radius)));
    const int maxY = std::min(height_ - 1, static_cast<int>(std::ceil(center.y() + radius)));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float dx = static_cast<float>(x - center.x());
            const float dy = static_cast<float>(y - center.y());
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance > radius) continue;
            const float t = 1.0f - distance / radius;
            const float falloff = t * t * (3.0f - 2.0f * t);
            const float variation = .92f + noise(x, y, 0x9e3779b9u) * .16f;
            float& height = heights_[index(x, y)];
            height = clamp01(height + (subtract ? -1.0f : 1.0f) * amount * falloff * variation);
        }
    }
}

void MapTerrainEngine::addHill(const QPointF& center, float radius, float height) {
    applyDisc(center, radius, height, false);
}

void MapTerrainEngine::addPit(const QPointF& center, float radius, float depth) {
    applyDisc(center, radius, depth, true);
}

void MapTerrainEngine::addRange(const QPointF& start, const QPointF& end, float radius, float height, float jitter) {
    const QPointF delta = end - start;
    const float length = std::max(1.0, std::hypot(delta.x(), delta.y()));
    const int steps = std::max(2, static_cast<int>(length / std::max(1.0f, radius * .35f)));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        QPointF point = start + delta * t;
        point += QPointF((noise(i, 11, 0x51ed270bu) - .5f) * radius * jitter,
                         (noise(i, 29, 0x7f4a7c15u) - .5f) * radius * jitter);
        const float localHeight = height * (.88f + noise(i, 7, 0x94d049bbu) * .24f);
        applyDisc(point, radius * (.82f + noise(i, 3, 0x369dea0fu) * .36f), localHeight, false);
    }
}

void MapTerrainEngine::addTrough(const QPointF& start, const QPointF& end, float radius, float depth, float jitter) {
    const QPointF delta = end - start;
    const float length = std::max(1.0, std::hypot(delta.x(), delta.y()));
    const int steps = std::max(2, static_cast<int>(length / std::max(1.0f, radius * .35f)));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        QPointF point = start + delta * t;
        point += QPointF((noise(i, 5, 0xa24baed5u) - .5f) * radius * jitter,
                         (noise(i, 17, 0x9fb21c65u) - .5f) * radius * jitter);
        applyDisc(point, radius, depth, true);
    }
}

void MapTerrainEngine::carveStrait(bool vertical, float center, float width, float depth) {
    const float half = std::max(1.0f, width * .5f);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const float axis = vertical ? static_cast<float>(x) : static_cast<float>(y);
            const float wobble = (vertical ? noise(y, 1, 0x243f6a88u) : noise(x, 1, 0x85a308d3u)) - .5f;
            const float localCenter = center + wobble * half * .9f;
            const float distance = std::abs(axis - localCenter);
            if (distance > half) continue;
            const float t = 1.0f - distance / half;
            heights_[index(x, y)] = clamp01(heights_[index(x, y)] - depth * t * t);
        }
    }
}

void MapTerrainEngine::generateWorld(int continentCount, int islandCount, float roughness) {
    continentCount = std::clamp(continentCount, 1, 8);
    islandCount = std::clamp(islandCount, 0, 40);
    roughness = std::clamp(roughness, .05f, 1.0f);
    clear(.04f);

    const float minDim = static_cast<float>(std::min(width_, height_));
    for (int i = 0; i < continentCount; ++i) {
        const float cx = width_ * (.16f + noise(i, 3, 0x51ed270bu) * .68f);
        const float cy = height_ * (.16f + noise(i, 9, 0x9e3779b9u) * .68f);
        const float radius = minDim * (.18f + noise(i, 17, 0x85ebca6bu) * .13f);
        addHill(QPointF(cx, cy), radius, .62f + noise(i, 23, 0xc2b2ae35u) * .22f);

        const int lobes = 3 + static_cast<int>(noise(i, 31, 0x27d4eb2fu) * 4.0f);
        for (int lobe = 0; lobe < lobes; ++lobe) {
            const float angle = noise(i * 17 + lobe, 5, 0x165667b1u) * 6.2831853f;
            const float distance = radius * (.28f + noise(i * 19 + lobe, 7, 0xd3a2646cu) * .56f);
            const QPointF point(cx + std::cos(angle) * distance, cy + std::sin(angle) * distance);
            addHill(point, radius * (.42f + noise(i + lobe, 41, 0xfd7046c5u) * .42f), .28f + roughness * .28f);
        }

        const float angle = noise(i, 53, 0xb55a4f09u) * 6.2831853f;
        addRange(QPointF(cx - std::cos(angle) * radius * .7f, cy - std::sin(angle) * radius * .7f),
                 QPointF(cx + std::cos(angle) * radius * .7f, cy + std::sin(angle) * radius * .7f),
                 radius * .18f, .16f + roughness * .24f, .22f + roughness * .22f);
    }

    for (int i = 0; i < islandCount; ++i) {
        const float cx = width_ * (.05f + noise(i, 61, 0x94d049bbu) * .90f);
        const float cy = height_ * (.05f + noise(i, 67, 0x369dea0fu) * .90f);
        const float radius = minDim * (.025f + noise(i, 71, 0xa24baed5u) * .065f);
        addHill(QPointF(cx, cy), radius, .38f + noise(i, 73, 0x9fb21c65u) * .32f);
    }

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const float edgeX = std::min(x, width_ - 1 - x) / std::max(1.0f, width_ * .15f);
            const float edgeY = std::min(y, height_ - 1 - y) / std::max(1.0f, height_ * .15f);
            const float edge = clamp01(std::min(edgeX, edgeY));
            const float broadNoise = (noise(x / 5, y / 5, 0x243f6a88u) - .5f) * roughness * .16f;
            const float detailNoise = (noise(x, y, 0x85a308d3u) - .5f) * roughness * .06f;
            float& height = heights_[index(x, y)];
            height = clamp01((height + broadNoise + detailNoise) * (.32f + .68f * edge));
        }
    }

    smooth(2);
    erode(2, .16f + roughness * .08f);
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
                        const int px = x + ox, py = y + oy;
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

void MapTerrainEngine::erode(int passes, float strength) {
    passes = std::clamp(passes, 1, 16);
    strength = std::clamp(strength, .01f, .45f);
    QVector<float> delta(heights_.size());
    for (int pass = 0; pass < passes; ++pass) {
        std::fill(delta.begin(), delta.end(), 0.0f);
        for (int y = 1; y < height_ - 1; ++y) {
            for (int x = 1; x < width_ - 1; ++x) {
                const int currentIndex = index(x, y);
                const float current = heights_.at(currentIndex);
                int lowX = x, lowY = y;
                float low = current;
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (!ox && !oy) continue;
                        const float candidate = at(x + ox, y + oy);
                        if (candidate < low) { low = candidate; lowX = x + ox; lowY = y + oy; }
                    }
                }
                const float slope = current - low;
                if (slope < .035f) continue;
                const float moved = std::min(slope * strength, current * .08f);
                delta[currentIndex] -= moved;
                delta[index(lowX, lowY)] += moved * .82f;
            }
        }
        for (qsizetype i = 0; i < heights_.size(); ++i) heights_[i] = clamp01(heights_.at(i) + delta.at(i));
    }
}

void MapTerrainEngine::normalize() {
    if (heights_.isEmpty()) return;
    const auto range = std::minmax_element(heights_.cbegin(), heights_.cend());
    const float low = *range.first;
    const float high = *range.second;
    const float span = std::max(.0001f, high - low);
    for (float& height : heights_) height = clamp01((height - low) / span);
}

QPointF MapTerrainEngine::steepestNeighbor(int x, int y) const {
    float best = at(x, y);
    QPointF result(x, y);
    for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
            if (!ox && !oy) continue;
            const int nx = x + ox, ny = y + oy;
            if (nx < 0 || ny < 0 || nx >= width_ || ny >= height_) continue;
            const float candidate = at(nx, ny);
            if (candidate < best) { best = candidate; result = QPointF(nx, ny); }
        }
    }
    return result;
}

QJsonArray MapTerrainEngine::traceRivers(int count, float seaLevel) const {
    count = std::clamp(count, 0, 80);
    seaLevel = std::clamp(seaLevel, .05f, .85f);
    struct Candidate { int x; int y; float score; };
    QVector<Candidate> candidates;
    for (int y = 2; y < height_ - 2; y += 2) {
        for (int x = 2; x < width_ - 2; x += 2) {
            const float height = at(x, y);
            if (height < seaLevel + .18f) continue;
            candidates.append({x, y, height + noise(x, y, 0x7f4a7c15u) * .12f});
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

    QJsonArray rivers;
    QSet<quint64> occupied;
    for (const Candidate& candidate : candidates) {
        if (rivers.size() >= count) break;
        bool nearExisting = false;
        for (int oy = -4; oy <= 4 && !nearExisting; ++oy) {
            for (int ox = -4; ox <= 4; ++ox) {
                const quint64 key = (static_cast<quint64>(candidate.x + ox) << 32) | static_cast<quint32>(candidate.y + oy);
                if (occupied.contains(key)) { nearExisting = true; break; }
            }
        }
        if (nearExisting) continue;

        int x = candidate.x, y = candidate.y;
        QSet<quint64> visited;
        QJsonArray points;
        for (int step = 0; step < width_ + height_; ++step) {
            const quint64 key = (static_cast<quint64>(x) << 32) | static_cast<quint32>(y);
            if (visited.contains(key)) break;
            visited.insert(key);
            occupied.insert(key);
            if (step % 2 == 0) {
                points.append(QJsonObject{{QStringLiteral("x"), static_cast<double>(x) / std::max(1, width_ - 1)},
                                          {QStringLiteral("y"), static_cast<double>(y) / std::max(1, height_ - 1)}});
            }
            if (at(x, y) <= seaLevel || x <= 1 || y <= 1 || x >= width_ - 2 || y >= height_ - 2) break;
            const QPointF next = steepestNeighbor(x, y);
            if (static_cast<int>(next.x()) == x && static_cast<int>(next.y()) == y) break;
            x = static_cast<int>(next.x());
            y = static_cast<int>(next.y());
        }
        if (points.size() >= 5) rivers.append(points);
    }
    return rivers;
}

QJsonObject MapTerrainEngine::toJson() const {
    QJsonArray values;
    for (float height : heights_) values.append(static_cast<int>(std::round(clamp01(height) * 255.0f)));
    return QJsonObject{{QStringLiteral("width"), width_},
                       {QStringLiteral("height"), height_},
                       {QStringLiteral("seed"), static_cast<qint64>(seed_)},
                       {QStringLiteral("values"), values},
                       {QStringLiteral("seaLevel"), .42}};
}

MapTerrainEngine MapTerrainEngine::fromJson(const QJsonObject& object) {
    MapTerrainEngine engine(object.value(QStringLiteral("width")).toInt(256),
                            object.value(QStringLiteral("height")).toInt(144),
                            static_cast<quint32>(object.value(QStringLiteral("seed")).toInteger(1)));
    const QJsonArray values = object.value(QStringLiteral("values")).toArray();
    const qsizetype count = std::min(values.size(), engine.heights_.size());
    for (qsizetype i = 0; i < count; ++i)
        engine.heights_[i] = clamp01(static_cast<float>(values.at(i).toInt()) / 255.0f);
    return engine;
}

} // namespace wbw
