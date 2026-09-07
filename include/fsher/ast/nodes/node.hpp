#pragma once
#include <cstdint>
#include <fmt/format.h>

#include "../../tokens.hpp"

namespace fsher {
    class Node {
    public:
        enum class Type : uint8_t {
            // Top level
            Program,
            GlobalDecl,
            ConstDecl,
            StructDecl,
            FnDecl,

            // Expressions
            LiteralExpr,
            IdentifierExpr,
            UnaryExpr,
            BinaryExpr,
            TernaryExpr,
            CallExpr,
            FieldExpr,
            IndexExpr,
            StructLiteralExpr,
            Block,
            IfExpr,
            MatchExpr,

            // Statements
            LetStmt,
            ConstStmt,
            AssignStmt,
            ForStmt,
            ReturnStmt,
            ExprStmt,
        };

        static std::string_view format_as(Type type) {
            switch (type) {
                case Type::Program: return "Program";
                case Type::GlobalDecl: return "GlobalDecl";
                case Type::ConstDecl: return "ConstDecl";
                case Type::StructDecl: return "StructDecl";
                case Type::FnDecl: return "FnDecl";
                case Type::LiteralExpr: return "LiteralExpr";
                case Type::IdentifierExpr: return "IdentifierExpr";
                case Type::UnaryExpr: return "UnaryExpr";
                case Type::BinaryExpr: return "BinaryExpr";
                case Type::TernaryExpr: return "TernaryExpr";
                case Type::CallExpr: return "CallExpr";
                case Type::FieldExpr: return "FieldExpr";
                case Type::IndexExpr: return "IndexExpr";
                case Type::StructLiteralExpr: return "StructLiteralExpr";
                case Type::Block: return "Block";
                case Type::IfExpr: return "IfExpr";
                case Type::MatchExpr: return "MatchExpr";
                case Type::LetStmt: return "LetStmt";
                case Type::ConstStmt: return "ConstStmt";
                case Type::AssignStmt: return "AssignStmt";
                case Type::ForStmt: return "ForStmt";
                case Type::ReturnStmt: return "ReturnStmt";
                case Type::ExprStmt: return "ExprStmt";
                default: return "Unknown";
            }
        }

        Node(SourceRange const& range, Type type) noexcept
            : m_range(range), m_type(type) {}

        Node(Node const&) = delete;
        Node(Node&&) = delete;

        virtual ~Node() = default;
        virtual void debug(fmt::appender out, int indent = 0) const = 0;

        [[nodiscard]] Type type() const noexcept { return m_type; }
        [[nodiscard]] SourceRange const& range() const noexcept { return m_range; }

    private:
        SourceRange m_range;
        Type m_type = Type::Program;
    };
}

template <typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_base_of_v<fsher::Node, T>, char>> : formatter<std::string_view> {
    auto format(fsher::Node const& obj, format_context& ctx) const noexcept {
        obj.debug(ctx.out());
        return ctx.out();
    }
};