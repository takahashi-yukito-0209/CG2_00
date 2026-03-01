#pragma once
#include <Windows.h>

/// <summary>
/// アプリケーションの基本的なライフサイクルを管理するフレームワーククラス
/// </summary>
class Framework {
public: // メンバ関数

    /// <summary>
    /// コンストラクタ: メンバ変数を初期化
    /// </summary>
    Framework() = default;

    /// <summary>
    /// デストラクタ: 派生クラスのFinalize()が呼ばれるように仮想デストラクタとする
    /// </summary>
    virtual ~Framework() = default;


    /// <summary>
    /// 初期化処理 (必要なら派生でオーバーライド)
    /// </summary>
    virtual bool Initialize(HINSTANCE hInstance, int nCmdShow) { (void)hInstance; (void)nCmdShow; return true; }
    
    /// <summary>
    /// 終了処理 (必要なら派生でオーバーライド)
    /// </summary>
    virtual void Finalize() { }
    
    /// <summary>
    /// 更新処理 (必要なら派生でオーバーライド)
    /// </summary>
    virtual void Update() { }
    
    /// <summary>
    /// イベントポーリング (必要なら派生でオーバーライド)
    /// </summary>
    virtual bool PollEvents();
    
    /// <summary>
    /// 描画処理 (必須オーバーライド: 派生クラスで実装を要求)
    /// </summary>
    virtual void Draw() = 0;
    
    /// <summary>
    /// 終了要求の参照 (必要なら派生でオーバーライド)
    /// </summary>
    virtual bool IsEndRequest() const { return endRequested_; }

    /// <summary>
    /// アプリケーションの実行: 初期化、メインループ、終了処理をまとめて行う
    /// </summary>
    int Run(HINSTANCE hInstance, int nCmdShow);

    /// <summary>
    /// 目標FPSの設定 (必要なら派生でオーバーライド)
    /// </summary>
    void SetTargetFPS(double fps) { if (fps > 0.0) targetFPS_ = fps; }

protected: // メンバ変数
    double targetFPS_ = 60.0; // 目標FPS
    // 基底による終了要求フラグ。
    // 外部で終了要求する場合は派生からこのフラグを操作しても良い
    mutable bool endRequested_ = false;
};
