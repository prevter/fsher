#pragma once
#include <memory>
#include <vector>
#include <fmt/format.h>

#include "expr.hpp"

namespace fsher {
    class CallExprNode : public ExprNode {
    public:
        CallExprNode(
            SourceRange const& range,
            std::unique_ptr<ExprNode> callee,
            std::vector<std::unique_ptr<ExprNode>> arguments
        ) noexcept : ExprNode(range, Type::CallExpr),
                     m_callee(std::move(callee)),
                     m_arguments(std::move(arguments)) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}CallExprNode {{\n", "", indent);
            fmt::format_to(out, "{:>{}}Callee:", "", indent + 1);
            m_callee->debug(out, indent + 1);
            fmt::format_to(out, "{:>{}}Arguments: [\n", "", indent + 1);
            for (auto const& arg : m_arguments) {
                arg->debug(out, indent + 2);
            }
            fmt::format_to(out, "{:>{}}]\n", "", indent + 1);
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        ExprNode* callee() const noexcept { return m_callee.get(); }
        std::vector<std::unique_ptr<ExprNode>> const& args() const noexcept { return m_arguments; }

    private:
        std::unique_ptr<ExprNode> m_callee;
        std::vector<std::unique_ptr<ExprNode>> m_arguments;
    };
}