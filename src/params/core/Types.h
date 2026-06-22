#pragma once

#include <cstdint>
#include <string>

namespace MugLab::OfxBase {

enum class ParamValueType : std::uint8_t {
    Double,
    Double2D,
    RGBA,
    Boolean,
    Choice
};

struct ParamIdTypePair {
    std::string id_;
    ParamValueType type_;
};

struct ParamPoint2D {
    double x_ = 0.0;
    double y_ = 0.0;
};

struct ParamRGBA {
    float r_ = 0.0F;
    float g_ = 0.0F;
    float b_ = 0.0F;
    float a_ = 1.0F;
};

}  // namespace MugLab::OfxBase
