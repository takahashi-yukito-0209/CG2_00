#pragma once

namespace MyEngine {
// 前方宣言: MyEngine 名前空間内の DirectXCommon を宣言
class DirectXCommon;

/// <summary>
/// 3Dモデル描画に共通の処理やリソースをまとめるクラス
/// </summary>
class ModelCommon {
public: // メンバ関数
    ModelCommon() = default; // デフォルトコンストラクタ

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize(DirectXCommon* dxCommon) { dxCommon_ = dxCommon; }

    /// <summary>
    /// DirectXCommonへの参照を取得
    /// </summary>
    DirectXCommon* GetDxCommon() const { return dxCommon_; }

private: // メンバ変数
    DirectXCommon* dxCommon_ = nullptr; // DirectXCommonへの参照（外部で管理される）
};

} // namespace MyEngine
