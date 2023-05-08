/*
 * sst-basic-blocks - an open source library of core audio utilities
 * built by Surge Synth Team.
 *
 * Provides a collection of tools useful on the audio thread for blocks,
 * modulation, etc... or useful for adapting code to multiple environments.
 *
 * Copyright 2023, various authors, as described in the GitHub
 * transaction log. Parts of this code are derived from similar
 * functions original in Surge or ShortCircuit.
 *
 * sst-basic-blocks is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * All source in sst-basic-blocks available at
 * https://github.com/surge-synthesizer/sst-basic-blocks
 */

#ifndef INCLUDE_SST_BASIC_BLOCKS_PARAMS_PARAMMETADATA_H
#define INCLUDE_SST_BASIC_BLOCKS_PARAMS_PARAMMETADATA_H

/*
 * ParamMetaData is exactly that; a way to encode the metadata (range, scale, string
 * formtting, string parsing, etc...) for a parameter without specifying how to store
 * an actual runtime value. It is a configuration- and ui- time object mostly which
 * lets you advertise things like natural mins and maxes.
 *
 * Critically it does *not* store the data for a parameter. All the APIs assume the
 * actual value and configuration come from an external source, so multiple clients
 * can adapt to objects which advertise lists of these, like the sst- effects currently
 * do.
 *
 * The coding structure is basically a bunch of bool and value and enum members
 * and then a bunch of modifiers to set them (.withRange(min,max)) or to set clusters
 * of them (.asPercentBipolar()). We can add and expand these methods as we see fit.
 *
 * Please note this class is still a work in active development. There's an lot
 * to do including
 *
 * - string conversion for absolute, extend, etc...
 * - custom string functions
 * - midi notes get note name typeins
 * - alternate displays
 * - and much much more
 *
 * Right now it just has the features paul needed to port flanger and reverb1 to sst-effects
 * so still expect change to be coming.
 */

#include <algorithm>
#include <string>
#include <string_view>
#include <variant>
#include <optional>
#include <unordered_map>
#include <cassert>
#include <cmath>

#include <fmt/core.h>

namespace sst::basic_blocks::params
{
struct ParamMetaData
{
    ParamMetaData() = default;

    enum Type
    {
        FLOAT, // min/max/default value are in natural units
        INT,   // min/max/default value are in natural units, stored as a float. (int)round(val)
        BOOL,  // min/max 0/1. `val > 0.5` is true false test
        NONE   // special signifier that this param has no value. Used for structural things like
               // unused slots
    } type{FLOAT};

    std::string name;

    float minVal{0.f}, maxVal{1.f}, defaultVal{0.f};
    bool canExtend{false}, canDeform{false}, canAbsolute{false}, canTemposync{false},
        canDeactivate{false};

    int deformationCount{0};

    bool supportsStringConversion{false};

    /*
     * To String and From String conversion functions require information about the
     * parameter to execute. The primary driver is the value so the API takes the form
     * `valueToString(float)` but for optional features like extension, deform,
     * absolute and temposync we need to knwo that. Since this metadata does not
     * store any values but has the values handed externally, we could either have
     * a long-argument-list api or a little helper class for those values. We chose
     * the later in FeatureState
     */
    struct FeatureState
    {
        bool isHighPrecision{false}, isExtended{false}, isAbsolute{false}, isTemposynced{false};

        FeatureState(){};

        FeatureState &withHighPrecision(bool e)
        {
            isHighPrecision = e;
            return *this;
        }
        FeatureState &withExtended(bool e)
        {
            isExtended = e;
            return *this;
        }
        FeatureState &withAbsolute(bool e)
        {
            isAbsolute = e;
            return *this;
        }
        FeatureState &withTemposync(bool e)
        {
            isTemposynced = e;
            return *this;
        }
    };

    /*
     * What is the primary string representation of this value
     */
    std::optional<std::string> valueToString(float val, const FeatureState &fs = {}) const;

    /*
     * Some parameters have a secondary representation. For instance 441.2hz could also be ~A4.
     * If this parameter supports that it will return a string value from this API. Surge uses
     * this in the left side of the tooltip.
     */
    std::optional<std::string> valueToAlternateString(float val) const;

    /*
     * Convert a value to a string; if the optional is empty populate the error message.
     */
    std::optional<float> valueFromString(std::string_view, std::string &errMsg) const;

    /*
     * Distances to String conversions are more peculiar, especially with non-linear ranges.
     * The parameter metadata assumes all distances are represented in a [-1,1] value on the
     * range of the parameter, and then can create four strings for a given distance:
     * - value up/down (to string of val +/- modulation)
     * - distance up (the string of the distance of appying the modulation up down)
     * To calculate these, modulation needs to be expressed as a natural base value
     * and a percentage modulation depth.
     */
    struct ModulationDisplay
    {
        // value is with-units value suitable to seed a typein. Like "4.3 semitones"
        std::string value;
        // Summary is a brief description suitable for a menu like "+/- 13.2%"
        std::string summary;

        // baseValue, valUp/Dn and changeUp/Dn are no unit indications of change in each direction.
        std::string baseValue, valUp, valDown, changeUp, changeDown;
    };
    std::optional<ModulationDisplay> modulationNaturalToString(float naturalBaseVal,
                                                               float modulationNatural,
                                                               bool isBipolar,
                                                               const FeatureState &fs = {}) const;
    std::optional<float> modulationNaturalFromString(std::string_view deltaNatural,
                                                     float naturalBaseVal,
                                                     std::string &errMsg) const;

    enum DisplayScale
    {
        LINEAR,         // out = A * r + B
        A_TWO_TO_THE_B, // out = A 2^(B r + C)
        DECIBEL,        // TODO - implement
        UNORDERED_MAP,  // out = discreteValues[(int)std::round(val)]
        USER_PROVIDED   // TODO - implement
    } displayScale{LINEAR};

    std::string unit;
    std::string customMinDisplay;
    std::string customMaxDisplay;
    std::string customDefaultDisplay;

    std::unordered_map<int, std::string> discreteValues;
    int decimalPlaces{2};
    float svA{0.f}, svB{0.f}, svC{0.f}, svD{0.f}; // for various functional forms
    float exA{1.f}, exB{0.f};

    float naturalToNormalized01(float naturalValue) const
    {
        float v = 0;
        switch (type)
        {
        case FLOAT:
            assert(maxVal != minVal);
            v = (naturalValue - minVal) / (maxVal - minVal);
            break;
        case INT:
            assert(maxVal != minVal);
            // This is the surge conversion. Do we want to keep it?
            v = 0.005 + 0.99 * (naturalValue - minVal) / (maxVal - minVal);
            break;
        case BOOL:
            assert(maxVal == 1 && minVal == 0);
            v = (naturalValue > 0.5 ? 1.f : 0.f);
            break;
        case NONE:
            assert(false);
            v = 0.f;
            break;
        }
        return std::clamp(v, 0.f, 1.f);
    }
    float normalized01ToNatural(float normalizedValue) const
    {
        assert(normalizedValue >= 0.f && normalizedValue <= 1.f);
        assert(maxVal != minVal);
        normalizedValue = std::clamp(normalizedValue, 0.f, 1.f);
        switch (type)
        {
        case FLOAT:
            return normalizedValue * (maxVal - minVal) + minVal;
        case INT:
        {
            // again the surge conversion
            return (int)((1 / 0.99) * (normalizedValue - 0.005) * (maxVal - minVal) + 0.5) + minVal;
        }
        case BOOL:
            assert(maxVal == 1 && minVal == 0);
            return normalizedValue > 0.5 ? maxVal : minVal;
        case NONE:
            assert(false);
            return 0.f;
        }
        // quiet gcc
        return 0.f;
    }

    ParamMetaData &withType(Type t)
    {
        type = t;
        return *this;
    }
    ParamMetaData &withName(const std::string t)
    {
        name = t;
        return *this;
    }
    ParamMetaData &withRange(float mn, float mx)
    {
        minVal = mn;
        maxVal = mx;
        defaultVal = std::clamp(defaultVal, minVal, maxVal);
        return *this;
    }
    ParamMetaData &withDefault(float t)
    {
        defaultVal = t;
        return *this;
    }
    ParamMetaData &extendable(bool b = true)
    {
        canExtend = b;
        return *this;
    }
    // extend is f -> A f + B
    ParamMetaData &withExtendFactors(float A, float B)
    {
        exA = A;
        exB = B;
        return *this;
    }
    ParamMetaData &deformable(bool b = true)
    {
        canDeform = b;
        return *this;
    }
    ParamMetaData &withDeformationCount(int c)
    {
        deformationCount = c;
        return *this;
    }
    ParamMetaData &absolutable(bool b = true)
    {
        canAbsolute = b;
        return *this;
    }
    ParamMetaData &temposyncable(bool b = true)
    {
        canTemposync = b;
        return *this;
    }
    ParamMetaData &deactivatable(bool b = true)
    {
        canDeactivate = b;
        return *this;
    }

    ParamMetaData &withATwoToTheBFormatting(float A, float B, std::string_view units)
    {
        return withATwoToTheBPlusCFormatting(A, B, 0.f, units);
    }

    ParamMetaData &withATwoToTheBPlusCFormatting(float A, float B, float C, std::string_view units)
    {
        svA = A;
        svB = B;
        svC = C;
        unit = units;
        displayScale = A_TWO_TO_THE_B;
        supportsStringConversion = true;
        return *this;
    }

    ParamMetaData &withSemitoneZeroAt400Formatting()
    {
        return withATwoToTheBFormatting(440, 1.0 / 12.0, "Hz");
    }
    ParamMetaData &withLog2SecondsFormatting() { return withATwoToTheBFormatting(1, 1, "s"); }

    ParamMetaData &withLinearScaleFormatting(std::string units, float scale = 1.f)
    {
        svA = scale;
        unit = units;
        displayScale = LINEAR;
        supportsStringConversion = true;
        return *this;
    }

    ParamMetaData &withUnorderedMapFormatting(const std::unordered_map<int, std::string> &map)
    {
        discreteValues = map;
        displayScale = UNORDERED_MAP;
        supportsStringConversion = true;
        return *this;
    }

    ParamMetaData &withDecimalPlaces(int d)
    {
        decimalPlaces = d;
        return *this;
    }

    ParamMetaData &withCustomMaxDisplay(const std::string &v)
    {
        customMaxDisplay = v;
        return *this;
    }

    ParamMetaData &withCustomMinDisplay(const std::string &v)
    {
        customMinDisplay = v;
        return *this;
    }
    ParamMetaData &withCustomDefaultDisplay(const std::string &v)
    {
        customDefaultDisplay = v;
        return *this;
    }

    ParamMetaData &asPercent()
    {
        return withRange(0.f, 1.f)
            .withDefault(0.f)
            .withType(FLOAT)
            .withLinearScaleFormatting("%", 100.f)
            .withDecimalPlaces(2);
    }

    ParamMetaData &asPercentExtendableToBipolar()
    {
        return asPercent().extendable().withExtendFactors(2.f, -1.f);
    }

    ParamMetaData &asPercentBipolar()
    {
        return withRange(-1.f, 1.f)
            .withDefault(0.f)
            .withType(FLOAT)
            .withLinearScaleFormatting("%", 100.f)
            .withDecimalPlaces(2);
    }
    ParamMetaData &asDecibelNarrow()
    {
        return withRange(-24.f, 24.f)
            .withDefault(0.f)
            .withType(FLOAT)
            .withLinearScaleFormatting("dB");
    }
    ParamMetaData &asDecibel()
    {
        return withRange(-48.f, 48.f)
            .withDefault(0.f)
            .withType(FLOAT)
            .withLinearScaleFormatting("dB");
    }
    ParamMetaData &asMIDIPitch()
    {
        return withType(FLOAT)
            .withRange(0.f, 127.f)
            .withDefault(60.f)
            .withLinearScaleFormatting("semitones");
    }
    ParamMetaData &asMIDINote()
    {
        return withType(INT)
            .withRange(0, 127)
            .withDefault(60)
            .withLinearScaleFormatting("semitones")
            .withDecimalPlaces(0);
    }
    ParamMetaData &asLfoRate()
    {
        return withType(FLOAT).withRange(-7, 9).temposyncable().withATwoToTheBFormatting(1, 1,
                                                                                         "Hz");
    }
    ParamMetaData &asEnvelopeTime()
    {
        return withType(FLOAT)
            .withRange(-8.f, 5.f)
            .withDefault(-1.f)
            .temposyncable()
            .withATwoToTheBFormatting(1, 1, "s");
    }
    ParamMetaData &asAudibleFrequency()
    {
        return withType(FLOAT).withRange(-60, 70).withDefault(0).withSemitoneZeroAt400Formatting();
    }

    std::string temposyncNotation(float f) const;
};

/*
 * Implementation below here
 */
inline std::optional<std::string> ParamMetaData::valueToString(float val,
                                                               const FeatureState &fs) const
{
    if (type == BOOL)
    {
        if (val < 0)
            return customMinDisplay.empty() ? "Off" : customMinDisplay;
        return customMaxDisplay.empty() ? "On" : customMaxDisplay;
    }

    if (type == INT)
    {
        auto iv = (int)std::round(val);
        if (displayScale == UNORDERED_MAP)
        {
            if (discreteValues.find(iv) != discreteValues.end())
                return discreteValues.at(iv);
            return std::nullopt;
        }
        return std::nullopt;
    }

    if (!customMinDisplay.empty() && val == minVal)
        return customMinDisplay;
    if (!customMaxDisplay.empty() && val == maxVal)
        return customMaxDisplay;
    if (!customDefaultDisplay.empty() && val == defaultVal)
        return customDefaultDisplay;

    if (fs.isExtended)
        val = exA * val + exB;

    if (fs.isTemposynced)
    {
        return temposyncNotation(val);
    }

    // float cases
    switch (displayScale)
    {
    case LINEAR:
        if (val == minVal && !customMinDisplay.empty())
        {
            return customMinDisplay;
        }
        if (val == maxVal && !customMaxDisplay.empty())
        {
            return customMinDisplay;
        }

        return fmt::format("{:.{}f} {:s}", svA * val,
                           (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces), unit);
        break;
    case A_TWO_TO_THE_B:
        if (val == minVal && !customMinDisplay.empty())
        {
            return customMinDisplay;
        }
        if (val == maxVal && !customMaxDisplay.empty())
        {
            return customMinDisplay;
        }

        return fmt::format("{:.{}f} {:s}", svA * pow(2.0, svB * val + svC),
                           (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces), unit);
        break;
    default:
        break;
    }
    return std::nullopt;
}

inline std::optional<float> ParamMetaData::valueFromString(std::string_view v,
                                                           std::string &errMsg) const
{
    if (type == BOOL)
        return std::nullopt;
    if (type == INT)
        return std::nullopt;

    if (!customMinDisplay.empty() && v == customMinDisplay)
        return minVal;

    if (!customMaxDisplay.empty() && v == customMaxDisplay)
        return maxVal;

    auto rangeMsg = [this]() {
        std::string em;
        auto nv = valueToString(minVal);
        auto xv = valueToString(maxVal);
        if (nv.has_value() && xv.has_value())
            em = fmt::format("{} < val < {}", *nv, *xv);
        else
            em = fmt::format("Invalid input");
        return em;
    };
    switch (displayScale)
    {
    case LINEAR:
    {
        try
        {
            auto r = std::stof(std::string(v));
            assert(svA != 0);
            r = r / svA;
            if (r < minVal || r > maxVal)
            {
                errMsg = rangeMsg();
                return std::nullopt;
            }

            return r;
        }
        catch (const std::exception &)
        {
            errMsg = rangeMsg();
            return std::nullopt;
        }
    }
    break;
    case A_TWO_TO_THE_B:
    {
        try
        {
            auto r = std::stof(std::string(v));
            assert(svA != 0);
            assert(svB != 0);
            if (r < 0)
            {
                errMsg = rangeMsg();
                return std::nullopt;
            }
            /* v = svA 2^(svB r + svC)
             * log2(v / svA) = svB r + svC
             * (log2(v/svA) - svC)/svB = r
             */
            r = (log2(r / svA) - svC) / svB;
            if (r < minVal || r > maxVal)
            {
                errMsg = rangeMsg();
                return std::nullopt;
            }

            return r;
        }
        catch (const std::exception &)
        {
            errMsg = rangeMsg();
            return std::nullopt;
        }
    }
    break;
    default:
        break;
    }
    return std::nullopt;
}

inline std::optional<std::string> ParamMetaData::valueToAlternateString(float val) const
{
    return std::nullopt;
}

inline std::optional<ParamMetaData::ModulationDisplay>
ParamMetaData::modulationNaturalToString(float naturalBaseVal, float modulationNatural,
                                         bool isBipolar, const FeatureState &fs) const
{
    if (type != FLOAT)
        return std::nullopt;
    ModulationDisplay result;

    switch (displayScale)
    {
    case LINEAR:
    {
        assert(abs(modulationNatural) <= maxVal - minVal);
        // OK this is super easy. It's just linear!
        auto du = modulationNatural;
        auto dd = -modulationNatural;

        auto dp = (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces);
        result.value = fmt::format("{:.{}f} {}", svA * du, dp, unit);
        if (isBipolar)
        {
            if (du > 0)
            {
                result.summary = fmt::format("+/- {:.{}f} {}", svA * du, dp, unit);
            }
            else
            {
                result.summary = fmt::format("-/+ {:.{}f} {}", -svA * du, dp, unit);
            }
        }
        else
        {
            result.summary = fmt::format("{:.{}f} {}", svA * du, dp, unit);
        }
        result.changeUp = fmt::format("{:.{}f}", svA * du, dp);
        if (isBipolar)
            result.changeDown = fmt::format("{:.{}f}", svA * dd, dp);
        result.valUp = fmt::format("{:.{}f}", svA * (naturalBaseVal + du), dp);

        if (isBipolar)
            result.valDown = fmt::format("{:.{}f}", svA * (naturalBaseVal + du), dp);
        // TODO pass this on not create
        auto v2s = valueToString(naturalBaseVal, fs);
        if (v2s.has_value())
            result.baseValue = *v2s;
        else
            result.baseValue = "-ERROR-";
        return result;
    }
    case A_TWO_TO_THE_B:
    {
        auto nvu = naturalBaseVal + modulationNatural;
        auto nvd = naturalBaseVal - modulationNatural;

        auto scv = svA * pow(2, svB * naturalBaseVal);
        auto svu = svA * pow(2, svB * nvu);
        auto svd = svA * pow(2, svB * nvd);
        auto du = svu - scv;
        auto dd = scv - svd;

        auto dp = (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces);
        result.value = fmt::format("{:.{}f} {}", du, dp, unit);
        if (isBipolar)
        {
            if (du > 0)
            {
                result.summary = fmt::format("+/- {:.{}f} {}", du, dp, unit);
            }
            else
            {
                result.summary = fmt::format("-/+ {:.{}f} {}", -du, dp, unit);
            }
        }
        else
        {
            result.summary = fmt::format("{:.{}f} {}", du, dp, unit);
        }
        result.changeUp = fmt::format("{:.{}f}", du, dp);
        if (isBipolar)
            result.changeDown = fmt::format("{:.{}f}", dd, dp);
        result.valUp = fmt::format("{:.{}f}", nvu, dp);

        if (isBipolar)
            result.valDown = fmt::format("{:.{}f}", nvd, dp);
        auto v2s = valueToString(naturalBaseVal, fs);
        if (v2s.has_value())
            result.baseValue = *v2s;
        else
            result.baseValue = "-ERROR-";
        return result;
    }
    default:
        break;
    }

    return std::nullopt;
}

inline std::optional<float>
ParamMetaData::modulationNaturalFromString(std::string_view deltaNatural, float naturalBaseVal,
                                           std::string &errMsg) const
{
    switch (displayScale)
    {
    case LINEAR:
    {
        try
        {
            auto mv = std::stof(std::string(deltaNatural)) / svA;
            if (abs(mv) > (maxVal - minVal))
            {
                errMsg = fmt::format("Maximum depth: {} {}", (maxVal - minVal) * svA, unit);
                return std::nullopt;
            }
            return mv;
        }
        catch (const std::exception &e)
        {
            return std::nullopt;
        }
    }
    break;
    case A_TWO_TO_THE_B:
    {
        try
        {
            auto xbv = svA * pow(2, svB * naturalBaseVal);
            auto mv = std::stof(std::string(deltaNatural));
            auto rv = xbv + mv;
            if (rv < 0)
            {
                return std::nullopt;
            }

            auto r = log2(rv / svA) / svB;
            auto rg = maxVal - minVal;
            if (r < -rg || r > rg)
            {
                return std::nullopt;
            }

            return r - naturalBaseVal;
        }
        catch (const std::exception &e)
        {
            return std::nullopt;
        }
    }
    break;
    default:
        break;
    }
    return std::nullopt;
}

inline std::string ParamMetaData::temposyncNotation(float f) const
{
    float a, b = modff(f, &a);

    if (b >= 0)
    {
        b -= 1.0;
        a += 1.0;
    }

    float d, q;
    std::string nn, t;
    char tmp[1024];

    if (f >= 1)
    {
        q = pow(2.0, f - 1);
        nn = "whole";
        if (q >= 3)
        {
            if (abs(q - floor(q + 0.01)) < 0.01)
            {
                snprintf(tmp, 1024, "%d whole notes", (int)floor(q + 0.01));
            }
            else
            {
                // this is the triplet case
                snprintf(tmp, 1024, "%d whole triplets", (int)floor(q * 3.0 / 2.0 + 0.02));
            }
            std::string res = tmp;
            return res;
        }
        else if (q >= 2)
        {
            nn = "double whole";
            q /= 2;
        }

        if (q < 1.3)
        {
            t = "note";
        }
        else if (q < 1.4)
        {
            t = "triplet";
            if (nn == "whole")
            {
                nn = "double whole";
            }
            else
            {
                q = pow(2.0, f - 1);
                snprintf(tmp, 1024, "%d whole triplets", (int)floor(q * 3.0 / 2.0 + 0.02));
                std::string res = tmp;
                return res;
            }
        }
        else
        {
            t = "dotted";
        }
    }
    else
    {
        d = pow(2.0, -(a - 2));
        q = pow(2.0, (b + 1));

        if (q < 1.3)
        {
            t = "note";
        }
        else if (q < 1.4)
        {
            t = "triplet";
            d = d / 2;
        }
        else
        {
            t = "dotted";
        }
        if (d == 1)
        {
            nn = "whole";
        }
        else
        {
            char tmp[1024];
            snprintf(tmp, 1024, "1/%d", (int)d);
            nn = tmp;
        }
    }
    std::string res = nn + " " + t;

    return res;
}

} // namespace sst::basic_blocks::params

#endif
