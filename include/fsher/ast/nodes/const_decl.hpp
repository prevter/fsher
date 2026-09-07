#pragma once
#include <cstdint>
#include <fmt/ranges.h>

#include "expr.hpp"
#include "item.hpp"
#include "../type_name.hpp"

namespace fsher {
    class ConstDeclNode : public ItemNode {
    public:
        ConstDeclNode(
            SourceRange const& range,
            std::vector<Attribute> attributes,
            Token const& name,
            TypeName type,
            std::unique_ptr<ExprNode> value
        ) noexcept : ItemNode(range, Type::ConstDecl, std::move(attributes)),
                     m_identifier(name),
                     m_name(name.lexeme),
                     m_declType(std::move(type)),
                     m_value(std::move(value)) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}ConstDeclNode {{\n", "", indent);
            fmt::format_to(out, "{:>{}}Name: '{}',\n", "", indent + 1, m_name);
            fmt::format_to(out, "{:>{}}Type: '{}',\n", "", indent + 1, m_declType);
            if (m_value) {
                fmt::format_to(out, "{:>{}}Value:", "", indent + 1);
                m_value->debug(out, indent + 1);
            }
            if (!this->attributes().empty()) {
                fmt::format_to(out, "{:>{}}Attributes: [ {} ]\n", "", indent + 1, fmt::join(this->attributes(), ", "));
            }
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        std::string_view name() const noexcept { return m_name; }
        TypeName const& declType() const noexcept { return m_declType; }

        Token const& identifier() const noexcept { return m_identifier; }

        ExprNode* value() const noexcept { return m_value.get(); }

    private:
        Token m_identifier;
        std::string_view m_name;
        TypeName m_declType;
        std::unique_ptr<ExprNode> m_value;
    };
}