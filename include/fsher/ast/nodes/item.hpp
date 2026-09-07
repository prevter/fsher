#pragma once
#include <memory>
#include <vector>

#include "node.hpp"

namespace fsher {
    struct Attribute {
        std::string_view name;
    };

    inline std::string_view format_as(Attribute const& attr) noexcept {
        return attr.name;
    }

    class ItemNode : public Node {
    public:
        ItemNode(SourceRange const& range, Type type, std::vector<Attribute> attributes) noexcept
            : Node(range, type), m_attributes(std::move(attributes)) {}

        [[nodiscard]] std::vector<Attribute> const& attributes() const noexcept { return m_attributes; }

        [[nodiscard]] bool hasAttribute(std::string_view name) const noexcept {
            return std::ranges::find_if(
                m_attributes,
                [name](Attribute const& attr) { return attr.name == name; }
            ) != m_attributes.end();
        }

    private:
        std::vector<Attribute> m_attributes;
    };
}