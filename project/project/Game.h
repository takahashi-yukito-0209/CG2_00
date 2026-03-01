#pragma once
#include <Windows.h>
#include "Framework.h"

/// <summary>
/// ゲームクラス: Framework を継承してゲーム固有の処理を実装するクラス
/// </summary>
class Game : public Framework {
public: // メンバ関数

    // コンストラクタとデストラクタ
    Game(); // デフォルトコンストラクタを使用
    ~Game(); // デストラクタは基底の仮想デストラクタが呼ばれるようにする

    /// <summary>
    /// 初期化処理
    /// </summary>
    bool Initialize(HINSTANCE hInstance, int nCmdShow) override;

    /// <summary>
    /// 更新処理
    /// </summary>
    void Update() override;

    /// <summary>
    /// 描画処理
    /// </summary>
    void Draw() override;

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize() override;

    /// <summary>
    /// 終了要求の参照
    /// </summary>
    bool IsEndRequest() const override;

    /// <summary>
    /// イベントポーリング
    /// </summary>
    bool PollEvents() override;

private: // メンバ変数

    struct Impl;
    Impl* impl_ = nullptr; // 実装の隠蔽
};
