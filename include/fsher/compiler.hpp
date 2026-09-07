#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <Geode/Result.hpp>

#include "codegen/backend/backend_id.hpp"
#include "codegen/backend/glsl.hpp"
#include "semantic/analyzer.hpp"

namespace fsher {
    enum class TargetBackends : uint8_t {
        GLSL120 = 1,
        GLSL330 = 2,
        GLSL450 = 4,
        ES100 = 8,
        ES300 = 16,
        ES320 = 32,
        Vulkan = 64,

        Windows = Vulkan | GLSL120, // | HLSL
        Linux = Vulkan | GLSL120,
        MacOS = Vulkan, // | MSL

        Steam = Windows | Linux | MacOS,
        Web = ES300,

        Android = ES300,
        // iOS = MSL,
    };

    constexpr TargetBackends operator|(TargetBackends a, TargetBackends b) noexcept {
        return static_cast<TargetBackends>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    constexpr TargetBackends operator&(TargetBackends a, TargetBackends b) noexcept {
        return static_cast<TargetBackends>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }

    constexpr bool has(TargetBackends flags, TargetBackends bit) noexcept {
        return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(bit)) != 0;
    }

    enum class CompilePhase : uint8_t {
        Lexing,
        Parsing,
        Semantic,
        Codegen,
    };

    struct CompileError {
        CompilePhase phase;
        std::string message;
        SourceRange range;
        enum class Severity : uint8_t {
            Error, Warning, Information, Debug
        } severity = Severity::Error;
    };

    struct CompileRequest {
        TargetBackends targets = TargetBackends::GLSL330;
        bool minify = true;
        bool bgfx = false;
    };

    struct CompiledStage {
        Stage stage;
        TargetBackends backend;
        std::vector<uint8_t> bytes;

        [[nodiscard]] std::string_view source() const noexcept {
            return std::string_view(reinterpret_cast<char const*>(bytes.data()), bytes.size());
        }

        [[nodiscard]] std::span<uint32_t const> spirv() const noexcept {
            return std::span<uint32_t const>(reinterpret_cast<uint32_t const*>(bytes.data()), bytes.size() / sizeof(uint32_t));
        }
    };

    struct CompiledArtifacts {
        std::vector<CompiledStage> stages;

        [[nodiscard]] CompiledStage const* find(Stage stage, TargetBackends target) const noexcept {
            for (auto const& s : stages) {
                if (s.stage == stage && s.backend == target) return &s;
            }
            return nullptr;
        }

        [[nodiscard]] std::vector<CompiledStage const*> forStage(Stage stage) const noexcept {
            std::vector<CompiledStage const*> out;
            for (auto const& s : stages) {
                if (s.stage == stage) out.push_back(&s);
            }
            return out;
        }

        [[nodiscard]] std::vector<CompiledStage const*> forTarget(TargetBackends target) const noexcept {
            std::vector<CompiledStage const*> out;
            for (auto const& s : stages) {
                if (s.backend == target) out.push_back(&s);
            }
            return out;
        }
    };

    class CompileResult {
    public:
        [[nodiscard]] bool success() const noexcept { return m_success; }
        explicit operator bool() const noexcept { return m_success; }

        [[nodiscard]] CompiledArtifacts const& artifacts() const noexcept { return m_artifacts; }
        [[nodiscard]] std::vector<CompileError> const& errors() const noexcept { return m_errors; }

    private:
        std::vector<CompileError> m_errors;
        CompiledArtifacts m_artifacts;
        bool m_success = false;

        friend CompileResult compile(std::string_view source, CompileRequest request);
    };

    CompileResult compile(std::string_view source, CompileRequest request = {});
}

template <>
struct fmt::formatter<fsher::TargetBackends> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(fsher::TargetBackends target, format_context& ctx) const noexcept {
        std::string_view name;
        switch (target) {
            case fsher::TargetBackends::GLSL120: name = "GLSL120"; break;
            case fsher::TargetBackends::GLSL330: name = "GLSL330"; break;
            case fsher::TargetBackends::GLSL450: name = "GLSL450"; break;
            case fsher::TargetBackends::ES100: name = "ES100"; break;
            case fsher::TargetBackends::ES300: name = "ES300"; break;
            case fsher::TargetBackends::ES320: name = "ES320"; break;
            case fsher::TargetBackends::Vulkan: name = "Vulkan"; break;
            default: name = "<unknown>"; break;
        }
        return fmt::format_to(ctx.out(), "{}", name);
    }
};