#pragma once

namespace MyEngine {
// ブレンドモードの列挙型
enum class BlendMode {
    None = 0,
    Alpha,    // SrcAlpha, InvSrcAlpha
    Add,      // Additive
    Multiply, // Multiply
    Screen,
    Count
};

} // namespace MyEngine
