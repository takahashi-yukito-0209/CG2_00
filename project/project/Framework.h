#pragma once
#include <Windows.h>

// フレームワークの抽象基底クラス
// - ライフサイクル(初期化・ゲームループ・終了)を一括管理する
// - ゲーム固有の処理は派生クラスでオーバーライドする
class Framework {
public:
    Framework() = default;
    // 仮想デストラクタ: ポリモーフィズムで安全に破棄するために必ず宣言する
    virtual ~Framework() = default;

    // 初期化 (必要なら派生でオーバーライド)
    virtual bool Initialize(HINSTANCE hInstance, int nCmdShow) { (void)hInstance; (void)nCmdShow; return true; }
    // 終了処理 (必要なら派生でオーバーライド)
    virtual void Finalize() { }
    // 毎フレーム更新 (必要なら派生でオーバーライド)
    virtual void Update() { }
    // ウィンドウメッセージ等のポーリング処理を行うフック
    // 戻り値: 続行可能なら true, 終了要求/エラーがあれば false
    // 基底実装は Windows メッセージをポーリングし、WM_QUIT を検出すると
    // フラグを立てて false を返す
    virtual bool PollEvents();
    // 描画は派生で必須実装とするため純粋仮想にする
    virtual void Draw() = 0;
    // 終了要求の問い合わせ (派生クラスの状態を返す実装を期待)
    // 基底は内部フラグを返す
    virtual bool IsEndRequest() const { return endRequested_; }

    // Run: 共通のライフサイクル実装
    // - Initialize -> ゲームループ(Update/Draw) -> Finalize
    int Run(HINSTANCE hInstance, int nCmdShow);

    // フレームレート目標設定 (デフォルト 60fps)
    void SetTargetFPS(double fps) { if (fps > 0.0) targetFPS_ = fps; }

protected:
    double targetFPS_ = 60.0; // 目標FPS
    // 基底による終了要求フラグ。
    // 外部で終了要求する場合は派生からこのフラグを操作しても良い
    mutable bool endRequested_ = false;
};
