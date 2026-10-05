#pragma once

namespace MyEngine {
// ブレンドモードの列挙型
enum class BlendMode {
    None = 0,
    Alpha, // 通常のアルファ合成（SrcAlpha、InvSrcAlpha）
    Add, // 加算合成
    Subtract, // 減算合成
    Multiply, // 乗算合成
    Screen, // スクリーン合成
    Count
};

} // namespace MyEngine
