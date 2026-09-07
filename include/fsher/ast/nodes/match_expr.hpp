#pragma once
#include <memory>
#include <string_view>
#include <variant>
#include <vector>
#include <fmt/format.h>

#include "expr.hpp"

namespace fsher {
    struct Pattern {
        enum class Kind : uint8_t {
            Wildcard,
            Expression
        } kind;

        std::unique_ptr<ExprNode> expr;
    };

    struct MatchArm {
        Pattern pattern;
        std::unique_ptr<ExprNode> body;
    };

    class MatchExprNode : public ExprNode {
    public:
        MatchExprNode(
            SourceRange const& range,
            std::unique_ptr<ExprNode> scrutinee,
            std::vector<MatchArm> arms
        ) noexcept : ExprNode(range, Type::MatchExpr),
                     m_scrutinee(std::move(scrutinee)),
                     m_arms(std::move(arms)) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}MatchExprNode {{\n", "", indent);
            fmt::format_to(out, "{:>{}}Scrutinee:\n", "", indent + 1);
            m_scrutinee->debug(out, indent + 2);
            fmt::format_to(out, "{:>{}}Arms: [\n", "", indent + 1);
            for (auto const& arm : m_arms) {
                fmt::format_to(out, "{:>{}}Arm {{\n", "", indent + 2);
                switch (arm.pattern.kind) {
                    case Pattern::Kind::Wildcard:
                        fmt::format_to(out, "{:>{}}Default\n", "", indent + 3);
                        break;
                    case Pattern::Kind::Expression:
                        fmt::format_to(out, "{:>{}}Pattern:\n", "", indent + 3);
                        arm.pattern.expr->debug(out, indent + 4);
                        break;
                }
                fmt::format_to(out, "{:>{}}Body:\n", "", indent + 3);
                arm.body->debug(out, indent + 4);
                fmt::format_to(out, "{:>{}}}},\n", "", indent + 2);
            }
            fmt::format_to(out, "{:>{}}]\n", "", indent + 1);
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        [[nodiscard]] ExprNode* scrutinee() const noexcept { return m_scrutinee.get(); }
        [[nodiscard]] std::vector<MatchArm> const& arms() const noexcept { return m_arms; }

    private:
        std::unique_ptr<ExprNode> m_scrutinee;
        std::vector<MatchArm> m_arms;
    };
}