#include "DistrhoUI.hpp"

#include "floozy_params.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

START_NAMESPACE_DISTRHO

namespace {

using downspout::floozy::ParamId;
using downspout::floozy::kInterfaceTypeNames;
using downspout::floozy::kParameterCount;
using downspout::floozy::kParameterSpecs;
using downspout::floozy::kSourceAlgorithmNames;

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    [[nodiscard]] bool contains(const float px, const float py) const noexcept
    {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

struct Color {
    int r;
    int g;
    int b;
};

struct ControlDef {
    std::uint32_t parameter;
    const char* label;
};

struct SectionDef {
    const char* title;
    const Color color;
    const std::array<ControlDef, 6> controls;
    std::size_t count;
};

constexpr std::array<SectionDef, 5> kSections = {{
    {"Source", {219, 132, 84}, {{{0, "Algorithm"}, {1, "Param 1"}, {2, "Param 2"}, {3, "Tone"}, {4, "Noise"}, {5, "DC"}}}, 6},
    {"Shape", {92, 178, 164}, {{{6, "Attack"}, {7, "Release"}, {8, "Interface"}, {9, "Intensity"}, {22, "Gain"}, {0, ""}}}, 5},
    {"Body", {126, 153, 221}, {{{10, "Tuning"}, {11, "Ratio"}, {12, "Delay 1"}, {13, "Delay 2"}, {14, "Feedback"}, {0, ""}}}, 5},
    {"Filter/Mod", {181, 137, 216}, {{{15, "Cutoff"}, {16, "Q"}, {17, "Shape"}, {18, "LFO"}, {19, "Mod Mix"}, {0, ""}}}, 5},
    {"Space", {211, 165, 82}, {{{20, "Size"}, {21, "Level"}, {0, ""}, {0, ""}, {0, ""}, {0, ""}}}, 2},
}};

float clampf(const float value, const float minimum, const float maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

float normalizedValue(const std::uint32_t parameter, const float value)
{
    const auto& spec = kParameterSpecs[parameter];
    if (spec.maximum <= spec.minimum)
        return 0.0f;
    return clampf((value - spec.minimum) / (spec.maximum - spec.minimum), 0.0f, 1.0f);
}

std::string formatValue(const std::uint32_t parameter, const float value)
{
    char buffer[64];
    if (parameter == static_cast<std::uint32_t>(ParamId::sourceAlgorithm))
    {
        const int index = std::clamp(static_cast<int>(std::lround(value)), 0, 6);
        std::snprintf(buffer, sizeof(buffer), "%s", kSourceAlgorithmNames[static_cast<std::size_t>(index)]);
    }
    else if (parameter == static_cast<std::uint32_t>(ParamId::interfaceType))
    {
        const int index = std::clamp(static_cast<int>(std::lround(value)), 0, 11);
        std::snprintf(buffer, sizeof(buffer), "%s", kInterfaceTypeNames[static_cast<std::size_t>(index)]);
    }
    else
    {
        std::snprintf(buffer, sizeof(buffer), "%d%%", static_cast<int>(std::lround(value * 100.0f)));
    }
    return buffer;
}

} // namespace

class FloozyUI : public UI
{
public:
    FloozyUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        for (std::uint32_t i = 0; i < kParameterCount; ++i)
            values_[i] = kParameterSpecs[i].defaultValue;

       #ifdef DGL_NO_SHARED_RESOURCES
        createFontFromFile("sans", "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf");
       #else
        loadSharedResources();
       #endif
    }

protected:
    void parameterChanged(const uint32_t index, const float value) override
    {
        if (index < values_.size())
        {
            values_[index] = value;
            repaint();
        }
    }

    void onNanoDisplay() override
    {
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        const float pad = 22.0f;

        drawBackground(width, height);
        drawHeader(pad, pad, width - pad * 2.0f, 78.0f);

        const float top = 126.0f;
        const float gap = 14.0f;
        const float sectionW = (width - pad * 2.0f - gap * 4.0f) / 5.0f;
        const float sectionH = height - top - pad;
        for (std::size_t i = 0; i < kSections.size(); ++i)
        {
            const Rect rect {pad + static_cast<float>(i) * (sectionW + gap), top, sectionW, sectionH};
            drawSection(i, rect);
        }
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1)
            return false;

        if (!ev.press)
        {
            activeParameter_ = -1;
            return false;
        }

        const float x = static_cast<float>(ev.pos.getX());
        const float y = static_cast<float>(ev.pos.getY());
        for (std::size_t i = 0; i < controlRects_.size(); ++i)
        {
            if (controlRects_[i].contains(x, y))
            {
                activeParameter_ = static_cast<int>(i);
                updateParameterFromPosition(static_cast<std::uint32_t>(i), x, controlRects_[i]);
                return true;
            }
        }

        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (activeParameter_ < 0)
            return false;

        const std::uint32_t parameter = static_cast<std::uint32_t>(activeParameter_);
        updateParameterFromPosition(parameter, static_cast<float>(ev.pos.getX()), controlRects_[parameter]);
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float x = static_cast<float>(ev.pos.getX());
        const float y = static_cast<float>(ev.pos.getY());
        const float direction = ev.delta.getY() > 0.0f ? 1.0f : -1.0f;
        for (std::size_t i = 0; i < controlRects_.size(); ++i)
        {
            if (controlRects_[i].contains(x, y))
            {
                const auto& spec = kParameterSpecs[i];
                nudgeParameter(static_cast<std::uint32_t>(i), direction * (spec.integer ? 1.0f : 0.02f));
                return true;
            }
        }
        return false;
    }

private:
    std::array<float, kParameterCount> values_ {};
    std::array<Rect, kParameterCount> controlRects_ {};
    int activeParameter_ = -1;

    void drawBackground(const float width, const float height)
    {
        beginPath();
        fillColor(12, 14, 15, 255);
        rect(0.0f, 0.0f, width, height);
        fill();
        closePath();

        beginPath();
        fillColor(29, 33, 33, 255);
        rect(0.0f, 0.0f, width, 116.0f);
        fill();
        closePath();
    }

    void drawHeader(const float x, const float y, const float w, const float h)
    {
        beginPath();
        roundedRect(x, y, w, h, 7.0f);
        fillColor(18, 22, 22, 238);
        fill();
        strokeColor(52, 60, 60, 255);
        strokeWidth(1.0f);
        stroke();
        closePath();

        fontSize(32.0f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(238, 241, 237, 255);
        text(x + 22.0f, y + 14.0f, "Floozy", nullptr);

        fontSize(13.0f);
        fillColor(155, 166, 164, 255);
        text(x + 170.0f, y + 27.0f, "8 voice hybrid physical/modulation synth", nullptr);

        fontSize(12.0f);
        textAlign(ALIGN_RIGHT | ALIGN_TOP);
        fillColor(204, 214, 210, 255);
        text(x + w - 22.0f, y + 29.0f, "MIDI in | stereo out", nullptr);
    }

    void drawSection(const std::size_t index, const Rect& rect)
    {
        const SectionDef& section = kSections[index];
        beginPath();
        roundedRect(rect.x, rect.y, rect.w, rect.h, 7.0f);
        fillColor(18, 22, 23, 238);
        fill();
        strokeColor(46, 54, 55, 255);
        strokeWidth(1.0f);
        stroke();
        closePath();

        beginPath();
        roundedRect(rect.x + 12.0f, rect.y + 12.0f, rect.w - 24.0f, 7.0f, 3.5f);
        fillColor(section.color.r, section.color.g, section.color.b, 230);
        fill();
        closePath();

        fontSize(17.0f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(232, 237, 233, 255);
        text(rect.x + 14.0f, rect.y + 33.0f, section.title, nullptr);

        const float startY = rect.y + 78.0f;
        const float rowH = 46.0f;
        const float rowGap = 17.0f;
        for (std::size_t i = 0; i < section.count; ++i)
        {
            const Rect control {rect.x + 14.0f, startY + static_cast<float>(i) * (rowH + rowGap), rect.w - 28.0f, rowH};
            controlRects_[section.controls[i].parameter] = control;
            drawControl(section.controls[i], control, section.color);
        }
    }

    void drawControl(const ControlDef& control, const Rect& rect, const Color& color)
    {
        const float value = values_[control.parameter];
        const float norm = normalizedValue(control.parameter, value);
        const std::string textValue = formatValue(control.parameter, value);

        fontSize(12.0f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(213, 220, 217, 255);
        text(rect.x, rect.y, control.label, nullptr);

        fontSize(10.5f);
        textAlign(ALIGN_RIGHT | ALIGN_TOP);
        fillColor(color.r, color.g, color.b, 255);
        text(rect.x + rect.w, rect.y + 1.0f, textValue.c_str(), nullptr);

        const float barY = rect.y + 23.0f;
        beginPath();
        roundedRect(rect.x, barY, rect.w, 12.0f, 6.0f);
        fillColor(38, 44, 45, 255);
        fill();
        closePath();

        beginPath();
        roundedRect(rect.x, barY, std::max(8.0f, rect.w * norm), 12.0f, 6.0f);
        fillColor(color.r, color.g, color.b, 220);
        fill();
        closePath();

        beginPath();
        roundedRect(rect.x + rect.w * norm - 5.0f, barY - 4.0f, 10.0f, 20.0f, 5.0f);
        fillColor(237, 242, 238, 255);
        fill();
        closePath();
    }

    void updateParameterFromPosition(const std::uint32_t parameter, const float x, const Rect& rect)
    {
        if (parameter >= kParameterCount)
            return;
        const auto& spec = kParameterSpecs[parameter];
        const float norm = clampf((x - rect.x) / rect.w, 0.0f, 1.0f);
        commitParameter(parameter, spec.minimum + norm * (spec.maximum - spec.minimum));
    }

    void nudgeParameter(const std::uint32_t parameter, const float delta)
    {
        if (parameter >= kParameterCount)
            return;
        commitParameter(parameter, values_[parameter] + delta);
    }

    void commitParameter(const std::uint32_t parameter, const float value)
    {
        if (parameter >= kParameterCount)
            return;

        const auto& spec = kParameterSpecs[parameter];
        float clamped = clampf(value, spec.minimum, spec.maximum);
        if (spec.integer)
            clamped = std::round(clamped);

        editParameter(parameter, true);
        setParameterValue(parameter, clamped);
        editParameter(parameter, false);
        values_[parameter] = clamped;
        repaint();
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FloozyUI)
};

UI* createUI()
{
    return new FloozyUI();
}

END_NAMESPACE_DISTRHO
