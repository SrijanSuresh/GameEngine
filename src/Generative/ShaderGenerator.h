#pragma once

// -----------------------------------------------------------------------------
// ShaderGenerator.h
//
// The generative tech-art pipeline — the core unique feature of NovaEngine.
//
// How it works:
//   1. User types a prompt like "fire galaxy" into the Inspector
//   2. ShaderGenerator::Generate(prompt) parses the keywords
//   3. Each recognized keyword contributes:
//        - A color palette (vec3 values)
//        - An effect function (GLSL math: noise, fbm, domain warping, etc.)
//   4. All contributions are blended into one complete GLSL fragment shader
//   5. The shader is compiled at runtime via glShaderSource + glCompileShader
//   6. The entity's material updates instantly — no restart needed
//
// Adding a new keyword:
//   1. Add a new entry to the s_Keywords table in ShaderGenerator.cpp
//   2. Write a GLSL snippet that returns a vec4 color
//   3. Done — it automatically becomes combinable with all other keywords
// -----------------------------------------------------------------------------

#include <string>
#include <vector>

namespace Nova {

class ShaderGenerator {
public:
    // Takes a freeform prompt string (e.g. "fire galaxy pulsing")
    // Returns a complete GLSL 330 core fragment shader source string.
    // If no keywords are recognized, returns a default solid color shader.
    static std::string Generate(const std::string& prompt);

    // Returns the vertex shader source — same for all generated materials.
    // (Only the fragment shader changes per prompt.)
    static const char* GetVertexSource();

private:
    // Splits prompt into lowercase tokens and matches against keyword table
    static std::string BuildFragmentSource(const std::vector<std::string>& tokens);
};

} // namespace Nova