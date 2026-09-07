#pragma once

#include "../ir.hpp"

namespace fsher::backend {
    enum class Precedence : uint8_t {
        None = 0,
        Assignment,     // = += -= ...
        Ternary,        // ? :
        Pipeline,       // |>
        LogicOr,        // ||
        LogicAnd,       // &&
        BitOr,          // |
        BitXor,         // ^
        BitAnd,         // &
        Equality,       // == !=
        Comparison,     // < > <= >=
        Shift,          // << >>
        Additive,       // + -
        Multiplicative, // * / %
        Unary,          // ! - ~
        Postfix,        // .index, .field, ()
    };

    inline Precedence getPrecedence(TokenType type) noexcept {
        switch (type) {
            case TokenType::EQUAL:
            case TokenType::PLUS_EQUAL:
            case TokenType::MINUS_EQUAL:
            case TokenType::ASTERISK_EQUAL:
            case TokenType::SLASH_EQUAL:
            case TokenType::PERCENT_EQUAL:
            case TokenType::CARET_EQUAL:
            case TokenType::AMPERSAND_EQUAL:
            case TokenType::PIPE_EQUAL:
            case TokenType::LEFT_SHIFT_EQUAL:
            case TokenType::RIGHT_SHIFT_EQUAL:
                return Precedence::Assignment;

            case TokenType::QUESTION:
                return Precedence::Ternary;

            case TokenType::PIPE_ARROW:
                return Precedence::Pipeline;

            case TokenType::PIPE_PIPE:
                return Precedence::LogicOr;

            case TokenType::AMPERSAND_AMPERSAND:
                return Precedence::LogicAnd;

            case TokenType::PIPE:
                return Precedence::BitOr;

            case TokenType::CARET:
                return Precedence::BitXor;

            case TokenType::AMPERSAND:
                return Precedence::BitAnd;

            case TokenType::EQUAL_EQUAL:
            case TokenType::BANG_EQUAL:
                return Precedence::Equality;

            case TokenType::LESS:
            case TokenType::GREATER:
            case TokenType::LESS_EQUAL:
            case TokenType::GREATER_EQUAL:
                return Precedence::Comparison;

            case TokenType::LEFT_SHIFT:
            case TokenType::RIGHT_SHIFT:
                return Precedence::Shift;

            case TokenType::PLUS:
            case TokenType::MINUS:
                return Precedence::Additive;

            case TokenType::ASTERISK:
            case TokenType::SLASH:
            case TokenType::PERCENT:
                return Precedence::Multiplicative;

            case TokenType::LPAREN:
            case TokenType::LBRACKET:
            case TokenType::DOT:
                return Precedence::Postfix;

            default:
                return Precedence::None;
        }
    }

    struct Output {
        std::string vertex;
        std::string fragment;
    };

    enum class CodeStyle : uint8_t {
        Compact,
        Pretty,

        Default = Pretty,
    };

    class IGenerator {
    public:
        virtual ~IGenerator() = default;

        virtual Output generate(ir::ShaderProgram const& program, CodeStyle style = CodeStyle::Default) = 0;
    };
}