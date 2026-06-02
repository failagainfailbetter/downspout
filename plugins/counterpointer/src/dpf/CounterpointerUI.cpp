#include "DistrhoUI.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>

START_NAMESPACE_DISTRHO

namespace {

enum ParameterIndex : uint32_t {
    kParamKey = 0,
    kParamScale,
    kParamCycleBars,
    kParamGranularity,
    kParamFollow,
    kParamCounter,
    kParamShortRandom,
    kParamLongRandom,
    kParamDensity,
    kParamRhythmFollow,
    kParamSyncopation,
    kParamConsonance,
    kParamEmbellish,
    kParamRegularity,
    kParamRegister,
    kParamSpan,
    kParamGate,
    kParamVelocityFollow,
    kParamPassInput,
    kParamOutputChannel,
    kParamFreeze,
    kParamActionLearn,
    kParamStatusReady,
    kParamStatusInput,
    kParamStatusOutput,
    kParameterCount
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    [[nodiscard]] bool contains(const float px, const float py) const noexcept
    {
        return px >= x && px <= (x + w) && py >= y && py <= (y + h);
    }
};

struct SliderDef {
    uint32_t index;
    const char* label;
    float min;
    float max;
    bool integer;
};

constexpr std::array<SliderDef, 21> kSliders = {{
    {kParamKey, "KEY", 0.0f, 11.0f, true},
    {kParamScale, "SCALE", 0.0f, 17.0f, true},
    {kParamCycleBars, "BARS", 1.0f, 8.0f, true},
    {kParamGranularity, "GRID", 0.0f, 2.0f, true},
    {kParamFollow, "FOLLOW", 0.0f, 1.0f, false},
    {kParamCounter, "COUNTER", 0.0f, 1.0f, false},
    {kParamEmbellish, "EMBELL", 0.0f, 1.0f, false},
    {kParamDensity, "DENSITY", 0.0f, 1.0f, false},
    {kParamRhythmFollow, "RHYTHM", 0.0f, 1.0f, false},
    {kParamSyncopation, "SYNCOP", 0.0f, 1.0f, false},
    {kParamConsonance, "CONSON", 0.0f, 1.0f, false},
    {kParamRegularity, "REGULAR", 0.0f, 1.0f, false},
    {kParamShortRandom, "SHORT RND", 0.0f, 1.0f, false},
    {kParamLongRandom, "LONG RND", 0.0f, 1.0f, false},
    {kParamRegister, "REG", 0.0f, 2.0f, true},
    {kParamSpan, "SPAN", 0.0f, 1.0f, false},
    {kParamGate, "GATE", 0.10f, 1.0f, false},
    {kParamVelocityFollow, "VELOCITY", 0.0f, 1.0f, false},
    {kParamPassInput, "PASS", 0.0f, 1.0f, true},
    {kParamOutputChannel, "CHAN", 0.0f, 16.0f, true},
    {kParamFreeze, "FREEZE", 0.0f, 1.0f, true},
}};

constexpr const char* kNoteNames[12] = {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

constexpr const char* kScaleNames[18] = {
    "Chrom", "Major", "Nat Min", "Harm Min", "Pent Maj",
    "Pent Min", "Blues", "Dorian", "Mixolyd",
    "Lydian", "Mel Min", "Whole", "Altered", "H-W Dim",
    "W-H Dim", "Bebop Dom", "Bebop Maj", "Bebop Min"
};

constexpr const char* kGranularityNames[3] = {
    "Beat", "Half Bar", "Bar"
};

constexpr const char* kRegisterNames[3] = {
    "Low", "Mid", "High"
};

constexpr const char* kToggleNames[2] = {
    "Off", "On"
};

[[nodiscard]] float clampf(const float value, const float minValue, const float maxValue)
{
    return std::max(minValue, std::min(value, maxValue));
}

[[nodiscard]] int clampi(const int value, const int minValue, const int maxValue)
{
    return std::max(minValue, std::min(value, maxValue));
}

[[nodiscard]] const char* label_from_index(const char* const* labels, const int count, int index)
{
    index = clampi(index, 0, count - 1);
    return labels[index];
}

[[nodiscard]] std::string formatValueLabel(const uint32_t index, const float value)
{
    char buffer[48];
    switch (index) {
    case kParamKey:
        std::snprintf(buffer, sizeof(buffer), "%s", label_from_index(kNoteNames, 12, static_cast<int>(std::lround(value))));
        break;
    case kParamScale:
        std::snprintf(buffer, sizeof(buffer), "%s", label_from_index(kScaleNames, 18, static_cast<int>(std::lround(value))));
        break;
    case kParamCycleBars:
        std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::lround(value)));
        break;
    case kParamGranularity:
        std::snprintf(buffer, sizeof(buffer), "%s", label_from_index(kGranularityNames, 3, static_cast<int>(std::lround(value))));
        break;
    case kParamRegister:
        std::snprintf(buffer, sizeof(buffer), "%s", label_from_index(kRegisterNames, 3, static_cast<int>(std::lround(value))));
        break;
    case kParamPassInput:
    case kParamFreeze:
        std::snprintf(buffer, sizeof(buffer), "%s", label_from_index(kToggleNames, 2, static_cast<int>(std::lround(value))));
        break;
    case kParamOutputChannel:
        if (static_cast<int>(std::lround(value)) <= 0)
            std::snprintf(buffer, sizeof(buffer), "Input");
        else
            std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::lround(value)));
        break;
    default:
        std::snprintf(buffer, sizeof(buffer), "%d%%", static_cast<int>(std::lround(value * 100.0f)));
        break;
    }
    return buffer;
}

}  // namespace

class CounterpointerUI : public UI
{
public:
    CounterpointerUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        values_.fill(0.0f);
        values_[kParamKey] = 0.0f;
        values_[kParamScale] = 2.0f;
        values_[kParamCycleBars] = 2.0f;
        values_[kParamGranularity] = 0.0f;
        values_[kParamFollow] = 0.55f;
        values_[kParamCounter] = 0.55f;
        values_[kParamShortRandom] = 0.15f;
        values_[kParamLongRandom] = 0.0f;
        values_[kParamDensity] = 0.78f;
        values_[kParamRhythmFollow] = 0.65f;
        values_[kParamSyncopation] = 0.25f;
        values_[kParamConsonance] = 0.75f;
        values_[kParamEmbellish] = 0.25f;
        values_[kParamRegularity] = 0.65f;
        values_[kParamRegister] = 1.0f;
        values_[kParamSpan] = 0.55f;
        values_[kParamGate] = 0.72f;
        values_[kParamVelocityFollow] = 0.65f;
        values_[kParamPassInput] = 1.0f;
        values_[kParamOutputChannel] = 0.0f;
        values_[kParamFreeze] = 0.0f;
        values_[kParamStatusReady] = 0.0f;
        values_[kParamStatusInput] = 0.0f;
        values_[kParamStatusOutput] = 0.0f;

       #ifdef DGL_NO_SHARED_RESOURCES
        createFontFromFile("sans", "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf");
       #else
        loadSharedResources();
       #endif
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < values_.size()) {
            values_[index] = value;
            repaint();
        }
    }

    void uiIdle() override
    {
        if (learnPulse_ > 0) {
            --learnPulse_;
            repaint();
        }
    }

    void onNanoDisplay() override
    {
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        const float pad = 24.0f;

        drawBackground(width, height);
        drawHeader(pad, pad, width - pad * 2.0f, 92.0f);
        drawSliders(pad, pad + 118.0f, width - pad * 2.0f, height - 226.0f);
        drawFooter(pad, height - 86.0f, width - pad * 2.0f, 62.0f);
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1)
            return false;

        const float x = static_cast<float>(ev.pos.getX());
        const float y = static_cast<float>(ev.pos.getY());

        if (!ev.press) {
            activeSlider_ = -1;
            return false;
        }

        if (learnButtonRect_.contains(x, y)) {
            triggerLearn();
            return true;
        }
        if (randomizeButtonRect_.contains(x, y)) {
            randomizeSliders();
            return true;
        }

        for (std::size_t i = 0; i < sliderRects_.size(); ++i) {
            if (sliderRects_[i].contains(x, y)) {
                activeSlider_ = static_cast<int>(i);
                updateSliderFromY(activeSlider_, y);
                return true;
            }
        }

        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (activeSlider_ < 0)
            return false;

        updateSliderFromY(activeSlider_, static_cast<float>(ev.pos.getY()));
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float x = static_cast<float>(ev.pos.getX());
        const float y = static_cast<float>(ev.pos.getY());

        for (std::size_t i = 0; i < sliderRects_.size(); ++i) {
            if (sliderRects_[i].contains(x, y)) {
                nudgeSlider(static_cast<int>(i), ev.delta.getY() > 0.0f ? 1.0f : -1.0f);
                return true;
            }
        }

        return false;
    }

private:
    std::array<float, kParameterCount> values_ {};
    std::array<Rect, kSliders.size()> sliderRects_ {};
    Rect learnButtonRect_ {};
    Rect randomizeButtonRect_ {};
    int activeSlider_ = -1;
    int learnPulse_ = 0;
    std::uint32_t randomState_ = 0x51f2a8d3u;

    void drawBackground(const float width, const float height)
    {
        beginPath();
        fillColor(8, 13, 14, 255);
        rect(0.0f, 0.0f, width, height);
        fill();
        closePath();

        beginPath();
        fillColor(17, 36, 33, 255);
        rect(0.0f, 0.0f, width, height * 0.34f);
        fill();
        closePath();

        beginPath();
        fillColor(55, 176, 151, 22);
        roundedRect(width - 330.0f, 34.0f, 280.0f, 240.0f, 46.0f);
        fill();
        closePath();

        beginPath();
        fillColor(231, 194, 91, 18);
        roundedRect(38.0f, height - 230.0f, 240.0f, 180.0f, 34.0f);
        fill();
        closePath();
    }

    void drawHeader(const float x, const float y, const float w, const float h)
    {
        beginPath();
        roundedRect(x, y, w, h, 20.0f);
        fillColor(13, 24, 25, 238);
        fill();
        closePath();

        beginPath();
        roundedRect(x, y, w, 5.0f, 2.0f);
        fillColor(69, 203, 169, 255);
        fill();
        closePath();

        fontSize(30.0f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(239, 246, 241, 255);
        text(x + 24.0f, y + 18.0f, "Counterpointer", nullptr);

        fontSize(13.0f);
        fillColor(166, 184, 178, 255);
        text(x + 26.0f, y + 56.0f, "Learns incoming MIDI and answers with a transport-locked counter-melody", nullptr);

        const float pillY = y + 22.0f;
        drawReadyPill(x + w - 394.0f, pillY, 154.0f, 32.0f, values_[kParamStatusReady] >= 0.5f);
        drawActivityPill(x + w - 226.0f, pillY, 96.0f, 32.0f, "MIDI In", values_[kParamStatusInput]);
        drawActivityPill(x + w - 116.0f, pillY, 96.0f, 32.0f, "MIDI Out", values_[kParamStatusOutput]);
    }

    void drawReadyPill(const float x, const float y, const float w, const float h, const bool ready)
    {
        beginPath();
        roundedRect(x, y, w, h, h * 0.5f);
        fillColor(ready ? 55 : 91, ready ? 160 : 88, ready ? 111 : 74, 42);
        fill();
        strokeColor(ready ? 93 : 183, ready ? 223 : 139, ready ? 157 : 103, 190);
        strokeWidth(1.2f);
        stroke();
        closePath();

        fontSize(12.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(ready ? 204 : 236, ready ? 247 : 198, ready ? 222 : 166, 255);
        text(x + w * 0.5f, y + h * 0.5f + 1.0f, ready ? "Phrase Ready" : "Listening", nullptr);
    }

    void drawActivityPill(const float x,
                          const float y,
                          const float w,
                          const float h,
                          const char* label,
                          const float activity)
    {
        const float level = clampf(activity, 0.0f, 1.0f);

        beginPath();
        roundedRect(x, y, w, h, h * 0.5f);
        fillColor(38, 52, 54, 70);
        fill();
        strokeColor(70 + static_cast<int>(level * 90.0f),
                    92 + static_cast<int>(level * 120.0f),
                    96 + static_cast<int>(level * 62.0f),
                    180);
        strokeWidth(1.2f);
        stroke();
        closePath();

        beginPath();
        circle(x + 17.0f, y + h * 0.5f, 5.0f);
        fillColor(83 + static_cast<int>(level * 122.0f),
                  97 + static_cast<int>(level * 135.0f),
                  101 + static_cast<int>(level * 55.0f),
                  255);
        fill();
        closePath();

        fontSize(11.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(206, 221, 216, 255);
        text(x + 28.0f, y + h * 0.5f + 1.0f, label, nullptr);
    }

    void drawSliders(const float x, const float y, const float w, const float h)
    {
        beginPath();
        roundedRect(x, y, w, h, 20.0f);
        fillColor(12, 19, 21, 246);
        fill();
        strokeColor(36, 59, 58, 255);
        strokeWidth(1.0f);
        stroke();
        closePath();

        const int columns = 7;
        const int rows = 3;
        const float innerX = x + 24.0f;
        const float innerY = y + 48.0f;
        const float innerW = w - 48.0f;
        const float innerH = h - 72.0f;
        const float colGap = 12.0f;
        const float rowGap = 22.0f;
        const float colW = (innerW - colGap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
        const float rowH = (innerH - rowGap * static_cast<float>(rows - 1)) / static_cast<float>(rows);

        fontSize(13.0f);
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        fillColor(169, 188, 183, 255);
        text(x + w * 0.5f, y + 18.0f, "Relationship, rhythm, voice and routing", nullptr);

        for (std::size_t i = 0; i < kSliders.size(); ++i) {
            const int row = static_cast<int>(i) / columns;
            const int col = static_cast<int>(i) % columns;
            const float rx = innerX + static_cast<float>(col) * (colW + colGap);
            const float ry = innerY + static_cast<float>(row) * (rowH + rowGap);
            sliderRects_[i] = {rx, ry + 20.0f, colW, rowH - 12.0f};
            drawSlider(kSliders[i], sliderRects_[i], values_[kSliders[i].index], activeSlider_ == static_cast<int>(i));
        }
    }

    void drawSlider(const SliderDef& def, const Rect& rect, const float value, const bool active)
    {
        const float trackW = 34.0f;
        const float trackH = rect.h - 50.0f;
        const float trackX = rect.x + (rect.w - trackW) * 0.5f;
        const float trackY = rect.y + 10.0f;
        const float knobH = 18.0f;

        fontSize(11.0f);
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        fillColor(225, 232, 228, 255);
        text(rect.x + rect.w * 0.5f, rect.y - 18.0f, def.label, nullptr);

        beginPath();
        roundedRect(trackX, trackY, trackW, trackH, 9.0f);
        fillColor(27, 42, 44, 255);
        fill();
        closePath();

        const float denom = std::max(0.0001f, def.max - def.min);
        const float t = clampf((value - def.min) / denom, 0.0f, 1.0f);
        const float fillY = trackY + (1.0f - t) * trackH;
        beginPath();
        roundedRect(trackX, fillY, trackW, trackY + trackH - fillY, 9.0f);
        fillColor(55, 155, 134, active ? 220 : 170);
        fill();
        closePath();

        const float knobY = trackY + (1.0f - t) * (trackH - knobH);
        beginPath();
        roundedRect(trackX - 4.0f, knobY, trackW + 8.0f, knobH, 7.0f);
        fillColor(active ? 243 : 224, active ? 207 : 181, active ? 111 : 80, 255);
        fill();
        closePath();

        fontSize(11.0f);
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        fillColor(178, 194, 190, 255);
        const std::string label = formatValueLabel(def.index, value);
        text(rect.x + rect.w * 0.5f, rect.y + rect.h - 18.0f, label.c_str(), nullptr);
    }

    void drawFooter(const float x, const float y, const float w, const float h)
    {
        const float buttonW = 170.0f;
        const float buttonH = 38.0f;
        learnButtonRect_ = {x + 20.0f, y + 6.0f, buttonW, buttonH};
        drawLearnButton(learnButtonRect_);
        randomizeButtonRect_ = {x + buttonW + 36.0f, y + 6.0f, buttonW, buttonH};
        drawRandomizeButton(randomizeButtonRect_);

        fontSize(12.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(145, 162, 158, 255);
        text(x + buttonW * 2.0f + 76.0f,
             y + buttonH * 0.5f + 6.0f,
             "Embellish adds extra notes per segment. Regularity pushes behavior from irregular to predictable.",
             nullptr);
    }

    void drawLearnButton(const Rect& rect)
    {
        const float pulse = static_cast<float>(learnPulse_) / 10.0f;
        beginPath();
        roundedRect(rect.x, rect.y, rect.w, rect.h, 9.0f);
        fillColor(35 + static_cast<int>(pulse * 34.0f),
                  73 + static_cast<int>(pulse * 42.0f),
                  68 + static_cast<int>(pulse * 34.0f),
                  255);
        fill();
        strokeColor(70, 207, 171, 255);
        strokeWidth(1.5f);
        stroke();
        closePath();

        fontSize(13.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(213, 252, 237, 255);
        text(rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f + 1.0f, "Relearn", nullptr);
    }

    void drawRandomizeButton(const Rect& rect)
    {
        beginPath();
        roundedRect(rect.x, rect.y, rect.w, rect.h, 9.0f);
        fillColor(75, 61, 35, 255);
        fill();
        strokeColor(236, 189, 87, 255);
        strokeWidth(1.5f);
        stroke();
        closePath();

        fontSize(13.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(252, 234, 190, 255);
        text(rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f + 1.0f, "Randomise", nullptr);
    }

    void triggerLearn()
    {
        editParameter(kParamActionLearn, true);
        setParameterValue(kParamActionLearn, 1.0f);
        editParameter(kParamActionLearn, false);
        values_[kParamStatusReady] = 0.0f;
        learnPulse_ = 10;
        repaint();
    }

    std::uint32_t nextRandom()
    {
        std::uint32_t x = randomState_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        randomState_ = x == 0 ? 0x51f2a8d3u : x;
        return randomState_;
    }

    float nextRandomFloat()
    {
        return static_cast<float>(nextRandom() & 0x00ffffffu) / static_cast<float>(0x01000000u);
    }

    void randomizeSliders()
    {
        for (const SliderDef& def : kSliders) {
            float value = def.min + nextRandomFloat() * (def.max - def.min);
            if (def.integer)
                value = std::round(value);
            setParameter(def.index, value);
        }
        repaint();
    }

    void updateSliderFromY(const int sliderIndex, const float mouseY)
    {
        const SliderDef& def = kSliders[static_cast<std::size_t>(sliderIndex)];
        const Rect& rect = sliderRects_[static_cast<std::size_t>(sliderIndex)];
        const float trackY = rect.y + 10.0f;
        const float trackH = rect.h - 50.0f;
        const float knobH = 18.0f;
        float t = 1.0f - ((mouseY - trackY - knobH * 0.5f) / std::max(1.0f, trackH - knobH));
        t = clampf(t, 0.0f, 1.0f);
        float value = def.min + t * (def.max - def.min);
        if (def.integer)
            value = std::round(value);
        setParameter(def.index, value);
    }

    void nudgeSlider(const int sliderIndex, const float direction)
    {
        const SliderDef& def = kSliders[static_cast<std::size_t>(sliderIndex)];
        const float step = def.integer ? 1.0f : ((def.max - def.min) / 100.0f);
        setParameter(def.index, values_[def.index] + direction * step);
    }

    void setParameter(const uint32_t index, float value)
    {
        const SliderDef& def = kSliders[sliderIndexForParameter(index)];
        value = clampf(value, def.min, def.max);
        if (def.integer)
            value = std::round(value);

        editParameter(index, true);
        setParameterValue(index, value);
        editParameter(index, false);
        values_[index] = value;
        repaint();
    }

    [[nodiscard]] std::size_t sliderIndexForParameter(const uint32_t index) const
    {
        for (std::size_t i = 0; i < kSliders.size(); ++i) {
            if (kSliders[i].index == index)
                return i;
        }
        return 0;
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CounterpointerUI)
};

UI* createUI()
{
    return new CounterpointerUI();
}

END_NAMESPACE_DISTRHO
