#pragma once
#include <fsher/ast/nodes/call.hpp>
#include <fsher/ast/nodes/if_expr.hpp>

#include "analyzer.hpp"
#include "symbol_table.hpp"

namespace fsher {
    class LiteralExprNode;
    class IdentifierExprNode;
    class BlockNode;
    class BinaryExprNode;
    class UnaryExprNode;
    class FieldExprNode;
    class IndexExprNode;
    class TernaryExprNode;
    class MatchExprNode;
    class StructLiteralExprNode;

    class StmtNode;
    class LetStmtNode;
    class ConstStmtNode;
    class AssignStmtNode;
    class ForStmtNode;
    class ReturnStmtNode;
    class ExprStmtNode;

    class TypeVisitor {
    public:
        TypeVisitor(StructRegistry& structs, SymbolTable& symbols, std::vector<AnalysisError>& errors) noexcept;

        Type visitIdentifier(IdentifierExprNode const& identifier);
        Type visitConstDecl(ConstDeclNode const* decl);
        Type visitFuncDecl(FnDeclNode const& decl);
        Type visitBlock(BlockNode const& block);
        Type visitIfExpr(IfExprNode const& expr);
        Type visitCallExpr(CallExprNode const& call);
        Type visitBinaryExpr(BinaryExprNode const& call);
        Type visitUnaryExpr(UnaryExprNode const& expr);
        Type visitFieldExpr(FieldExprNode const& expr);
        Type visitIndexExpr(IndexExprNode const& expr);
        Type visitTernaryExpr(TernaryExprNode const& expr);
        Type visitMatchExpr(MatchExprNode const& expr);
        Type visitStructLiteralExpr(StructLiteralExprNode const& expr);

        void visitStmt(StmtNode const& stmt);
        void visitLetStmt(LetStmtNode const& stmt);
        void visitConstStmt(ConstStmtNode const& stmt);
        void visitAssignStmt(AssignStmtNode const& stmt);
        void visitForStmt(ForStmtNode const& stmt);
        void visitReturnStmt(ReturnStmtNode const& stmt);
        void visitExprStmt(ExprStmtNode const& stmt);

        Type deduceTypes(Node const* node);

    private:
        enum class CoercionResult { Ok, Warn, Error };

        struct Coercion {
            CoercionResult result;
            std::string message;
        };

        static Coercion checkLiteralCoercion(LiteralExprNode const& literal, Type const& target);

        std::optional<Type> tryCoerceLiteral(
            Node const* valueExpr,
            Type const& valueType,
            Type const& targetType
        ) const;

        std::optional<Type> resolveType(std::string_view name, SourceRange const& range);
        std::optional<Type> resolveType(TypeName const& type, SourceRange const& range);

    private:
        StructRegistry& m_structs;
        SymbolTable& m_globals;
        SymbolTable* m_currentScope;
        std::vector<AnalysisError>& m_errors;
        TypeResolver m_resolver;
        std::optional<Type> m_currentFnReturnType;
    };
}
