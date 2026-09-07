#include <fsher/semantic/builtin.hpp>

namespace fsher {
    BuiltinRegistry& BuiltinRegistry::get() noexcept {
        static BuiltinRegistry instance;
        return instance;
    }

    std::optional<BuiltinMatch> BuiltinRegistry::resolve(
        std::string_view name, std::span<Type const> args
    ) const noexcept {
        auto it = m_builtins.find(name);
        if (it == m_builtins.end()) {
            return std::nullopt;
        }

        auto const& def = it->second;
        for (size_t i = 0; i < def.overloads.size(); ++i) {
            auto const& overload = def.overloads[i];
            if (overload.params.size() != args.size()) {
                continue;
            }

            if (this->matchOverload(overload, args)) {
                return BuiltinMatch{
                    .id = def.id,
                    .overloadIndex = i,
                    .returnType = overload.inferReturnType(args),
                };
            }
        }

        return std::nullopt;
    }

    BuiltinDef const* BuiltinRegistry::lookup(std::string_view name) const noexcept {
        auto it = m_builtins.find(name);
        if (it == m_builtins.end()) {
            return nullptr;
        }
        return &it->second;
    }

    std::string_view BuiltinRegistry::backendName(BuiltinID id, CodeBackend backend) const noexcept {
        auto it = m_backendNames.find(static_cast<uint16_t>(id));
        if (it == m_backendNames.end()) {
            for (auto const& [name, def] : m_builtins) {
                if (def.id == id) {
                    return name;
                }
            }
            return "<unknown>";
        }

        auto const& perBackend = it->second;
        auto bk = static_cast<uint16_t>(backend);
        auto it2 = perBackend.find(bk);
        if (it2 != perBackend.end()) {
            return it2->second;
        }

        return "<unknown>";
    }

    std::string_view BuiltinRegistry::canonicalName(BuiltinID id) const noexcept {
        for (auto const& [name, def] : m_builtins) {
            if (def.id == id) {
                return name;
            }
        }
        return "<unknown>";
    }

    bool BuiltinRegistry::matchParam(
        BuiltinParam const& param, Type const& type, std::span<Type const> args
    ) noexcept {
        switch (param.kind) {
            case ParamKind::AnyNumeric: return isNumeric(type);
            case ParamKind::FloatScalar: return isFloatScalar(type);
            case ParamKind::FloatVec: return isFloatVec(type);
            case ParamKind::FloatOrFloatVec: return isFloatOrFloatVec(type);
            case ParamKind::FloatMat: return isFloatMat(type);
            case ParamKind::IntScalar: return isIntScalar(type);
            case ParamKind::IntVec: return isIntVec(type);
            case ParamKind::BoolScalar: return type.kind == Type::Kind::Scalar && type.scalarKind == ScalarKind::Bool;
            case ParamKind::Sampler: return type.kind == Type::Kind::Sampler2D;
            case ParamKind::SameAsParam: return type == args[param.refIndex];
            case ParamKind::Any: return true;
        }
        return false;
    }

    bool BuiltinRegistry::matchOverload(BuiltinOverload const& overload, std::span<Type const> args) noexcept {
        if (overload.params.size() != args.size()) return false;
        for (size_t i = 0; i < overload.params.size(); ++i) {
            if (!matchParam(overload.params[i], args[i], args)) {
                return false;
            }
        }
        return true;
    }

    namespace builtins {
        static Type sameAsFirst(std::span<Type const> args) noexcept { return args[0]; }
        static Type scalarOfFirst(std::span<Type const> args) noexcept {
            return Type{.kind = Type::Kind::Scalar, .scalarKind = args[0].scalarKind};
        }
        static Type sameScalarAsFirst(std::span<Type const> args) noexcept {
            return Type{ .kind = Type::Kind::Scalar, .scalarKind = args[0].scalarKind };
        }
        static Type crossResult(std::span<Type const>) noexcept {
            return Type::Vector(ScalarKind::Float32, 3);
        }
        static Type textureResult(std::span<Type const>) noexcept {
            return Type::Vector(ScalarKind::Float32, 4);
        }
    }

    BuiltinRegistry::BuiltinRegistry() noexcept {
        this->add({
            .canonicalName = "abs",
            .id = BuiltinID::Abs,
            .overloads = {
                { {{ParamKind::AnyNumeric}}, builtins::sameAsFirst },
            }
        });

        this->add({
            .canonicalName = "sign",
            .id = BuiltinID::Sign,
            .overloads = {
                { {{ParamKind::AnyNumeric}}, builtins::scalarOfFirst },
            }
        });

        this->add({
            .canonicalName = "floor",
            .id = BuiltinID::Floor,
            .overloads = {
                { {{ParamKind::FloatOrFloatVec}}, builtins::sameAsFirst },
            }
        });

        this->add({
            .canonicalName = "ceil",
            .id = BuiltinID::Ceil,
            .overloads = {
                { {{ParamKind::FloatOrFloatVec}}, builtins::sameAsFirst },
            }
        });

        this->add({
            .canonicalName = "fract",
            .id = BuiltinID::Fract,
            .overloads = {
                { {{ParamKind::FloatOrFloatVec}}, builtins::sameAsFirst },
            }
        });

        this->add({
            .canonicalName = "trunc",
            .id = BuiltinID::Trunc,
            .overloads = {
                { {{ParamKind::FloatOrFloatVec}}, builtins::sameAsFirst },
            }
        });

        this->add({
            .canonicalName = "round",
            .id = BuiltinID::Round,
            .overloads = {
                { {{ParamKind::FloatOrFloatVec}}, builtins::sameAsFirst },
            }
        });

        this->add({
            .canonicalName = "min",
            .id = BuiltinID::Min,
            .overloads = {
                { {{ParamKind::AnyNumeric}, {ParamKind::AnyNumeric}}, builtins::sameAsFirst },
            }
        });

        this->add({
            .canonicalName = "max",
            .id = BuiltinID::Max,
            .overloads = {
                { {{ParamKind::AnyNumeric}, {ParamKind::AnyNumeric}}, builtins::sameAsFirst },
            }
        });

        this->add({
            .canonicalName = "clamp",
            .id = BuiltinID::Clamp,
            .overloads = {
                { {{ParamKind::FloatOrFloatVec}, {ParamKind::FloatScalar}, {ParamKind::FloatScalar}}, builtins::sameAsFirst },
                { {{ParamKind::FloatScalar}, {ParamKind::FloatScalar}, {ParamKind::FloatScalar}}, builtins::sameAsFirst },
                { {{ParamKind::IntScalar}, {ParamKind::IntScalar}, {ParamKind::IntScalar}}, builtins::sameAsFirst },
            }
        });

        this->add({
            .canonicalName = "mix",
            .id = BuiltinID::Mix,
            .overloads = {
                { {{ParamKind::FloatVec}, {ParamKind::FloatVec}, {ParamKind::FloatScalar}}, builtins::sameAsFirst },
                { {{ParamKind::FloatScalar}, {ParamKind::FloatScalar}, {ParamKind::FloatScalar}}, builtins::sameAsFirst },
                { {{ParamKind::FloatVec}, {ParamKind::FloatVec}, {ParamKind::FloatVec}}, builtins::sameAsFirst },
            }
        });

        this->add({
            .canonicalName = "step",
            .id = BuiltinID::Step,
            .overloads = {
                { {{ParamKind::FloatOrFloatVec}, {ParamKind::FloatOrFloatVec}}, builtins::sameAsFirst },
                { {{ParamKind::FloatScalar}, {ParamKind::FloatVec}}, [](auto args) { return args[1]; } },
            }
        });

        this->add({
            .canonicalName = "smoothstep",
            .id = BuiltinID::Smoothstep,
            .overloads = {
                { {{ParamKind::FloatScalar}, {ParamKind::FloatScalar}, {ParamKind::FloatScalar}}, builtins::sameAsFirst },
                { {{ParamKind::FloatVec}, {ParamKind::FloatVec}, {ParamKind::FloatVec}}, builtins::sameAsFirst },
                { {{ParamKind::FloatScalar}, {ParamKind::FloatScalar}, {ParamKind::FloatVec}}, [](auto args) { return args[2]; } },
            }
        });

        // --- Mod ---
        this->add({
            .canonicalName = "mod",
            .id = BuiltinID::Mod,
            .overloads = {
                { {{ParamKind::AnyNumeric}, {ParamKind::AnyNumeric}}, builtins::sameAsFirst },
            }
        });

        for (auto [name, id] : {
            std::pair{"sin", BuiltinID::Sin},
            {"cos", BuiltinID::Cos},
            {"tan", BuiltinID::Tan},
            {"asin", BuiltinID::Asin},
            {"acos", BuiltinID::Acos},
        }) {
            this->add({
                .canonicalName = name,
                .id = id,
                .overloads = {
                    { {{ParamKind::FloatOrFloatVec}}, builtins::sameAsFirst },
                }
            });
        }

        this->add({
            .canonicalName = "atan",
            .id = BuiltinID::Atan,
            .overloads = {
                { {{ParamKind::FloatOrFloatVec}}, builtins::sameAsFirst },
                { {{ParamKind::FloatScalar}, {ParamKind::FloatScalar}}, builtins::sameScalarAsFirst },
            }
        });

        this->add({
            .canonicalName = "atan2",
            .id = BuiltinID::Atan2,
            .overloads = {
                { {{ParamKind::FloatScalar}, {ParamKind::FloatScalar}}, builtins::sameScalarAsFirst },
            }
        });

        this->add({
            .canonicalName = "pow",
            .id = BuiltinID::Pow,
            .overloads = {
                { {{ParamKind::FloatOrFloatVec}, {ParamKind::FloatOrFloatVec}}, builtins::sameAsFirst },
            }
        });

        for (auto [name, id] : {
            std::pair{"sqrt", BuiltinID::Sqrt},
            {"exp", BuiltinID::Exp},
            {"exp2", BuiltinID::Exp2},
            {"log", BuiltinID::Log},
            {"log2", BuiltinID::Log2},
        }) {
            this->add({
                .canonicalName = name,
                .id = id,
                .overloads = {
                    { {{ParamKind::FloatOrFloatVec}}, builtins::sameAsFirst },
                }
            });
        }

        this->add({
            .canonicalName = "inversesqrt",
            .id = BuiltinID::InverseSqrt,
            .overloads = {
                { {{ParamKind::FloatOrFloatVec}}, builtins::sameAsFirst },
            }
        });

        this->add({
            .canonicalName = "dot",
            .id = BuiltinID::Dot,
            .overloads = {
                { {{ParamKind::FloatVec}, {ParamKind::FloatVec}}, builtins::scalarOfFirst },
            }
        });

        this->add({
            .canonicalName = "cross",
            .id = BuiltinID::Cross,
            .overloads = {
                { {{ParamKind::FloatVec}, {ParamKind::FloatVec}}, builtins::crossResult },
            }
        });

        this->add({
            .canonicalName = "normalize",
            .id = BuiltinID::Normalize,
            .overloads = {
                { {{ParamKind::FloatVec}}, builtins::sameAsFirst },
            }
        });

        this->add({
            .canonicalName = "length",
            .id = BuiltinID::Length,
            .overloads = {
                { {{ParamKind::FloatOrFloatVec}}, builtins::scalarOfFirst },
            }
        });

        this->add({
            .canonicalName = "distance",
            .id = BuiltinID::Distance,
            .overloads = {
                { {{ParamKind::FloatOrFloatVec}, {ParamKind::FloatOrFloatVec}}, builtins::sameScalarAsFirst },
            }
        });

        this->add({
            .canonicalName = "reflect",
            .id = BuiltinID::Reflect,
            .overloads = {
                { {{ParamKind::FloatVec}, {ParamKind::FloatVec}}, builtins::sameAsFirst },
            }
        });

        this->add({
            .canonicalName = "refract",
            .id = BuiltinID::Refract,
            .overloads = {
                { {{ParamKind::FloatVec}, {ParamKind::FloatVec}, {ParamKind::FloatScalar}}, builtins::sameAsFirst },
            }
        });

        this->add({
            .canonicalName = "texture",
            .id = BuiltinID::Texture,
            .overloads = {
                { {{ParamKind::Sampler}, {ParamKind::FloatVec}}, builtins::textureResult },
            }
        });

        for (auto [name, id] : {
            std::pair{"dFdx", BuiltinID::DFdx},
            {"dFdy", BuiltinID::DFdy},
            {"fwidth", BuiltinID::Fwidth},
        }) {
            this->add({
                .canonicalName = name,
                .id = id,
                .overloads = {
                    { {{ParamKind::FloatOrFloatVec}}, builtins::sameAsFirst },
                }
            });
        }

        this->addBackendName(BuiltinID::Mix, CodeBackend::HLSL, "lerp");
        this->addBackendName(BuiltinID::InverseSqrt, CodeBackend::HLSL, "rsqrt");
        this->addBackendName(BuiltinID::DFdx, CodeBackend::HLSL, "ddx");
        this->addBackendName(BuiltinID::DFdy, CodeBackend::HLSL, "ddy");
        this->addBackendName(BuiltinID::Fract, CodeBackend::HLSL, "frac");
        this->addBackendName(BuiltinID::Mod, CodeBackend::HLSL, "fmod");
        this->addBackendName(BuiltinID::Fwidth, CodeBackend::HLSL, "fwidth");

        this->addBackendName(BuiltinID::InverseSqrt, CodeBackend::MSL, "rsqrt");
        this->addBackendName(BuiltinID::DFdx, CodeBackend::MSL, "dfdx");
        this->addBackendName(BuiltinID::DFdy, CodeBackend::MSL, "dfdy");
        this->addBackendName(BuiltinID::Mod, CodeBackend::MSL, "fmod");
    }
}