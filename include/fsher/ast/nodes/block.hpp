#pragma once
#include <memory>
#include <vector>
#include <fmt/format.h>

#include "expr.hpp"
#include "stmt.hpp"

namespace fsher {
    class BlockNode : public ExprNode {
    public:
        BlockNode(
            SourceRange const& range,
            std::vector<std::unique_ptr<StmtNode>> statements,
            std::unique_ptr<ExprNode> trailingExpr
        ) noexcept : ExprNode(range, Type::Block),
                     m_statements(std::move(statements)),
                     m_trailingExpr(std::move(trailingExpr)) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}BlockNode {{\n", "", indent);
            for (auto const& stmt : m_statements) {
                stmt->debug(out, indent + 1);
            }
            if (m_trailingExpr) {
                fmt::format_to(out, "{:>{}}TrailingExpr:\n", "", indent + 1);
                m_trailingExpr->debug(out, indent + 2);
            }
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        std::vector<std::unique_ptr<StmtNode>> const& statements() const noexcept { return m_statements; }
        ExprNode* trail() const noexcept { return m_trailingExpr.get(); }

    private:
        std::vector<std::unique_ptr<StmtNode>> m_statements;
        std::unique_ptr<ExprNode> m_trailingExpr;
    };
}