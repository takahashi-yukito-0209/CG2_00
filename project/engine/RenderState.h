#pragma once

namespace MyEngine {
// ブレンドモードの列挙型
enum class BlendMode {
    None = 0,
    Alpha, // SrcAlpha, InvSrcAlpha
    Add, // Additive
    Subtract, // Subtractive
    Multiply, // Multiply
    Screen, // Screen
    Count
};

} // namespace MyEngine
