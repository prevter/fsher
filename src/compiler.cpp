#include <fsher/compiler.hpp>

#include <fsher/lexer.hpp>
#include <fsher/ast/parser.hpp>
#include <fsher/codegen/encoder.hpp>
#include <fsher/codegen/backend/bgfx.hpp>
#include <fsher/codegen/backend/glsl.hpp>
#include <fsher/semantic/analyzer.hpp>
#include <fsher/semantic/symbol_table.hpp>

#include <cstring>
#include <shaderc/shaderc.h>

namespace fsher {
    constexpr std::array ALL_BACKENDS = {
        TargetBackends::GLSL120,
        TargetBackends::GLSL330,
        TargetBackends::GLSL450,
        TargetBackends::ES100,
        TargetBackends::ES300,
        TargetBackends::ES320,
        TargetBackends::Vulkan
    };

    static backend::GLSLTarget toGLSLTarget(TargetBackends backend) {
        using namespace backend;
        if (backend == TargetBackends::GLSL120) return GLSLTarget::Desktop120();
        if (backend == TargetBackends::GLSL330) return GLSLTarget::Desktop330();
        if (backend == TargetBackends::GLSL450) return GLSLTarget::Desktop450();
        if (backend == TargetBackends::ES100) return GLSLTarget::ES100();
        if (backend == TargetBackends::ES300) return GLSLTarget::ES300();
        if (backend == TargetBackends::ES320) return GLSLTarget::ES320();
        if (backend == TargetBackends::Vulkan) return GLSLTarget::Vulkan();
        return GLSLTarget::Desktop330();
    }

    static std::vector<uint8_t> toBytes(std::string_view str) {
        return {str.begin(), str.end()};
    }

    static geode::Result<std::vector<uint8_t>> compileToSPIRV(std::string_view glsl, Stage stage) {
        shaderc_compiler_t compiler = shaderc_compiler_initialize();
        shaderc_compile_options_t opts = shaderc_compile_options_initialize();
        shaderc_compile_options_set_target_spirv(opts, shaderc_spirv_version_1_0);
        shaderc_compile_options_set_target_env(opts, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);

        shaderc_shader_kind kind = stage == Stage::Vertex ? shaderc_vertex_shader : shaderc_fragment_shader;

        auto result = shaderc_compile_into_spv(
            compiler, glsl.data(), glsl.size(),
            kind, "fsher", "main", opts
        );

        std::vector<uint8_t> out;
        if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
            std::string err = shaderc_result_get_error_message(result);

            fmt::println("=== GLSL Source ===\n{}\n=== Compilation Error ===\n{}", glsl, err);

            shaderc_result_release(result);
            shaderc_compile_options_release(opts);
            shaderc_compiler_release(compiler);
            return geode::Err(std::move(err));
        }

        auto* bytes = shaderc_result_get_bytes(result);
        auto len = shaderc_result_get_length(result);
        out.assign(bytes, bytes + len);

        shaderc_result_release(result);
        shaderc_compile_options_release(opts);
        shaderc_compiler_release(compiler);

        return geode::Ok(std::move(out));
    }

    CompileResult compile(std::string_view source, CompileRequest request) {
        CompileResult result;

        Lexer lexer(source);
        Parser parser(lexer);

        auto program = parser.parseProgram();
        if (program.isErr()) {
            auto const& err = program.unwrapErr();
            CompileError ce;
            ce.range = {};

            if (err.kind == ParsingError::Kind::LexingError) {
                ce.phase = CompilePhase::Lexing;
                ce.message = fmt::format("lexing error at line {}, column {}", err.lexingError.line, err.lexingError.column);
                ce.range.start = {err.lexingError.line, err.lexingError.column};
            } else {
                ce.phase = CompilePhase::Parsing;
                ce.message = std::string(err.message);
                ce.range.start = err.token.location;
            }

            result.m_errors.push_back(std::move(ce));
            return result;
        }

        SymbolTable globals;
        StructRegistry structs;
        StageRegistry stages;

        globals.registerBuiltIns();
        auto analysisErrors = SymbolCollector(globals, structs, stages).collect(*program.unwrap());

        bool hasErrors = false;
        for (auto const& err : analysisErrors) {
            result.m_errors.push_back({
                .phase = CompilePhase::Semantic,
                .message = std::string(err.message),
                .range = err.range,
                .severity = static_cast<CompileError::Severity>(err.severity)
            });

            if (err.severity == AnalysisError::Severity::Error) {
                hasErrors = true;
            }
        }

        if (hasErrors) {
            return result;
        }

        auto ir = ir::Encoder(globals, structs).processVFProgram(program.unwrap().get(), stages);

        auto style = request.minify ? backend::CodeStyle::Compact : backend::CodeStyle::Default;

        for (size_t i = 0; i < ALL_BACKENDS.size(); ++i) {
            auto target = ALL_BACKENDS[i];
            if (!has(request.targets, target)) {
                continue;
            }

            bool isVulkan = target == TargetBackends::Vulkan;

            auto glslTarget = toGLSLTarget(target);

            backend::GLSLGenerator generator(glslTarget);
            auto output = generator.generate(ir, style);
            auto const& bindingMap = generator.bindingAssignments();

            for (size_t j = 0; j < ir.stages.size(); ++j) {
                auto const& stageIr = ir.stages[j];
                auto const& src = stageIr.stage == Stage::Vertex ? output.vertex : output.fragment;

                auto bytesRes = isVulkan
                    ? compileToSPIRV(src, stageIr.stage)
                    : geode::Ok(toBytes(src));

                if (bytesRes.isErr()) {
                    result.m_errors.push_back({
                        .phase = CompilePhase::Codegen,
                        .message = fmt::format("failed to compile {} shader to SPIR-V: {}", stageIr.stage, bytesRes.unwrapErr()),
                        .range = {},
                        .severity = CompileError::Severity::Error
                    });
                    continue;
                }

                auto bytes = std::move(bytesRes).unwrap();
                if (request.bgfx) {
                    auto byteSpan = std::span<uint8_t const>(bytes.data(), bytes.size());
                    if (!isVulkan) {
                        // strip the #version line
                        auto pos = std::ranges::find(byteSpan, '\n');
                        if (pos != byteSpan.end()) {
                            byteSpan = std::span(pos, byteSpan.end());
                        }
                    }

                    bytes = backend::bgfx::encodeStage(stageIr, byteSpan, isVulkan, bindingMap);
                }

                result.m_artifacts.stages.push_back({
                    .stage = stageIr.stage,
                    .backend = target,
                    .bytes = std::move(bytes),
                });
            }
        }

        if (result.m_artifacts.stages.empty() && result.m_errors.empty()) {
            result.m_errors.push_back({
                .phase = CompilePhase::Codegen,
                .message = "no artifacts generated for the requested targets",
                .range = {},
                .severity = CompileError::Severity::Error
            });
        }

        result.m_success = !std::ranges::any_of(result.m_errors, [](CompileError const& e) {
            return e.severity == CompileError::Severity::Error;
        });

        return result;
    }
}
