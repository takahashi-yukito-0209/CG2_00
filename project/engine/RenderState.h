#pragma once

namespace MyEngine {

enum class BlendMode {
    None = 0,
    Alpha,    // SrcAlpha, InvSrcAlpha
    Add,      // Additive
    Multiply, // Multiply
    Screen,
    Count
};

} // namespace MyEngine
