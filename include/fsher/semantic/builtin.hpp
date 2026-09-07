#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"
#include "../utils.hpp"
#include "../codegen/backend/backend_id.hpp"

namespace fsher {
    enum class BuiltinID : uint16_t {
        Abs,
        Sign,
        Floor,
        Ceil,
        Fract,
        Trunc,
        Round,
        Min,
        Max,
        Clamp,
        Mix,
        Step,
        Smoothstep,
        Mod,
        Sin,
        Cos,
        Tan,
        Asin,
        Acos,
        Atan,
        Atan2,
        Pow,
        Sqrt,
        Exp,
        Exp2,
        Log,
        Log2,
        InverseSqrt,
        Dot,
        Cross,
        Normalize,
        Length,
        Distance,
        Reflect,
        Refract,
        Texture,
        DFdx,
        DFdy,
        Fwidth,
        COUNT
    };

    enum class ParamKind : uint8_t {
        AnyNumeric,
        FloatScalar,
        FloatVec,
        FloatOrFloatVec,
        FloatMat,
        IntScalar,
        IntVec,
        BoolScalar,
        Sampler,
        SameAsParam,
        Any
    };

    struct BuiltinParam {
        ParamKind kind;
        uint8_t refIndex = 0;
    };

    using ReturnTypeFn = Type(*)(std::span<Type const> args);

    struct BuiltinOverload {
        std::vector<BuiltinParam> params;
        ReturnTypeFn inferReturnType;
    };

    struct BuiltinDef {
        std::string_view canonicalName;
        BuiltinID id;
        std::vector<BuiltinOverload> overloads;
    };

    struct BuiltinMatch {
        BuiltinID id;
        size_t overloadIndex;
        Type returnType;
    };

    class BuiltinRegistry {
    public:
        static BuiltinRegistry& get() noexcept;

        std::optional<BuiltinMatch> resolve(std::string_view name, std::span<Type const> args) const noexcept;
        BuiltinDef const* lookup(std::string_view name) const noexcept;

        std::string_view backendName(BuiltinID id, CodeBackend backend) const noexcept;
        std::string_view canonicalName(BuiltinID id) const noexcept;

        util::StringMap<BuiltinDef> const& builtins() const noexcept { return m_builtins; }

        BuiltinRegistry(BuiltinRegistry const&) = delete;
        BuiltinRegistry& operator=(BuiltinRegistry const&) = delete;

    private:
        BuiltinRegistry() noexcept;

        void add(BuiltinDef def) {
            m_builtins.emplace(std::string(def.canonicalName), std::move(def));
        }

        void addBackendName(BuiltinID id, CodeBackend backend, std::string name) {
            m_backendNames[static_cast<uint16_t>(id)][static_cast<uint16_t>(backend)] = std::move(name);
        }

        static bool matchParam(BuiltinParam const& param, Type const& type, std::span<Type const> args) noexcept;
        static bool matchOverload(BuiltinOverload const& overload, std::span<Type const> args) noexcept;

        static bool isFloatLike(ScalarKind kind) noexcept {
            return kind == ScalarKind::Float32 || kind == ScalarKind::Float64 || kind == ScalarKind::UntypedFloat;
        }

        static bool isIntLike(ScalarKind kind) noexcept {
            return kind == ScalarKind::Int32 || kind == ScalarKind::UInt32 || kind == ScalarKind::UntypedInt;
        }

        static bool isNumeric(Type const& t) noexcept {
            switch (t.kind) {
                case Type::Kind::Scalar:
                    return isFloatLike(t.scalarKind) || isIntLike(t.scalarKind);
                case Type::Kind::Vector:
                case Type::Kind::Matrix:
                    return t.scalarKind == ScalarKind::Float32
                        || t.scalarKind == ScalarKind::Float64
                        || t.scalarKind == ScalarKind::Int32
                        || t.scalarKind == ScalarKind::UInt32;
                default:
                    return false;
            }
        }

        static bool isFloat(ScalarKind k) noexcept {
            return k == ScalarKind::Float32 || k == ScalarKind::Float64;
        }

        static bool isInt(ScalarKind k) noexcept {
            return k == ScalarKind::Int32 || k == ScalarKind::UInt32;
        }

        static bool isFloatType(Type const& t) noexcept {
            if (t.kind == Type::Kind::Scalar) return isFloatLike(t.scalarKind);
            if (t.kind == Type::Kind::Vector || t.kind == Type::Kind::Matrix) return isFloat(t.scalarKind);
            return false;
        }

        static bool isFloatVec(Type const& t) noexcept {
            return t.kind == Type::Kind::Vector && isFloat(t.scalarKind);
        }

        static bool isFloatMat(Type const& t) noexcept {
            return t.kind == Type::Kind::Matrix && isFloat(t.scalarKind);
        }

        static bool isIntScalar(Type const& t) noexcept {
            return t.kind == Type::Kind::Scalar && isIntLike(t.scalarKind);
        }

        static bool isIntVec(Type const& t) noexcept {
            return t.kind == Type::Kind::Vector && isInt(t.scalarKind);
        }

        static bool isFloatScalar(Type const& t) noexcept {
            return t.kind == Type::Kind::Scalar && isFloatLike(t.scalarKind);
        }

        static bool isFloatOrFloatVec(Type const& t) noexcept {
            return isFloatScalar(t) || isFloatVec(t);
        }

    private:
        util::StringMap<BuiltinDef> m_builtins;
        std::unordered_map<uint16_t, std::unordered_map<uint16_t, std::string>> m_backendNames;
    };
}
