#include <fsher/lexer.hpp>

namespace fsher {
    using geode::Err, geode::Ok;

    static constexpr bool isWhitespace(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    static constexpr bool isIdentifierStart(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    static constexpr bool isDigit(char c) noexcept {
        return c >= '0' && c <= '9';
    }

    static constexpr bool isHexDigit(char c) noexcept {
        return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    static constexpr bool isIdentifierPart(char c) noexcept {
        return isIdentifierStart(c) || isDigit(c) || static_cast<uint8_t>(c) >= 0x80;
    }

    template <char... Chars>
    static consteval std::string_view combineChars() {
        static constexpr std::array<char, sizeof...(Chars) + 1> arr = []() constexpr {
            std::array<char, sizeof...(Chars) + 1> temp{};
            size_t i = 0;
            ((temp[i++] = Chars), ...);
            return temp;
        }();
        return std::string_view(arr.data(), arr.size() - 1);
    }

    Lexer::Lexer(std::string_view source) noexcept
        : m_source(source) {}

    Lexer::Result Lexer::next() noexcept {
        // skip whitespace
        while (!this->isEnd()) {
            char c = this->peek();
            if (isWhitespace(c)) {
                this->advance();
            } else if (c == '/' && this->peekAt(1) == '/') {
                // skip line comment
                while (!this->isEnd() && this->peek() != '\n') {
                    this->advance();
                }
            } else if (c == '/' && this->peekAt(1) == '*') {
                // skip block comment
                this->advance();
                this->advance();
                while (!this->isEnd()) {
                    if (this->peek() == '*' && this->peekAt(1) == '/') {
                        this->advance();
                        this->advance();
                        break;
                    }
                    this->advance();
                }
            } else {
                break;
            }
        }

        if (this->isEnd()) {
            return Ok(Token::EOFToken(m_line, m_column));
        }

        char c = m_source[m_pos];
        uint32_t line = m_line, col = m_column;

        // for single tokens
    #define LEX_SINGLE_CHAR(ch, t) \
        case ch: this->advance(); return Ok(Token::Create(TokenType::t, combineChars<ch>(), line, col));

        // for assignment tokens
    #define LEX_COMPOUND_CHAR(ch, t1, t2) \
        case ch: { \
            this->advance(); \
            if (this->peek() == '=') { \
                this->advance(); \
                return Ok(Token::Create(TokenType::t2, combineChars<ch, '='>(), line, col)); \
            } \
            return Ok(Token::Create(TokenType::t1, combineChars<ch>(), line, col)); \
        }

        // bit shifts
    #define LEX_SHIFT_CHAR(ch, t1, t2, t3, t4) \
        case ch: { \
            this->advance(); \
            char next = this->peek(); \
            if (next == ch) { \
                this->advance(); \
                if (this->peek() == '=') { \
                    this->advance(); \
                    return Ok(Token::Create(TokenType::t4, combineChars<ch, ch, '='>(), line, col)); \
                } \
                return Ok(Token::Create(TokenType::t3, combineChars<ch, ch>(), line, col)); \
            } \
            if (next == '=') { \
                this->advance(); \
                return Ok(Token::Create(TokenType::t2, combineChars<ch, '='>(), line, col)); \
            } \
            return Ok(Token::Create(TokenType::t1, combineChars<ch>(), line, col)); \
        }

        switch (c) {
            LEX_SINGLE_CHAR('(', LPAREN);
            LEX_SINGLE_CHAR(')', RPAREN);
            LEX_SINGLE_CHAR('[', LBRACKET);
            LEX_SINGLE_CHAR(']', RBRACKET);
            LEX_SINGLE_CHAR('{', LBRACE);
            LEX_SINGLE_CHAR('}', RBRACE);
            LEX_SINGLE_CHAR(',', COMMA);
            LEX_SINGLE_CHAR(';', SEMICOLON);
            LEX_SINGLE_CHAR(':', COLON);
            LEX_SINGLE_CHAR('~', TILDE);
            LEX_SINGLE_CHAR('?', QUESTION);

            LEX_COMPOUND_CHAR('+', PLUS, PLUS_EQUAL);
            LEX_COMPOUND_CHAR('*', ASTERISK, ASTERISK_EQUAL);
            LEX_COMPOUND_CHAR('/', SLASH, SLASH_EQUAL);
            LEX_COMPOUND_CHAR('%', PERCENT, PERCENT_EQUAL);
            LEX_COMPOUND_CHAR('^', CARET, CARET_EQUAL);
            LEX_COMPOUND_CHAR('!', BANG, BANG_EQUAL);

            LEX_SHIFT_CHAR('<', LESS, LESS_EQUAL, LEFT_SHIFT, LEFT_SHIFT_EQUAL);
            LEX_SHIFT_CHAR('>', GREATER, GREATER_EQUAL, RIGHT_SHIFT, RIGHT_SHIFT_EQUAL);

            case '#': {
                this->advance();
                if (this->peek() == '[') {
                    this->advance();
                    return Ok(Token::Create(TokenType::HASH_BRACKET, "#[", line, col));
                }
                return Ok(Token::Create(TokenType::HASH, "#", line, col));
            }

            case '&': {
                this->advance();
                char next = this->peek();
                if (next == '&') {
                    this->advance();
                    return Ok(Token::Create(TokenType::AMPERSAND_AMPERSAND, "&&", line, col));
                }
                if (next == '=') {
                    this->advance();
                    return Ok(Token::Create(TokenType::AMPERSAND_EQUAL, "&=", line, col));
                }
                return Ok(Token::Create(TokenType::AMPERSAND, "&", line, col));
            }

            case '|': {
                this->advance();
                char next = this->peek();
                if (next == '|') {
                    this->advance();
                    return Ok(Token::Create(TokenType::PIPE_PIPE, "||", line, col));
                }
                if (next == '=') {
                    this->advance();
                    return Ok(Token::Create(TokenType::PIPE_EQUAL, "|=", line, col));
                }
                if (next == '>') {
                    this->advance();
                    return Ok(Token::Create(TokenType::PIPE_ARROW, "|>", line, col));
                }
                return Ok(Token::Create(TokenType::PIPE, "|", line, col));
            }

            case '-': {
                this->advance();
                char next = this->peek();
                if (next == '>') {
                    this->advance();
                    return Ok(Token::Create(TokenType::ARROW, "->", line, col));
                }
                if (next == '=') {
                    this->advance();
                    return Ok(Token::Create(TokenType::MINUS_EQUAL, "-=", line, col));
                }
                return Ok(Token::Create(TokenType::MINUS, "-", line, col));
            }

            case '.': {
                if (isDigit(this->peekAt(1))) {
                    return this->lexNumber();
                }
                this->advance();
                if (this->peek() == '.') {
                    this->advance();
                    if (this->peek() == '=') {
                        this->advance();
                        return Ok(Token::Create(TokenType::DOT_DOT_EQUAL, "..=", line, col));
                    }
                    return Ok(Token::Create(TokenType::DOT_DOT, "..", line, col));
                }
                return Ok(Token::Create(TokenType::DOT, ".", line, col));
            }

            case '=': {
                this->advance();
                if (this->peek() == '=') {
                    this->advance();
                    return Ok(Token::Create(TokenType::EQUAL_EQUAL, "==", line, col));
                }
                if (this->peek() == '>') {
                    this->advance();
                    return Ok(Token::Create(TokenType::FAT_ARROW, "=>", line, col));
                }
                return Ok(Token::Create(TokenType::EQUAL, "=", line, col));
            }

            default: {
                if (isIdentifierStart(c)) {
                    return this->lexKeywordOrIdentifier();
                }
                if (isDigit(c)) {
                    return this->lexNumber();
                }
                if (static_cast<uint8_t>(c) >= 0x80) {
                    return this->lexKeywordOrIdentifier();
                }
                return this->unexpected(c, line, col);
            }
        }
    }

    bool Lexer::isEnd() const noexcept {
        return m_pos >= m_source.size();
    }

    char Lexer::peek() const noexcept {
        return this->isEnd() ? '\0' : m_source[m_pos];
    }

    char Lexer::peekAt(size_t offset) const noexcept {
        size_t idx = m_pos + offset;
        return idx >= m_source.size() ? '\0' : m_source[idx];
    }

    char Lexer::advance() noexcept {
        char c = m_source[m_pos++];
        if (c == '\n') {
            m_line++;
            m_column = 1;
        } else {
            m_column++;
        }
        return c;
    }

    Lexer::Result Lexer::lexKeywordOrIdentifier() noexcept {
        size_t start = m_pos;
        uint32_t line = m_line, col = m_column;

        while (!this->isEnd() && isIdentifierPart(this->peek())) {
            this->advance();
        }

        std::string_view text = m_source.substr(start, m_pos - start);
        auto type = TokenType::IDENTIFIER;

        struct Keyword { std::string_view name; TokenType type; };
        static constexpr auto keywords = std::to_array<Keyword>({
            {"break", TokenType::BREAK},
            {"const", TokenType::CONST},
            {"continue", TokenType::CONTINUE},
            {"discard", TokenType::DISCARD},
            {"else", TokenType::ELSE},
            {"false", TokenType::FALSE},
            {"fn", TokenType::FN},
            {"for", TokenType::FOR},
            {"if", TokenType::IF},
            {"in", TokenType::IN},
            {"let", TokenType::LET},
            {"match", TokenType::MATCH},
            {"out", TokenType::OUT},
            {"return", TokenType::RETURN},
            {"struct", TokenType::STRUCT},
            {"true", TokenType::TRUE},
            {"uniform", TokenType::UNIFORM}
        });

        auto it = std::ranges::lower_bound(keywords, text, {}, &Keyword::name);
        if (it != keywords.end() && it->name == text) {
            type = it->type;
        }

        return Ok(Token::Create(type, text, line, col));
    }

    Lexer::Result Lexer::lexNumber() noexcept {
        size_t start = m_pos;
        uint32_t line = m_line, col = m_column;

        enum class Base { Decimal, Hex, Binary, Octal } base = Base::Decimal;

        // handle prefix
        if (m_source[m_pos] == '0' && m_pos + 1 < m_source.size()) {
            char next = m_source[m_pos + 1];
            if (next == 'x' || next == 'X') {
                base = Base::Hex;
                this->advance();
                this->advance();
            } else if (next == 'b' || next == 'B') {
                base = Base::Binary;
                this->advance();
                this->advance();
            } else if (next == 'o' || next == 'O') {
                base = Base::Octal;
                this->advance();
                this->advance();
            }
        }

        if (base != Base::Decimal) {
            size_t digitsStart = m_pos;
            while (!this->isEnd()) {
                char c = m_source[m_pos];
                bool valid = false;

                if (base == Base::Hex) {
                    valid = isHexDigit(c);
                } else if (base == Base::Binary) {
                    valid = c == '0' || c == '1';
                } else {
                    valid = c >= '0' && c <= '7';
                }

                if (valid) {
                    this->advance();
                } else {
                    break;
                }
            }

            if (m_pos == digitsStart || (!this->isEnd() && isIdentifierPart(m_source[m_pos]))) {
                return Err(LexingError{
                    .kind = LexingError::Kind::InvalidNumber,
                    .line = line,
                    .column = col,
                });
            }

            return Ok(Token::Create(TokenType::INTEGER, m_source.substr(start, m_pos - start), line, col));
        }

        bool isFloat = false;

        while (!this->isEnd()) {
            char c = m_source[m_pos];
            if (isDigit(c)) {
                this->advance();
            } else if (c == '.' && !isFloat) {
                if (this->peekAt(1) == '.') {
                    break;
                }
                isFloat = true;
                this->advance();
            } else if ((c == 'e' || c == 'E') && !isFloat) {
                size_t offset = 1;
                char next = this->peekAt(offset);
                if (next == '+' || next == '-') {
                    offset++;
                    next = this->peekAt(offset);
                }

                if (isDigit(next)) {
                    isFloat = true;
                    this->advance();
                    if (m_source[m_pos] == '+' || m_source[m_pos] == '-') {
                        this->advance();
                    }
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        if (!this->isEnd() && isIdentifierPart(m_source[m_pos])) {
            return Err(LexingError{
                .kind = LexingError::Kind::InvalidNumber,
                .line = line,
                .column = col,
            });
        }

        return Ok(Token::Create(isFloat ? TokenType::FLOAT : TokenType::INTEGER, m_source.substr(start, m_pos - start), line, col));
    }

    Lexer::Result Lexer::unexpected(char c, uint32_t line, uint32_t column) noexcept {
        return Err(LexingError{
            .kind = LexingError::Kind::UnexpectedCharacter,
            .unexpectedChar = c,
            .line = line,
            .column = column,
        });
    }
}
