#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "builtin.hpp"
#include "types.hpp"
#include "../tokens.hpp"
#include "../utils.hpp"
#include "../ast/type_name.hpp"

namespace fsher {
    struct Symbol {
        enum class Kind : uint8_t {
            In, Out, Uniform,
            Const, Local, Param,
            Function, Struct
        };

        enum class ResolutionState : uint8_t {
            Unresolved,
            Resolving,
            Resolved,
            Error
        };

        std::string_view name;
        TypeName declaredType;
        Node const* node = nullptr;
        uint32_t declOrder = 0;
        Type type;
        Kind kind;
        ResolutionState state = ResolutionState::Unresolved;
        std::optional<BuiltinID> builtinId;

        bool hasDeclaredType() const noexcept { return !declaredType.empty(); }
    };

    struct StructMember {
        std::string_view name;
        Type type;
        SourceRange range;
    };

    struct StructInfo {
        std::string_view name;
        std::vector<StructMember> members;

        [[nodiscard]] StructMember const* find(std::string_view member) const noexcept {
            for (auto const& m : members) {
                if (m.name == member) {
                    return &m;
                }
            }
            return nullptr;
        }
    };

    class SymbolTable {
    public:
        explicit SymbolTable(SymbolTable* parent = nullptr) noexcept
            : m_parent(parent) {}

        [[nodiscard]] SymbolTable* parent() const noexcept { return m_parent; }

        void registerBuiltIns() {
            auto const& registry = BuiltinRegistry::get();
            for (auto const& [name, def] : registry.builtins()) {
                this->insert(Symbol{
                    .name = std::string_view(name),
                    .declaredType = {},
                    .node = nullptr,
                    .type = {},
                    .kind = Symbol::Kind::Function,
                    .state = Symbol::ResolutionState::Resolved,
                    .builtinId = def.id,
                });
            }
        }

        bool insert(Symbol symbol) {
            symbol.declOrder = m_nextOrder++;
            auto [_, inserted] = m_symbols.emplace(std::string(symbol.name), symbol);
            return inserted;
        }

        Symbol* find(std::string_view name) {
            auto it = m_symbols.find(name);
            if (it != m_symbols.end()) {
                return &it->second;
            }
            if (m_parent) {
                return m_parent->find(name);
            }
            return nullptr;
        }

        Symbol const* find(std::string_view name) const {
            auto it = m_symbols.find(name);
            if (it != m_symbols.end()) {
                return &it->second;
            }
            if (m_parent) {
                return m_parent->find(name);
            }
            return nullptr;
        }

        std::optional<Symbol> lookup(std::string_view name) const {
            auto it = m_symbols.find(name);
            if (it != m_symbols.end()) {
                return it->second;
            }
            if (m_parent) {
                return m_parent->lookup(name);
            }
            return std::nullopt;
        }

        util::StringMap<Symbol> const& symbols() const noexcept {
            return m_symbols;
        }

    private:
        util::StringMap<Symbol> m_symbols;
        SymbolTable* m_parent = nullptr;
        uint32_t m_nextOrder = 0;
    };

    class StructRegistry {
    public:
        bool declare(StructInfo structInfo) {
            auto [_, inserted] = m_structs.emplace(
                std::string(structInfo.name),
                std::move(structInfo)
            );
            return inserted;
        }

        StructInfo const* lookup(std::string_view name) const noexcept {
            auto it = m_structs.find(name);
            if (it != m_structs.end()) {
                return &it->second;
            }
            return nullptr;
        }

        void updateMembers(std::string_view name, std::vector<StructMember> members) {
            auto it = m_structs.find(name);
            if (it != m_structs.end()) {
                it->second.members = std::move(members);
            }
        }

        util::StringMap<StructInfo> const& structs() const noexcept {
            return m_structs;
        }

    private:
        util::StringMap<StructInfo> m_structs;
    };
}