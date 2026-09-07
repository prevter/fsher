#pragma once
#include <cstdint>
#include <variant>
#include <fmt/format.h>
#include <Geode/Result.hpp>

#include "expr.hpp"

namespace fsher {
    class LiteralExprNode : public ExprNode {
    public:
        using Value = std::variant<int64_t, double, bool>;

        LiteralExprNode(SourceRange const& range, Value value) noexcept
            : ExprNode(range, Type::LiteralExpr), m_value(value) {}

        static geode::Result<Value, std::string_view> parseLiteralValue(Token const& token) {
            using geode::Ok, geode::Err;
            switch (token.type) {
                case TokenType::TRUE: return Ok(true);
                case TokenType::FALSE: return Ok(false);
                case TokenType::INTEGER: {
                    // TODO: support underscores '_' for digit grouping
                    std::string_view lexeme = token.lexeme;
                    int base = 10;
                    if (lexeme.starts_with("0x") || lexeme.starts_with("0X")) {
                        lexeme.remove_prefix(2);
                        base = 16;
                    } else if (lexeme.starts_with("0o") || lexeme.starts_with("0O")) {
                        lexeme.remove_prefix(2);
                        base = 8;
                    } else if (lexeme.starts_with("0b") || lexeme.starts_with("0B")) {
                        lexeme.remove_prefix(2);
                        base = 2;
                    }

                    uint64_t value = 0;
                    auto [ptr, ec] = std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), value, base);

                    if (ec != std::errc()) {
                        return Err("Failed to parse integer literal");
                    }

                    return Ok(static_cast<int64_t>(value));
                }
                case TokenType::FLOAT: {
                    std::string_view lexeme = token.lexeme;
                    double value = 0.0;
                    auto [ptr, ec] = std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), value);
                    if (ec != std::errc()) {
                        return Err("Failed to parse float literal");
                    }

                    return Ok(value);
                }
                default: return Err("Invalid literal token");
            }
        }

        void debug(fmt::appender out, int indent = 0) const override {
            std::visit([&](auto const& v) {
                fmt::format_to(out, "{:>{}}LiteralExprNode {{ value: {} }}\n", "", indent, v);
            }, m_value);
        }

        enum class ValueType : uint8_t {
            Integer, Float, Boolean
        };

        [[nodiscard]] ValueType valueType() const noexcept {
            return static_cast<ValueType>(m_value.index());
        }

        [[nodiscard]] int64_t intValue() const noexcept {
            return std::get<int64_t>(m_value);
        }

        [[nodiscard]] double floatValue() const noexcept {
            return std::get<double>(m_value);
        }

        [[nodiscard]] bool boolValue() const noexcept {
            return std::get<bool>(m_value);
        }

        [[nodiscard]] std::variant<int64_t, double, bool> value() const noexcept {
            return m_value;
        }

        [[nodiscard]] std::variant<int64_t, double, bool> getAs(ValueType type) const noexcept {
            switch (type) {
                case ValueType::Integer: {
                    switch (m_value.index()) {
                        case 0: return std::get<int64_t>(m_value);
                        case 1: return static_cast<int64_t>(std::get<double>(m_value));
                        case 2: return std::get<bool>(m_value) ? 1 : 0;
                        default: return 0;
                    }
                }
                case ValueType::Float: {
                    switch (m_value.index()) {
                        case 0: return static_cast<double>(std::get<int64_t>(m_value));
                        case 1: return std::get<double>(m_value);
                        case 2: return std::get<bool>(m_value) ? 1.0 : 0.0;
                        default: return 0.0;
                    }
                }
                case ValueType::Boolean: {
                    switch (m_value.index()) {
                        case 0: return std::get<int64_t>(m_value) != 0;
                        case 1: return std::get<double>(m_value) != 0.0;
                        case 2: return std::get<bool>(m_value);
                        default: return false;
                    }
                }
            }
            return {};
        }

    private:
        Value m_value;
    };
}