#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <Geode/Result.hpp>

#include "tokens.hpp"

namespace fsher {
    struct LexingError {
        enum class Kind : uint8_t {
            UnexpectedCharacter = 1,
            InvalidNumber = 2
        } kind;
        char unexpectedChar;
        uint32_t line;
        uint32_t column;
    };

    class Lexer {
    public:
        using Result = geode::Result<Token, LexingError>;

        explicit Lexer(std::string_view source) noexcept;

        Result next() noexcept;

    private:
        [[nodiscard]] bool isEnd() const noexcept;
        char peek() const noexcept;
        char peekAt(size_t offset) const noexcept;
        char advance() noexcept;

        Result lexKeywordOrIdentifier() noexcept;
        Result lexNumber() noexcept;

        static Result unexpected(char c, uint32_t line, uint32_t column) noexcept;

    private:
        std::string_view m_source;
        size_t m_pos = 0;
        uint32_t m_line = 1, m_column = 1;
    };
}