#pragma once
#include "Framework.h"
#include <Windows.h>
#include <memory>

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
    /// <summary>
    /// クラッシュダンプ出力を設定する
    /// </summary>
    void SetupCrashDumpHandler();

    /// <summary>
    /// ログファイル出力を設定する
    /// </summary>
    void SetupLogFile();

    /// <summary>
    /// ウィンドウと入力を初期化する
    /// </summary>
    bool InitializeWindowAndInput(HINSTANCE hInstance, int nCmdShow);

    /// <summary>
    /// エンジン共通リソースを初期化する
    /// </summary>
    bool InitializeEngineResources(HINSTANCE hInstance);

    /// <summary>
    /// カメラとライトを初期化する
    /// </summary>
    void InitializeCameraAndLighting();

    /// <summary>
    /// デバッグ機能、ImGui、サウンドを初期化する
    /// </summary>
    void InitializeDebugToolsAndSound();

    /// <summary>
    /// リサイズ通知を設定する
    /// </summary>
    void SetupResizeCallbacks();

    /// <summary>
    /// シーン管理を初期化する
    /// </summary>
    void InitializeScene();

    struct Impl;
    std::unique_ptr<Impl> impl_ = nullptr; // 実装の隠蔽
};
