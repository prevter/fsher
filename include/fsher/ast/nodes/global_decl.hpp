#pragma once
#include <cstdint>
#include <variant>
#include <fmt/ranges.h>
#include <Geode/Result.hpp>

#include "item.hpp"
#include "struct_decl.hpp"
#include "../type_name.hpp"

namespace fsher {
    enum class DeclKind : uint8_t { In, Out, Uniform };

    inline std::string_view format_as(DeclKind kind) noexcept {
        switch (kind) {
            case DeclKind::In: return "In";
            case DeclKind::Out: return "Out";
            case DeclKind::Uniform: return "Uniform";
            default: return "Unknown";
        }
    }

    class GlobalDeclNode : public ItemNode {
    public:
        struct Single {
            std::string_view name;
            TypeName type;
        };

        struct Group {
            std::vector<StructField> fields;
        };

        using BindTarget = std::variant<Single, Group>;

        GlobalDeclNode(
            SourceRange const& range,
            std::vector<Attribute> attributes,
            DeclKind declKind,
            BindTarget bindTarget
        ) noexcept
            : ItemNode(range, Type::GlobalDecl, std::move(attributes)),
              m_bindTarget(std::move(bindTarget)),
              m_declKind(declKind) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}GlobalDeclNode {{\n", "", indent);
            fmt::format_to(out, "{:>{}}DeclKind: {},\n", "", indent + 1, m_declKind);
            std::visit([&]<typename T0>(T0 const& target) {
                using T = std::decay_t<T0>;
                if constexpr (std::is_same_v<T, Single>) {
                    fmt::format_to(out, "{:>{}}Single {{ name: '{}', type: '{}' }},\n", "", indent + 1, target.name, target.type);
                } else if constexpr (std::is_same_v<T, Group>) {
                    fmt::format_to(out, "{:>{}}Group {{ fields: [\n", "", indent + 1);
                    for (auto const& field : target.fields) {
                        fmt::format_to(out, "{:>{}}{},\n", "", indent + 2, field);
                    }
                    fmt::format_to(out, "{:>{}}] }},\n", "", indent + 1);
                }
            }, m_bindTarget);
            if (!this->attributes().empty()) {
                fmt::format_to(out, "{:>{}}Attributes: [ {} ]\n", "", indent + 1, fmt::join(this->attributes(), ", "));
            }
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        [[nodiscard]] DeclKind declKind() const noexcept { return m_declKind; }
        [[nodiscard]] BindTarget const& bindTarget() const noexcept { return m_bindTarget; }

    private:
        BindTarget m_bindTarget;
        DeclKind m_declKind;
    };
}
