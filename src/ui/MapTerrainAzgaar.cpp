#include "ui/MapTerrainEngine.h"

#include <algorithm>
#include <cmath>

namespace wbw {
namespace {

float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

} // namespace

void MapTerrainEngine::modify(float minHeight, float maxHeight, float add,
                              float multiply, float power, bool preserveLandFloor) {
    minHeight = clamp01(minHeight);
    maxHeight = clamp01(maxHeight);
    if (minHeight > maxHeight) std::swap(minHeight, maxHeight);
    const float floor = preserveLandFloor ? .20f : 0.0f;

    for (float& h : heights_) {
        if (h < minHeight || h > maxHeight) continue;
        if (add != 0.0f) h = preserveLandFloor ? std::max(floor, h + add) : h + add;
        if (multiply != 1.0f) {
            h = preserveLandFloor ? (h - floor) * multiply + floor : h * multiply;
        }
        if (power != 0.0f) {
            const float base = preserveLandFloor ? std::max(0.0f, h - floor) : std::max(0.0f, h);
            h = (preserveLandFloor ? floor : 0.0f) + std::pow(base, power);
        }
        h = clamp01(h);
    }
}

void MapTerrainEngine::mask(float power) {
    const float fraction = power == 0.0f ? 1.0f : std::abs(power);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const float nx = 2.0f * static_cast<float>(x) / std::max(1, width_ - 1) - 1.0f;
            const float ny = 2.0f * static_cast<float>(y) / std::max(1, height_ - 1) - 1.0f;
            float distance = (1.0f - nx * nx) * (1.0f - ny * ny);
            distance = std::clamp(distance, 0.0f, 1.0f);
            if (power < 0.0f) distance = 1.0f - distance;
            const int i = index(x, y);
            const float masked = heights_[i] * distance;
            heights_[i] = clamp01((heights_[i] * (fraction - 1.0f) + masked) / std::max(.001f, fraction));
        }
    }
}

void MapTerrainEngine::invert(bool xAxis, bool yAxis) {
    QVector<float> inverted(heights_.size(), 0.0f);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const int nx = xAxis ? width_ - x - 1 : x;
            const int ny = yAxis ? height_ - y - 1 : y;
            inverted[index(x, y)] = heights_.at(index(nx, ny));
        }
    }
    heights_.swap(inverted);
}

void MapTerrainEngine::generateAzgaarTemplate(const QString& templateId) {
    clear(0.0f);
    int cursor = 0;
    auto random01 = [&]() {
        return noise(cursor++, cursor * 17 + 11, 0x51ed270bu);
    };
    auto value = [&](float low, float high) {
        return low + (high - low) * random01();
    };
    auto count = [&](float expected) {
        const int whole = static_cast<int>(std::floor(expected));
        return whole + (random01() < expected - whole ? 1 : 0);
    };
    auto point = [&](float x1, float x2, float y1, float y2) {
        return QPointF(value(x1, x2) * width_, value(y1, y2) * height_);
    };
    auto hill = [&](float expected, float h1, float h2, float x1, float x2, float y1, float y2) {
        for (int i = 0; i < count(expected); ++i) {
            const QPointF p = point(x1, x2, y1, y2);
            const float radius = std::min(width_, height_) * value(.10f, .24f);
            addHill(p, radius, value(h1, h2));
        }
    };
    auto pit = [&](float expected, float h1, float h2, float x1, float x2, float y1, float y2) {
        for (int i = 0; i < count(expected); ++i) {
            const QPointF p = point(x1, x2, y1, y2);
            const float radius = std::min(width_, height_) * value(.08f, .20f);
            addPit(p, radius, value(h1, h2));
        }
    };
    auto ridge = [&](float expected, float h1, float h2, float x1, float x2, float y1, float y2) {
        for (int i = 0; i < count(expected); ++i) {
            const QPointF start = point(x1, x2, y1, y2);
            const QPointF end = point(.10f, .90f, .15f, .85f);
            addRange(start, end, std::min(width_, height_) * value(.035f, .075f), value(h1, h2), .24f);
        }
    };
    auto trough = [&](float expected, float h1, float h2, float x1, float x2, float y1, float y2) {
        for (int i = 0; i < count(expected); ++i) {
            const QPointF start = point(x1, x2, y1, y2);
            const QPointF end = point(.10f, .90f, .15f, .85f);
            addTrough(start, end, std::min(width_, height_) * value(.025f, .060f), value(h1, h2), .28f);
        }
    };
    auto strait = [&](float widthPercent, bool vertical) {
        const float center = vertical ? width_ * value(.35f, .65f) : height_ * value(.35f, .65f);
        const float span = (vertical ? width_ : height_) * widthPercent;
        carveStrait(vertical, center, std::max(2.0f, span), .70f);
    };
    auto addAll = [&](float amount) { modify(0.0f, 1.0f, amount, 1.0f, 0.0f, false); };
    auto multiplyLand = [&](float factor) { modify(.20f, 1.0f, 0.0f, factor, 0.0f, true); };
    auto multiplyRange = [&](float factor, float low, float high) { modify(low, high, 0.0f, factor, 0.0f, false); };

    const QString id = templateId.trimmed().toLower();

    if (id == QStringLiteral("volcano")) {
        hill(1, .90f, 1.00f, .44f, .56f, .40f, .60f);
        multiplyRange(.8f, .50f, 1.00f);
        ridge(1.5f, .30f, .55f, .45f, .55f, .40f, .60f);
        smooth(3);
        hill(1.5f, .35f, .45f, .25f, .30f, .20f, .75f);
        hill(1, .35f, .55f, .75f, .80f, .25f, .75f);
        hill(.5f, .20f, .25f, .10f, .15f, .20f, .25f);
        mask(3);
    } else if (id == QStringLiteral("highisland")) {
        hill(1, .90f, 1.00f, .65f, .75f, .47f, .53f);
        addAll(.07f);
        hill(5.5f, .20f, .30f, .25f, .55f, .45f, .55f);
        ridge(1, .40f, .50f, .45f, .55f, .45f, .55f);
        multiplyLand(.8f); mask(3); smooth(2);
        trough(2.5f, .20f, .30f, .20f, .30f, .20f, .30f);
        trough(2.5f, .20f, .30f, .60f, .80f, .70f, .80f);
        hill(1, .10f, .15f, .60f, .60f, .50f, .50f);
        hill(1.5f, .13f, .16f, .15f, .20f, .20f, .75f);
        ridge(1.5f, .30f, .40f, .15f, .85f, .30f, .40f);
        ridge(1.5f, .30f, .40f, .15f, .85f, .60f, .70f);
        pit(4, .10f, .30f, .15f, .85f, .20f, .80f);
    } else if (id == QStringLiteral("lowisland")) {
        hill(1, .90f, .99f, .60f, .80f, .45f, .55f);
        hill(1.5f, .20f, .30f, .10f, .30f, .10f, .90f);
        smooth(2);
        hill(6.5f, .25f, .35f, .20f, .70f, .30f, .70f);
        ridge(1, .40f, .50f, .45f, .55f, .45f, .55f);
        trough(2.5f, .20f, .30f, .15f, .85f, .20f, .30f);
        trough(2.5f, .20f, .30f, .15f, .85f, .70f, .80f);
        hill(1.5f, .10f, .15f, .05f, .15f, .20f, .80f);
        hill(1, .10f, .15f, .85f, .95f, .70f, .80f);
        pit(6, .15f, .25f, .15f, .85f, .20f, .80f);
        multiplyRange(.4f, .20f, 1.0f); mask(4);
    } else if (id == QStringLiteral("archipelago")) {
        addAll(.11f);
        ridge(2.5f, .40f, .60f, .20f, .80f, .20f, .80f);
        hill(5, .15f, .20f, .10f, .90f, .30f, .70f);
        hill(2, .10f, .15f, .10f, .30f, .20f, .80f);
        hill(2, .10f, .15f, .60f, .90f, .20f, .80f);
        smooth(3);
        trough(10, .20f, .30f, .05f, .95f, .05f, .95f);
        strait(.02f, true); strait(.02f, false);
    } else if (id == QStringLiteral("atoll")) {
        hill(1, .75f, .80f, .50f, .60f, .45f, .55f);
        hill(1.5f, .30f, .50f, .25f, .75f, .30f, .70f);
        hill(.5f, .30f, .50f, .25f, .35f, .30f, .70f);
        smooth(1); multiplyRange(.2f, .25f, 1.0f);
        hill(.5f, .10f, .20f, .50f, .55f, .48f, .52f);
    } else if (id == QStringLiteral("mediterranean")) {
        ridge(5, .30f, .80f, 0.0f, 1.0f, 0.0f, .10f);
        ridge(5, .30f, .80f, 0.0f, 1.0f, .90f, 1.0f);
        hill(7, .30f, .50f, .10f, .90f, 0.0f, .05f);
        hill(7, .30f, .50f, .10f, .90f, .95f, 1.0f);
        multiplyLand(.9f); mask(-2); smooth(1);
        hill(2.5f, .30f, .70f, 0.0f, .05f, .20f, .80f);
        hill(2.5f, .30f, .70f, .95f, 1.0f, .20f, .80f);
        trough(4.5f, .40f, .50f, 0.0f, 1.0f, 0.0f, .10f);
        trough(4.5f, .40f, .50f, 0.0f, 1.0f, .90f, 1.0f);
    } else if (id == QStringLiteral("peninsula")) {
        ridge(2.5f, .20f, .35f, .40f, .50f, 0.0f, .15f);
        addAll(.05f);
        hill(1, .90f, 1.00f, .10f, .90f, 0.0f, .05f);
        addAll(.13f);
        hill(3.5f, .03f, .05f, .05f, .95f, .80f, 1.0f);
        hill(1.5f, .03f, .05f, .05f, .95f, .40f, .60f);
        trough(5.5f, .10f, .25f, .05f, .95f, .05f, .95f);
        smooth(3); invert(true, true);
    } else if (id == QStringLiteral("pangea")) {
        hill(1.5f, .25f, .40f, .15f, .50f, 0.0f, .10f);
        hill(1.5f, .05f, .40f, .50f, .85f, 0.0f, .10f);
        hill(1.5f, .25f, .40f, .50f, .85f, .90f, 1.0f);
        hill(1.5f, .05f, .40f, .15f, .50f, .90f, 1.0f);
        hill(10, .20f, .40f, .20f, .80f, .48f, .52f);
        smooth(2); multiplyLand(.7f);
        trough(3.5f, .25f, .35f, .05f, .95f, .10f, .20f);
        trough(3.5f, .25f, .35f, .05f, .95f, .80f, .90f);
        ridge(5.5f, .30f, .40f, .10f, .90f, .35f, .65f);
    } else if (id == QStringLiteral("isthmus")) {
        hill(7.5f, .15f, .30f, 0.0f, .30f, 0.0f, .20f);
        hill(7.5f, .15f, .30f, .10f, .50f, .20f, .40f);
        hill(7.5f, .15f, .30f, .30f, .70f, .40f, .60f);
        hill(7.5f, .15f, .30f, .50f, .90f, .60f, .80f);
        hill(7.5f, .15f, .30f, .70f, 1.0f, .80f, 1.0f);
        smooth(2);
        trough(6, .15f, .30f, 0.0f, .30f, 0.0f, .20f);
        trough(6, .15f, .30f, .10f, .50f, .20f, .40f);
        trough(6, .15f, .30f, .30f, .70f, .40f, .60f);
        trough(6, .15f, .30f, .50f, .90f, .60f, .80f);
        trough(6, .15f, .30f, .70f, 1.0f, .80f, 1.0f);
        if (random01() < .25f) invert(true, false);
    } else if (id == QStringLiteral("shattered")) {
        hill(8, .35f, .40f, .15f, .85f, .30f, .70f);
        trough(15, .40f, .50f, .05f, .95f, .05f, .95f);
        ridge(6, .30f, .40f, .10f, .90f, .20f, .80f);
        pit(16, .30f, .40f, .15f, .85f, .20f, .80f);
    } else if (id == QStringLiteral("taklamakan")) {
        hill(2, .20f, .30f, .30f, .70f, .30f, .70f);
        hill(3, .60f, .85f, 0.0f, .05f, 0.0f, 1.0f);
        hill(3, .60f, .85f, .95f, 1.0f, 0.0f, 1.0f);
        hill(3.5f, .60f, .85f, .20f, .80f, 0.0f, .05f);
        hill(3.5f, .60f, .85f, .20f, .80f, .95f, 1.0f);
        smooth(3);
    } else if (id == QStringLiteral("oldworld")) {
        ridge(3, .70f, .70f, .15f, .85f, .20f, .80f);
        hill(2.5f, .50f, .70f, .15f, .45f, .20f, .80f);
        hill(2.5f, .50f, .70f, .65f, .85f, .20f, .80f);
        hill(5, .20f, .25f, .15f, .85f, .20f, .80f);
        multiplyLand(.5f); smooth(2);
        ridge(3.5f, .20f, .50f, .15f, .35f, .20f, .45f);
        ridge(3, .20f, .50f, .65f, .85f, .45f, .80f);
        strait(value(.03f, .07f), true);
        trough(7, .20f, .50f, .15f, .85f, .45f, .65f);
        pit(5.5f, .20f, .30f, .10f, .90f, .10f, .90f);
    } else if (id == QStringLiteral("fractious")) {
        hill(13.5f, .50f, .80f, .05f, .95f, .05f, .95f);
        mask(-1.5f); mask(3.0f);
        modify(.30f, 1.0f, -.20f, 1.0f, 0.0f, false);
        ridge(7, .40f, .50f, .05f, .95f, .10f, .90f);
    } else {
        // Upstream's "Continents" template is the default because it gives a
        // useful editable world rather than a featureless random bitmap.
        hill(1, .80f, .85f, .60f, .80f, .40f, .60f);
        hill(1, .80f, .85f, .20f, .30f, .40f, .60f);
        hill(6.5f, .15f, .30f, .25f, .75f, .15f, .85f);
        multiplyLand(.6f);
        hill(9, .05f, .10f, .15f, .85f, .20f, .80f);
        ridge(1.5f, .30f, .60f, .05f, .15f, .25f, .75f);
        ridge(1.5f, .30f, .60f, .80f, .95f, .25f, .75f);
        ridge(1.5f, .30f, .60f, .80f, .90f, .20f, .80f);
        strait(.02f, true); strait(.01f, true);
        smooth(3);
        trough(3.5f, .15f, .20f, .15f, .85f, .20f, .80f);
        trough(3.5f, .05f, .10f, .45f, .55f, .45f, .55f);
        pit(3.5f, .10f, .20f, .15f, .85f, .20f, .80f);
        mask(4);
    }

    normalize();
}

} // namespace wbw
