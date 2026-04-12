// -----------------------------------------------------------------------------
// ShaderGenerator.cpp
//
// Each keyword entry has:
//   - name    : the keyword string to match
//   - snippet : a GLSL function body that returns a vec4 color
//               It receives these inputs automatically:
//                 vec2  uv       — normalized [0,1] UVs
//                 float time     — seconds since engine start
//                 vec3  worldPos — fragment world position (for 3D effects)
//
// All matched snippets are blended together equally in the final shader.
// -----------------------------------------------------------------------------

#include "ShaderGenerator.h"

#include <algorithm>  // std::transform
#include <sstream>
#include <vector>
#include <string>

namespace Nova {

// -----------------------------------------------------------------------------
// Keyword table
// Each entry: { keyword, GLSL snippet that returns vec4 }
// Snippets share these uniforms: u_Time, and receive: vec2 uv
// -----------------------------------------------------------------------------

struct Keyword {
    const char* name;
    const char* snippet; // must return vec4
};

// Shared GLSL utility functions injected into every generated shader.
// These are the building blocks all snippets use.
static const char* s_SharedGLSL = R"(
// ── Shared utility functions ─────────────────────────────────────────────────

// Basic hash — pseudo-random float from a vec2
float hash(vec2 p) {
    p = fract(p * vec2(127.1, 311.7));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

// Smooth value noise
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f); // smoothstep

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// Fractal Brownian Motion — layered noise for organic complexity
// octaves = how many layers (more = more detail, more cost)
float fbm(vec2 p, int octaves) {
    float value    = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < octaves; i++) {
        value     += amplitude * noise(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

// Domain warping — distorts UV coordinates using fbm for fluid, swirling shapes
vec2 warp(vec2 p, float strength) {
    float wx = fbm(p + vec2(1.7, 9.2), 4);
    float wy = fbm(p + vec2(8.3, 2.8), 4);
    return p + strength * vec2(wx, wy);
}
)";

// ── Keyword snippets ──────────────────────────────────────────────────────────

static const Keyword s_Keywords[] = {

    { "galaxy", R"(
vec4 effect_galaxy(vec2 uv, float time) {
    // Swirling spiral arms using domain-warped fbm
    vec2  center    = uv - 0.5;
    float dist      = length(center);
    float angle     = atan(center.y, center.x);

    // Spiral: rotate UV based on distance from center
    float spiral    = angle + dist * 6.0 - time * 0.3;
    vec2  spiralUV  = vec2(cos(spiral), sin(spiral)) * dist;

    vec2  warped    = warp(spiralUV + time * 0.05, 0.4);
    float nebula    = fbm(warped * 3.0, 6);

    // Deep space color palette: dark blue → purple → bright core
    vec3 deep    = vec3(0.02, 0.01, 0.08);
    vec3 mid     = vec3(0.15, 0.05, 0.40);
    vec3 bright  = vec3(0.80, 0.60, 1.00);
    vec3 core    = vec3(1.00, 0.95, 1.00);

    vec3 color   = mix(deep, mid,   smoothstep(0.0, 0.4, nebula));
    color        = mix(color, bright, smoothstep(0.4, 0.7, nebula));
    color        = mix(color, core,   smoothstep(0.7, 1.0, nebula));

    // Bright star-like center
    float glow   = exp(-dist * 8.0);
    color       += vec3(0.6, 0.5, 1.0) * glow;

    return vec4(color, 1.0);
}
)" },

    { "fire", R"(
vec4 effect_fire(vec2 uv, float time) {
    // Fire rises upward — flip UV.y so base is hot, top fades
    vec2 fireUV = vec2(uv.x - 0.5, 1.0 - uv.y);

    // Domain warp to create flickering turbulence
    vec2 warped = warp(fireUV * 2.0 + vec2(0.0, -time * 1.2), 0.3);
    float f     = fbm(warped * 3.5, 5);

    // Fade toward top and edges
    float fade  = (1.0 - uv.y) * (1.0 - abs(uv.x - 0.5) * 2.0);
    f          *= fade;

    // Fire palette: black → dark red → orange → yellow → white hot
    vec3 black  = vec3(0.0,  0.0,  0.0);
    vec3 red    = vec3(0.8,  0.05, 0.0);
    vec3 orange = vec3(1.0,  0.4,  0.0);
    vec3 yellow = vec3(1.0,  0.9,  0.1);
    vec3 white  = vec3(1.0,  1.0,  0.9);

    vec3 color  = mix(black,  red,    smoothstep(0.0, 0.25, f));
    color       = mix(color,  orange, smoothstep(0.25, 0.5, f));
    color       = mix(color,  yellow, smoothstep(0.5, 0.75, f));
    color       = mix(color,  white,  smoothstep(0.75, 1.0, f));

    return vec4(color, 1.0);
}
)" },

    { "ocean", R"(
vec4 effect_ocean(vec2 uv, float time) {
    // Layered sine waves for water surface
    vec2 p      = uv * 4.0;
    float wave1 = sin(p.x * 2.0 + time * 1.2) * 0.5 + 0.5;
    float wave2 = sin(p.x * 3.7 - p.y * 2.0 + time * 0.8) * 0.5 + 0.5;
    float wave3 = sin(p.y * 5.0 + time * 1.5) * 0.5 + 0.5;

    vec2  warped = warp(p + time * 0.1, 0.25);
    float depth  = fbm(warped, 4);

    float surface = (wave1 * 0.4 + wave2 * 0.35 + wave3 * 0.25) * depth;

    // Ocean palette: deep navy → teal → bright foam white
    vec3 deep    = vec3(0.01, 0.05, 0.18);
    vec3 mid     = vec3(0.05, 0.30, 0.50);
    vec3 shallow = vec3(0.10, 0.65, 0.75);
    vec3 foam    = vec3(0.85, 0.95, 1.00);

    vec3 color   = mix(deep, mid,     smoothstep(0.0, 0.35, surface));
    color        = mix(color, shallow, smoothstep(0.35, 0.7, surface));
    color        = mix(color, foam,    smoothstep(0.7,  1.0, surface));

    // Specular highlight
    float spec   = pow(wave1 * wave2, 4.0);
    color       += vec3(0.8, 0.9, 1.0) * spec * 0.4;

    return vec4(color, 1.0);
}
)" },

    { "nebula", R"(
vec4 effect_nebula(vec2 uv, float time) {
    vec2 p      = (uv - 0.5) * 2.5;
    vec2 warped = warp(p + time * 0.04, 0.6);
    float f1    = fbm(warped,         6);
    float f2    = fbm(warped * 2.0 + vec2(5.2, 1.3), 5);
    float gas   = f1 * f2;

    // Nebula palette: deep void → rose → cyan → bright emission
    vec3 void_  = vec3(0.00, 0.00, 0.02);
    vec3 rose   = vec3(0.60, 0.05, 0.20);
    vec3 cyan   = vec3(0.05, 0.50, 0.70);
    vec3 emit   = vec3(0.90, 0.95, 1.00);

    vec3 color  = mix(void_, rose, smoothstep(0.0, 0.3, gas));
    color       = mix(color, cyan, smoothstep(0.3, 0.6, gas));
    color       = mix(color, emit, smoothstep(0.6, 1.0, gas));

    // Dust lanes — dark streaks through the nebula
    float dust  = smoothstep(0.45, 0.55, fbm(warped * 4.0, 3));
    color      *= 1.0 - dust * 0.6;

    return vec4(color, 1.0);
}
)" },

    { "cyberpunk", R"(
vec4 effect_cyberpunk(vec2 uv, float time) {
    // Grid lines
    vec2  grid     = fract(uv * 12.0);
    float gridLine = step(0.95, grid.x) + step(0.95, grid.y);
    gridLine       = clamp(gridLine, 0.0, 1.0);

    // Scanline flicker
    float scan     = step(0.5, fract(uv.y * 80.0 + time * 0.5)) * 0.08;

    // Neon glow pulses
    float pulse1   = 0.5 + 0.5 * sin(time * 3.0);
    float pulse2   = 0.5 + 0.5 * sin(time * 2.0 + 1.57);

    // Noise for grime / texture
    float grime    = fbm(uv * 8.0 + time * 0.02, 3) * 0.3;

    // Palette: near-black → electric magenta / cyan neon
    vec3 base      = vec3(0.02, 0.02, 0.05);
    vec3 neonPink  = vec3(1.00, 0.05, 0.60) * pulse1;
    vec3 neonCyan  = vec3(0.00, 0.90, 1.00) * pulse2;
    vec3 gridColor = mix(neonCyan, neonPink, uv.x);

    vec3 color     = base + grime;
    color         += gridLine * gridColor * 0.8;
    color         -= vec3(scan);

    return vec4(color, 1.0);
}
)" },

    { "lava", R"(
vec4 effect_lava(vec2 uv, float time) {
    vec2 p      = uv * 2.5;

    // Slow-moving domain warp simulates viscous flow
    vec2 warped = warp(p + vec2(time * 0.12, time * 0.08), 0.5);
    float f     = fbm(warped * 2.0, 6);

    // Crack pattern — dark fissures in bright molten rock
    float crack = smoothstep(0.45, 0.55, f);

    // Lava palette: black rock → dark red → bright orange → yellow white
    vec3 rock   = vec3(0.05, 0.02, 0.01);
    vec3 hot    = vec3(0.90, 0.15, 0.00);
    vec3 molten = vec3(1.00, 0.55, 0.00);
    vec3 glow   = vec3(1.00, 0.95, 0.50);

    vec3 color  = mix(rock,   hot,    smoothstep(0.0, 0.3, f));
    color       = mix(color,  molten, smoothstep(0.3, 0.6, f));
    color       = mix(color,  glow,   smoothstep(0.6, 1.0, f));

    // Dark cracks
    color      *= (1.0 - crack * 0.85);

    // Emissive pulse — lava breathes
    float breathe = 0.85 + 0.15 * sin(time * 0.8 + f * 6.0);
    color        *= breathe;

    return vec4(color, 1.0);
}
)" },

    { "aurora", R"(
vec4 effect_aurora(vec2 uv, float time) {
    // Curtains of light rippling vertically
    float band1 = sin(uv.x * 4.0  + time * 0.5) * 0.5 + 0.5;
    float band2 = sin(uv.x * 7.0  - time * 0.3) * 0.5 + 0.5;
    float band3 = sin(uv.x * 11.0 + time * 0.7) * 0.5 + 0.5;

    // Noise adds organic waviness to the curtains
    float wave  = fbm(vec2(uv.x * 2.0, time * 0.2), 3);
    float height = uv.y + wave * 0.3;

    // Curtain fade — brighter in middle bands
    float curtain = (band1 * 0.4 + band2 * 0.35 + band3 * 0.25);
    curtain      *= smoothstep(0.0, 0.3, height) * smoothstep(1.0, 0.4, height);

    // Aurora palette: black sky → emerald green → cyan → violet tips
    vec3 sky     = vec3(0.00, 0.01, 0.04);
    vec3 green   = vec3(0.05, 0.80, 0.30);
    vec3 cyan    = vec3(0.00, 0.90, 0.80);
    vec3 violet  = vec3(0.50, 0.10, 0.90);

    vec3 color   = sky;
    color       += green  * curtain * band1;
    color       += cyan   * curtain * band2 * 0.7;
    color       += violet * curtain * band3 * 0.5;

    // Stars in the background
    float star   = step(0.98, hash(floor(uv * 200.0)));
    color       += vec3(star) * 0.8;

    return vec4(color, 1.0);
}
)" },

    { "glitch", R"(
vec4 effect_glitch(vec2 uv, float time) {
    // Horizontal slice offset — core glitch effect
    float sliceY    = floor(uv.y * 20.0) / 20.0;
    float sliceRand = hash(vec2(sliceY, floor(time * 8.0)));

    // Only glitch some slices (threshold controls frequency)
    float glitchAmt = step(0.85, sliceRand) * (sliceRand - 0.85) * 6.0;
    vec2  glitchUV  = vec2(fract(uv.x + glitchAmt), uv.y);

    // RGB channel split — shift red and blue sideways
    float r = fbm(glitchUV + vec2( 0.01, 0.0), 3);
    float g = fbm(glitchUV,                    3);
    float b = fbm(glitchUV + vec2(-0.01, 0.0), 3);

    // Scanline bars
    float bar     = step(0.97, fract(uv.y * 30.0 + time * 2.0));
    float flicker = 0.9 + 0.1 * hash(vec2(time * 30.0, 0.0));

    // Glitch palette: dark green terminal / corrupted signal
    vec3 color = vec3(r * 0.3, g * 0.9, b * 0.5) * flicker;
    color      = mix(color, vec3(0.9, 0.1, 0.8), bar * 0.5); // magenta corruption bar
    color      = mix(color, vec3(1.0),            step(0.95, r * g * b)); // white noise

    return vec4(color, 1.0);
}
)" },

    { "plasma", R"(
vec4 effect_plasma(vec2 uv, float time) {
    // Classic plasma: sum of sine waves at different frequencies/angles
    float v = 0.0;
    v += sin(uv.x * 6.0 + time);
    v += sin(uv.y * 4.0 + time * 1.3);
    v += sin((uv.x + uv.y) * 5.0 + time * 0.8);

    // Circular ripple from center
    float dist = length(uv - 0.5);
    v += sin(dist * 12.0 - time * 2.0);

    // Normalize to [0, 1]
    v = v * 0.25 + 0.5;

    // Plasma palette: cycling hue using sine offset per channel
    vec3 color;
    color.r = 0.5 + 0.5 * sin(v * 6.28 + 0.0);
    color.g = 0.5 + 0.5 * sin(v * 6.28 + 2.09); // 120° offset
    color.b = 0.5 + 0.5 * sin(v * 6.28 + 4.19); // 240° offset

    return vec4(color, 1.0);
}
)" },

    { "void", R"(
vec4 effect_void(vec2 uv, float time) {
    vec2  center = uv - 0.5;
    float dist   = length(center);
    float angle  = atan(center.y, center.x);

    // Rotating dark vortex using domain warp
    vec2 vortexUV = vec2(dist, angle / 6.28 + time * 0.08);
    vec2 warped   = warp(vortexUV * 3.0, 0.5);
    float f       = fbm(warped, 6);

    // Event horizon — everything falls toward center
    float horizon = 1.0 - smoothstep(0.0, 0.45, dist);
    float accrete = fbm(warp(center * 2.0 + time * 0.05, 0.3), 5);

    // Void palette: pure black core → deep purple → sickly teal accretion disk
    vec3 black    = vec3(0.00, 0.00, 0.00);
    vec3 purple   = vec3(0.10, 0.00, 0.20);
    vec3 teal     = vec3(0.00, 0.40, 0.35);
    vec3 edge     = vec3(0.70, 0.85, 1.00);

    vec3 color    = mix(purple, teal, smoothstep(0.2, 0.5, f));
    color         = mix(color, edge, smoothstep(0.5, 0.8, accrete));
    color         = mix(color, black, horizon); // black hole swallows everything

    // Gravitational lensing ring
    float ring    = smoothstep(0.44, 0.45, dist) * smoothstep(0.50, 0.45, dist);
    color        += vec3(0.8, 0.9, 1.0) * ring * 2.0;

    return vec4(color, 1.0);
}
)" },

}; // end s_Keywords

static const int s_KeywordCount = sizeof(s_Keywords) / sizeof(s_Keywords[0]);

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

// Converts a string to lowercase in-place
static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// Splits a string into tokens by whitespace
static std::vector<std::string> Tokenize(const std::string& prompt) {
    std::vector<std::string> tokens;
    std::istringstream stream(prompt);
    std::string token;
    while (stream >> token)
        tokens.push_back(ToLower(token));
    return tokens;
}

// -----------------------------------------------------------------------------
// GetVertexSource — shared vertex shader for all generated materials
// -----------------------------------------------------------------------------

const char* ShaderGenerator::GetVertexSource() {
    return R"(
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;

out vec2 v_UV;
out vec3 v_WorldPos;
out vec3 v_Color;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

void main() {
    v_Color    = a_Color;
    v_WorldPos = vec3(u_Model * vec4(a_Position, 1.0));

    // Generate UV from position: remap [-0.5, 0.5] → [0, 1]
    v_UV = a_Position.xy + 0.5;

    gl_Position = u_Projection * u_View * u_Model * vec4(a_Position, 1.0);
}
)";
}

// -----------------------------------------------------------------------------
// Generate — main entry point
// -----------------------------------------------------------------------------

std::string ShaderGenerator::Generate(const std::string& prompt) {
    std::vector<std::string> tokens = Tokenize(prompt);
    return BuildFragmentSource(tokens);
}

// -----------------------------------------------------------------------------
// BuildFragmentSource — assembles the full GLSL fragment shader
// -----------------------------------------------------------------------------

std::string ShaderGenerator::BuildFragmentSource(const std::vector<std::string>& tokens) {
    // Find which keywords appear in the prompt
    std::vector<const Keyword*> matched;
    for (int i = 0; i < s_KeywordCount; i++) {
        for (const auto& token : tokens) {
            if (token == s_Keywords[i].name) {
                matched.push_back(&s_Keywords[i]);
                break; // don't double-add same keyword
            }
        }
    }

    std::ostringstream out;

    // ── Header ────────────────────────────────────────────────────────────────
    out << "#version 330 core\n\n";
    out << "in vec2 v_UV;\n";
    out << "in vec3 v_WorldPos;\n";
    out << "in vec3 v_Color;\n";
    out << "out vec4 FragColor;\n\n";
    out << "uniform float u_Time;\n\n";

    // ── Shared utility functions ───────────────────────────────────────────────
    out << s_SharedGLSL << "\n";

    if (matched.empty()) {
        // No keywords recognized — fallback: solid color using vertex color
        out << "void main() {\n";
        out << "    FragColor = vec4(v_Color, 1.0);\n";
        out << "}\n";
        return out.str();
    }

    // ── Effect functions ───────────────────────────────────────────────────────
    for (const auto* kw : matched) {
        out << kw->snippet << "\n";
    }

    // ── Main — blend all matched effects equally ───────────────────────────────
    out << "void main() {\n";
    out << "    vec2  uv   = v_UV;\n";
    out << "    float time = u_Time;\n";
    out << "    vec4  result = vec4(0.0);\n\n";

    float weight = 1.0f / (float)matched.size();
    for (const auto* kw : matched) {
        out << "    result += effect_" << kw->name
            << "(uv, time) * " << weight << ";\n";
    }

    out << "\n    FragColor = result;\n";
    out << "}\n";

    return out.str();
}

} // namespace Nova