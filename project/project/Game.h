#pragma once
#include <Windows.h>
#include "Framework.h"

// ゲーム全体を管理するクラス（DirectX12 前提）
// Framework を継承し、ゲーム固有処理を実装する
class Game : public Framework {
public:
    Game();
    ~Game();

    // 初期化（Win 起動パラメータを受け取る）
    // 基底の初期化を先に呼び出し、派生側の初期化を行う
    bool Initialize(HINSTANCE hInstance, int nCmdShow) override;
    // 毎フレーム更新
    // 基底の更新処理を呼び出した上でゲーム固有の更新を行う
    void Update() override;
    // 毎フレーム描画
    // 描画は必須実装 (Framework 側では純粋仮想)
    void Draw() override;
    // 終了処理
    // 終了処理は派生側の解放処理を行った後で基底の Finalize を呼ぶ
    void Finalize() override;

    // ゲーム終了要求を外部から参照する
    bool IsEndRequest() const override;
    // イベントポーリングを Framework に接続する
    bool PollEvents() override;

private:
    struct Impl;
    Impl* impl_ = nullptr; // 実装の隠蔽
};
