#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QVector>

namespace wbw {

class MapTerrainEngine {
public:
    MapTerrainEngine(int width = 256, int height = 144, quint32 seed = 1);

    void clear(float value = 0.0f);
    void generateWorld(int continentCount = 3, int islandCount = 10, float roughness = 0.58f);
    void addHill(const QPointF& center, float radius, float height);
    void addPit(const QPointF& center, float radius, float depth);
    void addRange(const QPointF& start, const QPointF& end, float radius, float height, float jitter = 0.18f);
    void addTrough(const QPointF& start, const QPointF& end, float radius, float depth, float jitter = 0.16f);
    void carveStrait(bool vertical, float center, float width, float depth);
    void smooth(int passes = 1);
    void erode(int passes = 1, float strength = 0.18f);
    void normalize();

    QJsonArray traceRivers(int count = 18, float seaLevel = 0.42f) const;
    QJsonObject toJson() const;
    static MapTerrainEngine fromJson(const QJsonObject& object);

    int width() const { return width_; }
    int height() const { return height_; }
    float at(int x, int y) const;

private:
    int index(int x, int y) const { return y * width_ + x; }
    float noise(int x, int y, quint32 salt) const;
    void applyDisc(const QPointF& center, float radius, float amount, bool subtract);
    QPointF steepestNeighbor(int x, int y) const;

    int width_ = 256;
    int height_ = 144;
    quint32 seed_ = 1;
    QVector<float> heights_;
};

} // namespace wbw
