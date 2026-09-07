#pragma once

#include <cstdint>

namespace fsher {
    enum class CodeBackend : uint8_t {
        GLSL  = 0,
        HLSL  = 1,
        MSL   = 2,
        SPIRV = 3,
        WGSL  = 4
    };
}