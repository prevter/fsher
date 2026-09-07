# 🐟 Fish Shading Language

Fish Shading Language (FSL) is a custom shading language designed to be much simpler to use compared to traditional
shading languages like GLSL or HLSL. It aims to reduce boilerplate and allow writing cross-platform shaders using a
single source of truth.

> [!NOTE]
> While FSL is already functional, it's still a WIP project and may include bugs or missing features.
> Documentation is also not available yet.

---

## Features

- Syntax similar to Rust, featuring if-expressions, loops, and pattern matching
- Unified shader stages (vertex, fragment) in a single file
- Automatic type inference
- Vertex stage auto-generation based on fragment stage inputs
- Compatible with bgfx ecosystem by generating blobs similar to bgfx shaderc
- Semantic analysis that can detect errors at compile time

## Target Platforms

FSL IR can be used to generate shader code for any platform. Currently, the following platforms are supported:

- **Vulkan**: Generates SPIR-V code for Vulkan applications
- **OpenGL**: Generates GLSL code for OpenGL applications, supporting from OpenGL 2.1 to OpenGL 4.6 including ES
  versions

DirectX, Metal and WebGPU support is planned for future releases.

## Example

Simplest FSL shader that outputs a solid color:

```glsl
in {
    #[position] pos: vec2,
    color: vec4,
}

uniform {
    #[mvp] modelViewProj: mat4,
}

#[fragment]
fn main() => color;
```

In this example, the `in` block defines vertex stage inputs and the `uniform` block defines uniform variables for both
stages (they get distributed based on usage).

A vertex entry is missing, but since we defined `#[position]` on the `pos` input, it will automatically figure out how
to fill it. Providing `#[mvp]` will also utilize it to transform the position.

A fragment entry is defined with `#[fragment]` and simply returns the `color` input, which gets automatically passed
from the vertex stage.

## Why?

While it's true that similar more mature projects exist (such as [slang](https://shader-slang.org/)), my ultimate goal
was to make something lightweight and also learn more about language processing and compiler design. Considering I'm
working on a personal game engine project, I needed something that could be easily integrated into my workflow.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.