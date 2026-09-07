#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <Geode/Result.hpp>

#include "../lexer.hpp"
#include "nodes/program.hpp"
#include "type_name.hpp"

namespace fsher {
    class IfExprNode;
    class MatchExprNode;
    class BlockNode;
    class ExprNode;
    class StmtNode;
    class LetStmtNode;
    class ConstStmtNode;
    class AssignStmtNode;
    class ForStmtNode;
    class ReturnStmtNode;
    class ConstDeclNode;
    class FnDeclNode;
    class StructDeclNode;
    class GlobalDeclNode;
    struct StructField;

    struct ParsingError {
        std::string_view message;
        enum class Kind : uint8_t {
            UnexpectedToken = 1,
            UnexpectedEndOfFile = 2,
            LexingError = 3,
        } kind;
        LexingError lexingError;
        Token token;
    };

    class Parser {
    public:
        template <typename T>
        using Result = geode::Result<std::unique_ptr<T>, ParsingError>;

        explicit Parser(Lexer& lexer) noexcept;

        Result<ProgramNode> parseProgram();

    private:
        geode::Result<void, ParsingError> advance() noexcept;
        [[nodiscard]] bool check(TokenType type) const noexcept;
        bool match(TokenType type) noexcept;

        Result<ItemNode> parseItem();

        geode::Result<std::vector<Attribute>, ParsingError> parseAttributes();
        geode::Result<std::vector<StructField>, ParsingError> parseStructFields();
        geode::Result<TypeName, ParsingError> parseType();

        Result<GlobalDeclNode> parseGlobalDecl(std::vector<Attribute> attrs);
        Result<ConstDeclNode> parseConstDecl(std::vector<Attribute> attrs);
        Result<StructDeclNode> parseStructDecl(std::vector<Attribute> attrs);
        Result<FnDeclNode> parseFnDecl(std::vector<Attribute> attrs);

        Result<BlockNode> parseBlock();
        Result<LetStmtNode> parseLetStmt();
        Result<ConstStmtNode> parseConstStmt();
        Result<ForStmtNode> parseForStmt();
        Result<ReturnStmtNode> parseReturnStmt();
        Result<ExprNode> parseExpr(int precedence = 0, bool allowStructLiterals = true);
        Result<ExprNode> parsePrefixOrPrimary(bool allowStructLiterals);

        Result<IfExprNode> parseIfExpr();
        Result<MatchExprNode> parseMatchExpr();

        geode::Result<Token, ParsingError> expect(TokenType type, std::string_view message) noexcept;
        geode::impl::ErrContainer<ParsingError> error(std::string_view message) const noexcept;

    private:
        Lexer& m_lexer;
        Token m_current;
        Token m_previous;
    };
}
