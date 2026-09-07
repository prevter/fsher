#pragma once
#include <memory>
#include <fmt/format.h>

#include "expr.hpp"
#include "stmt.hpp"

namespace fsher {
    class ExprStmtNode : public StmtNode {
    public:
        ExprStmtNode(std::unique_ptr<ExprNode> expr) noexcept
            : StmtNode(expr->range(), Type::ExprStmt), m_expr(std::move(expr)) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}ExprStmtNode {{\n", "", indent);
            m_expr->debug(out, indent + 1);
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        [[nodiscard]] ExprNode* expr() const noexcept { return m_expr.get(); }

    private:
        std::unique_ptr<ExprNode> m_expr;
    };
}