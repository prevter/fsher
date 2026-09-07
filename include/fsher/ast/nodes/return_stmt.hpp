#pragma once
#include <memory>
#include <fmt/format.h>

#include "expr.hpp"
#include "stmt.hpp"

namespace fsher {
    class ReturnStmtNode : public StmtNode {
    public:
        ReturnStmtNode(
            SourceRange const& range,
            std::unique_ptr<ExprNode> value
        ) noexcept : StmtNode(range, Type::ReturnStmt),
                     m_value(std::move(value)) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}ReturnStmtNode {{\n", "", indent);
            if (m_value) {
                fmt::format_to(out, "{:>{}}Value:\n", "", indent + 1);
                m_value->debug(out, indent + 2);
            }
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        [[nodiscard]] ExprNode* value() const noexcept { return m_value.get(); }

    private:
        std::unique_ptr<ExprNode> m_value;
    };
}
