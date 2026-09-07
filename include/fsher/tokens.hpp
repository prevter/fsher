#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <fmt/format.h>

namespace fsher {
    enum class TokenType : uint8_t {
        // --- Literals --- //
        IDENTIFIER, INTEGER, FLOAT,

        // --- Arithmetic operators --- //
        PLUS, MINUS, ASTERISK, SLASH, PERCENT, CARET,

        // --- Bitwise operators --- //
        AMPERSAND, PIPE, LEFT_SHIFT, RIGHT_SHIFT, TILDE,

        // --- Logical operators --- //
        AMPERSAND_AMPERSAND, PIPE_PIPE, BANG,

        // --- Comparison operators --- //
        EQUAL_EQUAL, BANG_EQUAL, GREATER, GREATER_EQUAL, LESS, LESS_EQUAL,

        // --- Assignment operators --- //
        EQUAL,
        PLUS_EQUAL, MINUS_EQUAL, ASTERISK_EQUAL, SLASH_EQUAL, PERCENT_EQUAL,
        CARET_EQUAL, AMPERSAND_EQUAL, PIPE_EQUAL, LEFT_SHIFT_EQUAL, RIGHT_SHIFT_EQUAL,

        // --- Punctuation / delimiters --- //
        LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
        COMMA, DOT, SEMICOLON, COLON, HASH, QUESTION,
        ARROW, FAT_ARROW, PIPE_ARROW, HASH_BRACKET,
        DOT_DOT, DOT_DOT_EQUAL,

        // --- Keywords: declarations --- //
        FN, LET, CONST, UNIFORM, IN, OUT, STRUCT,

        // --- Keywords: control flow --- //
        IF, ELSE, FOR, RETURN, CONTINUE, BREAK, DISCARD, MATCH,

        // --- Keywords: literals --- //
        TRUE, FALSE,

        // --- Special --- //
        END_OF_FILE, ERROR
    };

    inline std::string_view format_as(TokenType type) noexcept {
        switch (type) {
            // --- Literals --- //
            case TokenType::IDENTIFIER: return "IDENTIFIER";
            case TokenType::INTEGER: return "INTEGER";
            case TokenType::FLOAT: return "FLOAT";
            // --- Arithmetic operators --- //
            case TokenType::PLUS: return "PLUS";
            case TokenType::MINUS: return "MINUS";
            case TokenType::ASTERISK: return "ASTERISK";
            case TokenType::SLASH: return "SLASH";
            case TokenType::PERCENT: return "PERCENT";
            case TokenType::CARET: return "CARET";
            // --- Bitwise operators --- //
            case TokenType::AMPERSAND: return "AMPERSAND";
            case TokenType::PIPE: return "PIPE";
            case TokenType::LEFT_SHIFT: return "LEFT_SHIFT";
            case TokenType::RIGHT_SHIFT: return "RIGHT_SHIFT";
            case TokenType::TILDE: return "TILDE";
            // --- Logical operators --- //
            case TokenType::AMPERSAND_AMPERSAND: return "AMPERSAND_AMPERSAND";
            case TokenType::PIPE_PIPE: return "PIPE_PIPE";
            case TokenType::BANG: return "BANG";
            // --- Comparison operators --- //
            case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
            case TokenType::BANG_EQUAL: return "BANG_EQUAL";
            case TokenType::GREATER: return "GREATER";
            case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
            case TokenType::LESS: return "LESS";
            case TokenType::LESS_EQUAL: return "LESS_EQUAL";
            // --- Assignment operators --- //
            case TokenType::EQUAL: return "EQUAL";
            case TokenType::PLUS_EQUAL: return "PLUS_EQUAL";
            case TokenType::MINUS_EQUAL: return "MINUS_EQUAL";
            case TokenType::ASTERISK_EQUAL: return "ASTERISK_EQUAL";
            case TokenType::SLASH_EQUAL: return "SLASH_EQUAL";
            case TokenType::PERCENT_EQUAL: return "PERCENT_EQUAL";
            case TokenType::CARET_EQUAL: return "CARET_EQUAL";
            case TokenType::AMPERSAND_EQUAL: return "AMPERSAND_EQUAL";
            case TokenType::PIPE_EQUAL: return "PIPE_EQUAL";
            case TokenType::LEFT_SHIFT_EQUAL: return "LEFT_SHIFT_EQUAL";
            case TokenType::RIGHT_SHIFT_EQUAL: return "RIGHT_SHIFT_EQUAL";
            // --- Punctuation / delimiters --- //
            case TokenType::LPAREN: return "LPAREN";
            case TokenType::RPAREN: return "RPAREN";
            case TokenType::LBRACE: return "LBRACE";
            case TokenType::RBRACE: return "RBRACE";
            case TokenType::LBRACKET: return "LBRACKET";
            case TokenType::RBRACKET: return "RBRACKET";
            case TokenType::COMMA: return "COMMA";
            case TokenType::DOT: return "DOT";
            case TokenType::SEMICOLON: return "SEMICOLON";
            case TokenType::COLON: return "COLON";
            case TokenType::HASH: return "HASH";
            case TokenType::QUESTION: return "QUESTION";
            case TokenType::ARROW: return "ARROW";
            case TokenType::FAT_ARROW: return "FAT_ARROW";
            case TokenType::PIPE_ARROW: return "PIPE_ARROW";
            case TokenType::HASH_BRACKET: return "HASH_BRACKET";
            case TokenType::DOT_DOT: return "DOT_DOT";
            case TokenType::DOT_DOT_EQUAL: return "DOT_DOT_EQUAL";
            // --- Keywords: declarations --- //
            case TokenType::FN: return "FN";
            case TokenType::LET: return "LET";
            case TokenType::CONST: return "CONST";
            case TokenType::UNIFORM: return "UNIFORM";
            case TokenType::IN: return "IN";
            case TokenType::OUT: return "OUT";
            case TokenType::STRUCT: return "STRUCT";
            // --- Keywords: control flow --- //
            case TokenType::IF: return "IF";
            case TokenType::ELSE: return "ELSE";
            case TokenType::FOR: return "FOR";
            case TokenType::RETURN: return "RETURN";
            case TokenType::CONTINUE: return "CONTINUE";
            // -- - Keywords: control flow --- //
            case TokenType::BREAK: return "BREAK";
            case TokenType::DISCARD: return "DISCARD";
            case TokenType::MATCH: return "MATCH";
            // --- Keywords: literals --- //
            case TokenType::TRUE: return "TRUE";
            case TokenType::FALSE: return "FALSE";
            // --- Special --- //
            case TokenType::END_OF_FILE: return "END_OF_FILE";
            case TokenType::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    struct SourceLocation {
        size_t line = 0;
        size_t column = 0;
    };

    struct SourceRange {
        SourceLocation start;
        SourceLocation end;
    };

    struct Token {
        TokenType type = TokenType::ERROR;
        std::string_view lexeme;
        SourceLocation location;

        Token() = default;
        Token(TokenType type, std::string_view lexeme, SourceLocation location)
            : type(type), lexeme(lexeme), location(location) {}

        static Token EOFToken(size_t line, size_t column) {
            return Token{TokenType::END_OF_FILE, "", {line, column}};
        }

        static Token Error(size_t line, size_t column) {
            return Token{TokenType::ERROR, "", {line, column}};
        }

        static Token Identifier(std::string_view lexeme, size_t line, size_t column) {
            return Token{TokenType::IDENTIFIER, lexeme, {line, column}};
        }

        static Token Integer(std::string_view lexeme, size_t line, size_t column) {
            return Token{TokenType::INTEGER, lexeme, {line, column}};
        }

        static Token Float(std::string_view lexeme, size_t line, size_t column) {
            return Token{TokenType::FLOAT, lexeme, {line, column}};
        }

        static Token Create(TokenType type, std::string_view lexeme, size_t line, size_t column) {
            return Token{type, lexeme, {line, column}};
        }
    };
}

template <>
struct fmt::formatter<fsher::Token> {
    static constexpr auto parse(format_parse_context& ctx) noexcept {
        return ctx.begin();
    }

    auto format(fsher::Token const& token, format_context& ctx) const noexcept {
        return fmt::format_to(ctx.out(), "Token(type={}, lexeme='{}', location={}:{})", token.type, token.lexeme, token.location.line, token.location.column);
    }
};
