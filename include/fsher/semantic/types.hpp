#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <fmt/format.h>

namespace fsher {
    enum class ScalarKind : uint8_t {
        Void,
        Bool,

        UntypedInt, UntypedFloat,

        Float32, Float64,
        Int32, UInt32,
    };

    inline std::string_view format_as(ScalarKind kind) noexcept {
        switch (kind) {
            case ScalarKind::Void: return "void";
            case ScalarKind::Bool: return "bool";
            case ScalarKind::UntypedInt: return "integer";
            case ScalarKind::UntypedFloat: return "decimal";
            case ScalarKind::Float32: return "f32";
            case ScalarKind::Float64: return "f64";
            case ScalarKind::Int32: return "i32";
            case ScalarKind::UInt32: return "u32";
        }
        return "unknown";
    }

    struct Type {
        enum class Kind : uint8_t {
            Scalar,
            Vector, Matrix,
            Sampler2D,
            Struct,
            Array,
            Void,
            Resolving,
            Unresolved,
            Error
        };

        std::string_view structName;
        Kind kind = Kind::Error;
        ScalarKind scalarKind = ScalarKind::Void;
        uint8_t rows = 0, cols = 0;

        std::shared_ptr<Type const> elementType;
        uint64_t arrayLength = 0;

        static constexpr Type Error() noexcept {
            return Type{.kind = Kind::Error};
        }

        static constexpr Type Void() noexcept {
            return Type{.kind = Kind::Void, .scalarKind = ScalarKind::Void};
        }

        static constexpr Type Float(bool isDouble = false) noexcept {
            return Type{.kind = Kind::Scalar, .scalarKind = isDouble ? ScalarKind::Float64 : ScalarKind::Float32};
        }

        static constexpr Type Numeric(bool isFloat = false) noexcept {
            return Type{.kind = Kind::Scalar, .scalarKind = isFloat ? ScalarKind::UntypedFloat : ScalarKind::UntypedInt};
        }

        static constexpr Type Int(bool isUnsigned = false) noexcept {
            return Type{.kind = Kind::Scalar, .scalarKind = isUnsigned ? ScalarKind::UInt32 : ScalarKind::Int32};
        }

        static constexpr Type Bool() noexcept {
            return Type{.kind = Kind::Scalar, .scalarKind = ScalarKind::Bool};
        }

        static constexpr Type Sampler2D() noexcept {
            return Type{.kind = Kind::Sampler2D, .scalarKind = ScalarKind::Void};
        }

        static constexpr Type Struct(std::string_view name) noexcept {
            return Type{.structName = name, .kind = Kind::Struct, .scalarKind = ScalarKind::Void};
        }

        static constexpr Type Vector(ScalarKind scalarKind, uint8_t size) noexcept {
            return Type{.kind = Kind::Vector, .scalarKind = scalarKind, .rows = size, .cols = 1};
        }

        static constexpr Type Matrix(ScalarKind scalarKind, uint8_t cols, uint8_t rows) noexcept {
            return Type{.kind = Kind::Matrix, .scalarKind = scalarKind, .rows = rows, .cols = cols};
        }

        static constexpr Type Array(Type elementType, uint64_t length) noexcept {
            return Type{
                .kind = Kind::Array,
                .scalarKind = ScalarKind::Void,
                .elementType = std::make_shared<Type const>(std::move(elementType)),
                .arrayLength = length
            };
        }

        [[nodiscard]] Type const& elementBase() const noexcept {
            auto current = this;
            while (current->kind == Kind::Array && current->elementType) {
                current = current->elementType.get();
            }
            return *current;
        }

        [[nodiscard]] std::string arraySuffix() const {
            std::string result;
            auto current = this;
            while (current->kind == Kind::Array) {
                result += fmt::format("[{}]", current->arrayLength);
                if (!current->elementType) break;
                current = current->elementType.get();
            }
            return result;
        }

        bool operator==(Type const& o) const noexcept {
            if (o.kind != kind) return false;
            if (o.scalarKind != scalarKind) return false;
            if (o.cols != cols) return false;
            if (o.rows != rows) return false;
            if (o.structName != structName) return false;

            if (kind == Kind::Array) {
                if (arrayLength != o.arrayLength) return false;
                if (static_cast<bool>(elementType) != static_cast<bool>(o.elementType)) return false;
                if (elementType && o.elementType && *elementType != *o.elementType) return false;
            }

            return true;
        }

        static constexpr size_t sizeOfScalar(ScalarKind kind) noexcept {
            switch (kind) {
                case ScalarKind::Float32: return 4;
                case ScalarKind::Float64: return 8;
                case ScalarKind::Int32: return 4;
                case ScalarKind::UInt32: return 4;
                case ScalarKind::Bool: return 1;
                default: return 0;
            }
        }

        size_t sizeOf() const noexcept {
            switch (kind) {
                case Kind::Scalar:
                    return sizeOfScalar(scalarKind);
                case Kind::Vector:
                    return sizeOfScalar(scalarKind) * rows;
                case Kind::Matrix:
                    return sizeOfScalar(scalarKind) * rows * cols;
                case Kind::Array:
                    if (elementType) {
                        return elementType->sizeOf() * arrayLength;
                    }
                    return 0;
                default:
                case Kind::Struct:
                    // TODO: Calculate size based on struct members
                    // would have to look up the struct definition
                    return 0;
            }
        }
    };
}

template <>
struct fmt::formatter<fsher::Type> {
    enum class NamingStyle { Rust, C } style = NamingStyle::Rust;

    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin();
        auto end = ctx.end();

        if (it != end && *it != '}') {
            if (*it == 'r') {
                style = NamingStyle::Rust;
                ++it;
            } else if (*it == 'c') {
                style = NamingStyle::C;
                ++it;
            }
        }

        if (it != end && *it != '}') {
            throw format_error("invalid format specifier for fsher::Type");
        }

        return it;
    }

    auto format(fsher::Type const& type, format_context& ctx) const noexcept {
        auto scalarPrefix = [](fsher::ScalarKind kind) -> char {
            switch (kind) {
                case fsher::ScalarKind::Float64: return 'd';
                case fsher::ScalarKind::Bool: return 'b';
                case fsher::ScalarKind::Int32: return 'i';
                case fsher::ScalarKind::UInt32: return 'u';
                default: return '\0';
            }
        };

        switch (type.kind) {
            case fsher::Type::Kind::Scalar:
                if (style == NamingStyle::C) {
                    auto name = [&] -> std::string_view {
                        switch (type.scalarKind) {
                            case fsher::ScalarKind::Float32: return "float";
                            case fsher::ScalarKind::Float64: return "double";
                            case fsher::ScalarKind::Bool: return "bool";
                            case fsher::ScalarKind::Int32: return "int";
                            case fsher::ScalarKind::UInt32: return "uint";
                            case fsher::ScalarKind::Void: return "void";
                            default: return "<unknown>";
                        }
                    }();
                    return fmt::format_to(ctx.out(), "{}", name);
                }
                return fmt::format_to(ctx.out(), "{}", type.scalarKind);
            case fsher::Type::Kind::Vector: {
                if (type.scalarKind == fsher::ScalarKind::Float32) {
                    return fmt::format_to(ctx.out(), "vec{}", type.rows);
                }

                char prefix = scalarPrefix(type.scalarKind);
                if (prefix == '\0') break;

                return fmt::format_to(ctx.out(), "{}vec{}", prefix, type.rows);
            }
            case fsher::Type::Kind::Matrix: {
                if (type.scalarKind == fsher::ScalarKind::Float32) {
                    fmt::format_to(ctx.out(), "mat");
                } else {
                    char prefix = scalarPrefix(type.scalarKind);
                    if (prefix == '\0') break;
                    fmt::format_to(ctx.out(), "{}mat", prefix);
                }

                if (type.rows == type.cols) {
                    return fmt::format_to(ctx.out(), "{}", type.rows);
                }

                return fmt::format_to(ctx.out(), "{}x{}", type.cols, type.rows);
            }
            case fsher::Type::Kind::Sampler2D:
                return fmt::format_to(ctx.out(), "sampler2D");
            case fsher::Type::Kind::Struct:
                return fmt::format_to(ctx.out(), "{}", type.structName);
            case fsher::Type::Kind::Void:
                return fmt::format_to(ctx.out(), "void");
            case fsher::Type::Kind::Unresolved:
                return fmt::format_to(ctx.out(), "Unresolved");
            default: break;
        }

        return fmt::format_to(ctx.out(), "Error");
    }
};