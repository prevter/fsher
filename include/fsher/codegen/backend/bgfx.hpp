#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "../ir.hpp"

namespace fsher::backend::bgfx {
    enum class UniformType : uint8_t {
        Sampler = 0,
        End     = 1,
        Vec4    = 2,
        Mat3    = 3,
        Mat4    = 4,
    };

    enum class TextureDimension : uint8_t {
        None    = 0,
        D2      = 1,
        D3      = 2,
        Cube    = 3,
        Array2D = 4,
    };

    enum class AttributeKind : uint16_t {
        Position   = 0x01,
        Normal     = 0x02,
        Tangent    = 0x03,
        Bitangent  = 0x04,
        Color0     = 0x05,
        Color1     = 0x06,
        Color2     = 0x18,
        Color3     = 0x19,
        Indices    = 0x0e,
        Weight     = 0x0f,
        TexCoord0  = 0x10,
        TexCoord1  = 0x11,
        TexCoord2  = 0x12,
        TexCoord3  = 0x13,
        TexCoord4  = 0x14,
        TexCoord5  = 0x15,
        TexCoord6  = 0x16,
        TexCoord7  = 0x17,
        TexCoord8  = 0x1a,
        TexCoord9  = 0x1b,
        TexCoord10 = 0x1c,
        TexCoord11 = 0x1d,
        TexCoord12 = 0x1e,
        TexCoord13 = 0x1f,
        TexCoord14 = 0x20,
        TexCoord15 = 0x21,
    };

    constexpr auto ATTRIBUTE_NAMES = std::to_array<std::pair<std::string_view, AttributeKind>>({
        {"a_position", AttributeKind::Position},
        {"a_normal", AttributeKind::Normal},
        {"a_tangent", AttributeKind::Tangent},
        {"a_bitangent", AttributeKind::Bitangent},
        {"a_color0", AttributeKind::Color0},
        {"a_color1", AttributeKind::Color1},
        {"a_color2", AttributeKind::Color2},
        {"a_color3", AttributeKind::Color3},
        {"a_indices", AttributeKind::Indices},
        {"a_weight", AttributeKind::Weight},
        {"a_texcoord0", AttributeKind::TexCoord0},
        {"a_texcoord1", AttributeKind::TexCoord1},
        {"a_texcoord2", AttributeKind::TexCoord2},
        {"a_texcoord3", AttributeKind::TexCoord3},
        {"a_texcoord4", AttributeKind::TexCoord4},
        {"a_texcoord5", AttributeKind::TexCoord5},
        {"a_texcoord6", AttributeKind::TexCoord6},
        {"a_texcoord7", AttributeKind::TexCoord7},
        {"a_texcoord8", AttributeKind::TexCoord8},
        {"a_texcoord9", AttributeKind::TexCoord9},
        {"a_texcoord10", AttributeKind::TexCoord10},
        {"a_texcoord11", AttributeKind::TexCoord11},
        {"a_texcoord12", AttributeKind::TexCoord12},
        {"a_texcoord13", AttributeKind::TexCoord13},
        {"a_texcoord14", AttributeKind::TexCoord14},
        {"a_texcoord15", AttributeKind::TexCoord15},
    });

    struct Uniform {
        std::string name;
        UniformType type = UniformType::End;
        uint8_t num = 1;
        uint8_t texComponent = 0;
        TextureDimension texDimension = TextureDimension::None;
        uint16_t texFormat = 0;
        uint16_t byteSize = 0;
    };

    inline std::optional<Uniform> fromIR(ir::GlobalVar const& v, bool isSpirv) {
        auto const& base = v.type.elementBase();

        uint8_t num = 1;
        if (v.type.kind == Type::Kind::Array) {
            if (v.type.arrayLength == 0 || v.type.arrayLength > 255) {
                return std::nullopt;
            }
            num = static_cast<uint8_t>(v.type.arrayLength);
        }

        Uniform u;
        u.name = v.name;

        switch (base.kind) {
            case Type::Kind::Vector:
                if (base.scalarKind != ScalarKind::Float32 || base.rows != 4) {
                    return std::nullopt;
                }
                u.type = UniformType::Vec4;
                break;
            case Type::Kind::Matrix:
                if (base.scalarKind != ScalarKind::Float32) {
                    return std::nullopt;
                }
                if (base.cols == 3 && base.rows == 3) {
                    u.type = UniformType::Mat3;
                } else if (base.cols == 4 && base.rows == 4) {
                    u.type = UniformType::Mat4;
                } else {
                    return std::nullopt;
                }
                break;
            case Type::Kind::Sampler2D:
                u.type = UniformType::Sampler;
                if (isSpirv) {
                    num = 0;
                    u.texComponent = 0;
                    u.texDimension = TextureDimension::D3;
                    u.texFormat = 38;
                } else {
                    u.texComponent = 4;
                    u.texDimension = TextureDimension::D2;
                    u.texFormat = 0;
                }
                break;
            default: return std::nullopt;
        }

        u.num = num;
        u.byteSize = v.type.sizeOf();
        return u;
    }

    inline uint16_t regSlotsFor(UniformType type, uint8_t num) noexcept {
        switch (type) {
            case UniformType::Mat3: return static_cast<uint16_t>(num * 3);
            case UniformType::Mat4: return static_cast<uint16_t>(num * 4);
            case UniformType::Vec4: return num;
            case UniformType::Sampler: return num;
            case UniformType::End: return 0;
        }
        return 0;
    }

    inline std::string_view magicFor(Stage stage) noexcept {
        switch (stage) {
            case Stage::Vertex: return "VSH";
            case Stage::Fragment: return "FSH";
            default: return "VSH";
        }
    }

    namespace detail {
        inline void pushU8(std::vector<uint8_t>& out, uint8_t v) {
            out.push_back(v);
        }
        inline void pushU16(std::vector<uint8_t>& out, uint16_t v) {
            out.push_back(static_cast<uint8_t>(v & 0xFF));
            out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        }
        inline void pushU32(std::vector<uint8_t>& out, uint32_t v) {
            out.push_back(static_cast<uint8_t>(v & 0xFF));
            out.push_back(static_cast<uint8_t>((v >> 8)  & 0xFF));
            out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
            out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        }
        inline void pushBytes(std::vector<uint8_t>& out, std::span<uint8_t const> bytes) {
            out.insert(out.end(), bytes.begin(), bytes.end());
        }
    }

    inline std::vector<uint8_t> encodeStage(
        ir::StageModule const& mod,
        std::span<uint8_t const> sourceBytes,
        bool isSpirv,
        util::StringMap<uint32_t> const& bindingMap = {}
    ) {
        std::vector<Uniform> uniforms;
        uniforms.reserve(mod.uniforms.size());
        for (auto const& u : mod.uniforms) {
            if (auto opt = fromIR(u, isSpirv)) {
                uniforms.push_back(std::move(*opt));
            }
        }

        std::vector<uint8_t> out;
        out.reserve(20 + uniforms.size() * 40 + sourceBytes.size() + 8);

        // magic / version
        detail::pushBytes(out, std::span(reinterpret_cast<uint8_t const*>(magicFor(mod.stage).data()), 3));
        detail::pushU8(out, 11);

        // inputHash / outputHash
        detail::pushU32(out, 0);
        detail::pushU32(out, 0);

        // uniform count
        uint16_t uniformCount = static_cast<uint16_t>(uniforms.size());
        detail::pushU16(out, uniformCount);

        uint16_t regIndex = 0;
        uint16_t offset = 0;
        for (auto const& u : uniforms) {
            size_t nameLen = u.name.size();
            if (nameLen > 255) nameLen = 255;

            detail::pushU8(out, nameLen);
            detail::pushBytes(out, std::span(reinterpret_cast<uint8_t const*>(u.name.data()), nameLen));

            uint8_t t = static_cast<uint8_t>(u.type);
            if (isSpirv && mod.stage == Stage::Fragment) {
                t |= 0x10; // fragment shader bit
                if (u.type == UniformType::Sampler) {
                    t |= 0x20; // sampler bit
                }
            }

            detail::pushU8(out, t);
            detail::pushU8(out, u.num);

            if (isSpirv) {
                if (u.type == UniformType::Sampler) {
                    auto it = bindingMap.find(u.name);
                    uint16_t binding = (it != bindingMap.end())
                        ? static_cast<uint16_t>(it->second)
                        : 2;
                    detail::pushU16(out, binding);
                } else {
                    detail::pushU16(out, offset);
                }
            } else {
                detail::pushU16(out, regIndex);
            }

            uint16_t regCount = regSlotsFor(u.type, u.num);
            detail::pushU16(out, isSpirv && u.type == UniformType::Sampler ? 0 : regCount);

            regIndex = static_cast<uint16_t>(regIndex + regCount);
            offset += u.byteSize;

            detail::pushU8(out, u.texComponent);
            detail::pushU8(out, static_cast<uint8_t>(u.texDimension));
            detail::pushU16(out, u.texFormat);
        }

        detail::pushU32(out, sourceBytes.size());
        detail::pushBytes(out, sourceBytes);
        detail::pushU8(out, 0);

        if (isSpirv) {
            if (mod.stage == Stage::Vertex) {
                uint8_t attrCount = static_cast<uint8_t>(mod.inputs.size());
                detail::pushU8(out, attrCount);
                for (auto const& v : mod.inputs) {
                    for (size_t i = 0; i < ATTRIBUTE_NAMES.size(); ++i) {
                        if (v.name == ATTRIBUTE_NAMES[i].first) {
                            detail::pushU16(out, static_cast<uint16_t>(ATTRIBUTE_NAMES[i].second));
                            break;
                        }
                    }
                }
            } else {
                detail::pushU8(out, 0);
            }

            // calculate the size of uniforms
            uint16_t uniformSize = 0;
            for (auto const& u : mod.uniforms) {
                uniformSize += u.type.sizeOf();
            }

            detail::pushU16(out, uniformSize);
        }

        return out;
    }
}
