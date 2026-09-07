#pragma once
#include <array>
#include <cstdint>
#include <fsher/ast/nodes/program.hpp>
#include <fsher/ast/nodes/struct_decl.hpp>

#include "symbol_table.hpp"
#include "../tokens.hpp"

namespace fsher {
    class StructDeclNode;
    class FnDeclNode;
    class ConstDeclNode;
    class GlobalDeclNode;

    struct AnalysisError {
        enum class Kind : uint8_t {
            DuplicateSymbol = 1,
            DuplicateStructField,
            UnknownType,
            MultipleGlobalAttributes,
            RecursiveStruct,
            RecursiveDeclaration,
            InvalidAttribute,
            DeduceMismatch,
            UndeclaredSymbol,
            TypeMismatch,
            InvalidOperation,
        } kind;
        std::string message;
        SourceRange range;
        enum class Severity : uint8_t {
            Error, Warning, Information, Debug
        } severity = Severity::Error;
    };

    class TypeResolver {
    public:
        explicit TypeResolver(StructRegistry const& structs) noexcept : m_structs(structs) {}

        std::optional<Type> resolve(std::string_view name) const noexcept;
        std::optional<Type> resolve(TypeName const& type) const noexcept;

    private:
        StructRegistry const& m_structs;
    };

    enum class Stage : uint8_t {
        Vertex, Fragment, /* Compute */
    };

    inline std::string_view format_as(Stage stage) {
        switch (stage) {
            case Stage::Vertex: return "vertex";
            case Stage::Fragment: return "fragment";
            default: return "unknown";
        }
    }

    class StageRegistry {
    public:
        bool declare(Stage stage, FnDeclNode const& decl) noexcept;
        [[nodiscard]] FnDeclNode const* lookup(Stage stage) const noexcept;

        void setPositionInput(std::string_view name) noexcept { m_positionInput = name; }
        [[nodiscard]] std::string_view positionInput() const noexcept { return m_positionInput; }

        void setMatrixInput(std::string_view name) noexcept { m_matrixInput = name; }
        [[nodiscard]] std::string_view matrixInput() const noexcept { return m_matrixInput; }

    private:
        std::array<FnDeclNode const*, 2> m_entries{};
        std::string_view m_positionInput;
        std::string_view m_matrixInput;
    };

    class SymbolCollector {
    public:
        SymbolCollector(SymbolTable& globals, StructRegistry& structs, StageRegistry& stages) noexcept
            : m_globals(globals), m_structs(structs), m_stages(stages), m_resolver(structs) {}

        std::vector<AnalysisError> collect(ProgramNode const& program);

    private:
        void checkStructCycles();
        void collectStructFields(StructDeclNode const& decl);
        void collectGlobalDecl(GlobalDeclNode const& decl);
        void collectConstDecl(ConstDeclNode const& decl);
        void collectFnDecl(FnDeclNode const& decl);

        void visitConstNodes(std::vector<ConstDeclNode*> const& consts);
        void visitFnNodes(std::vector<FnDeclNode*> const& funcs);

        enum class AttributeScope : uint8_t {
            Fn, Const,
            Global, GlobalField,
            Struct, StructField
        };

        bool checkAttributes(std::vector<Attribute> const& attributes, AttributeScope scope, SourceRange const& range);
        bool checkAttributesFor(ItemNode const& item, AttributeScope scope) { return this->checkAttributes(item.attributes(), scope, item.range()); }
        bool checkAttributesFor(StructField const& item, AttributeScope scope) { return this->checkAttributes(item.attributes, scope, item.range); }

        std::optional<Type> resolveOrError(TypeName const& type, SourceRange const& range);
        void trackGlobalAttributes(std::vector<Attribute> const& attributes, std::string_view name);
        void error(AnalysisError::Kind kind, std::string message, SourceRange const& range);

    private:
        SymbolTable& m_globals;
        StructRegistry& m_structs;
        StageRegistry& m_stages;
        TypeResolver m_resolver;
        std::vector<AnalysisError> m_errors;
    };
}
