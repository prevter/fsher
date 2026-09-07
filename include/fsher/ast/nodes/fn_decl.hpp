#pragma once
#include <cstdint>
#include <fmt/ranges.h>

#include "expr.hpp"
#include "item.hpp"
#include "struct_decl.hpp"
#include "../type_name.hpp"

namespace fsher {
    class FnDeclNode : public ItemNode {
    public:
        FnDeclNode(
            SourceRange const& range,
            std::vector<Attribute> attributes,
            std::string_view name,
            TypeName returnType,
            std::unique_ptr<ExprNode> body,
            std::vector<StructField> parameters,
            SourceLocation argsEnd
        ) noexcept : ItemNode(range, Type::FnDecl, std::move(attributes)),
                     m_body(std::move(body)),
                     m_parameters(std::move(parameters)),
                     m_name(name),
                     m_returnType(std::move(returnType)),
                     m_argsEnd(argsEnd) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}FnDeclNode {{\n", "", indent);
            fmt::format_to(out, "{:>{}}Name: '{}',\n", "", indent + 1, m_name);
            fmt::format_to(out, "{:>{}}ReturnType: '{}',\n", "", indent + 1, m_returnType);
            fmt::format_to(out, "{:>{}}Parameters: [\n", "", indent + 1);
            for (auto const& parameter : m_parameters) {
                fmt::format_to(out, "{:>{}}{},\n", "", indent + 2, parameter);
            }
            fmt::format_to(out, "{:>{}}],\n", "", indent + 1);
            if (m_body) {
                fmt::format_to(out, "{:>{}}Body:", "", indent + 1);
                m_body->debug(out, indent + 1);
            }
            if (!this->attributes().empty()) {
                fmt::format_to(out, "{:>{}}Attributes: [ {} ]\n", "", indent + 1, fmt::join(this->attributes(), ", "));
            }
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        [[nodiscard]] std::string_view name() const noexcept { return m_name; }
        [[nodiscard]] TypeName const& returnType() const noexcept { return m_returnType; }
        [[nodiscard]] SourceLocation paramsEndLoc() const noexcept { return m_argsEnd; }

        [[nodiscard]] std::vector<StructField> const& parameters() const noexcept { return m_parameters; }
        [[nodiscard]] ExprNode const* body() const noexcept { return m_body.get(); }

    private:
        std::unique_ptr<ExprNode> m_body;
        std::vector<StructField> m_parameters;
        std::string_view m_name;
        TypeName m_returnType;
        SourceLocation m_argsEnd;
    };
}
