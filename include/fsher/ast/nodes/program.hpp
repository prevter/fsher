#pragma once
#include <memory>
#include <vector>

#include "item.hpp"

namespace fsher {
    class ProgramNode : public Node {
    public:
        ProgramNode(SourceRange const& range, std::vector<std::unique_ptr<ItemNode>> items) noexcept
            : Node(range, Type::Program), m_items(std::move(items)) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}ProgramNode {{\n", "", indent);
            for (auto const& item : m_items) {
                item->debug(out, indent + 1);
            }
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        std::vector<std::unique_ptr<ItemNode>> const& items() const noexcept { return m_items; }

    private:
        std::vector<std::unique_ptr<ItemNode>> m_items;
    };
}