#pragma once
#include <memory>
#include <fmt/format.h>

#include "expr.hpp"
#include "stmt.hpp"

namespace fsher {
    class AssignStmtNode : public StmtNode {
    public:
        AssignStmtNode(
            SourceRange const& range,
            std::unique_ptr<ExprNode> target,
            TokenType op,
            std::unique_ptr<ExprNode> value
        ) noexcept : StmtNode(range, Type::AssignStmt),
                     m_target(std::move(target)),
                     m_op(op),
                     m_value(std::move(value)) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}AssignStmtNode {{\n", "", indent);
            fmt::format_to(out, "{:>{}}Target:\n", "", indent + 1);
            m_target->debug(out, indent + 2);
            fmt::format_to(out, "{:>{}}Op: '{}',\n", "", indent + 1, fsher::format_as(m_op));
            fmt::format_to(out, "{:>{}}Value:\n", "", indent + 1);
            m_value->debug(out, indent + 2);
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        [[nodiscard]] ExprNode* target() const noexcept { return m_target.get(); }
        [[nodiscard]] TokenType op() const noexcept { return m_op; }
        [[nodiscard]] ExprNode* value() const noexcept { return m_value.get(); }

    private:
        std::unique_ptr<ExprNode> m_target;
        TokenType m_op;
        std::unique_ptr<ExprNode> m_value;
    };
}
