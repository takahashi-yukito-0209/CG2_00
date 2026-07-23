#pragma once
#include "IScene.h"
#include <memory>
#include <string>
#include <vector>

namespace MyEngine {

/// <summary>
/// シーン管理クラス
/// </summary>
class SceneManager {
public: // メンバ関数
    /// <summary>
    /// コンストラクタ
    /// </summary>
    SceneManager() = default;

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~SceneManager();

    /// <summary>
    /// シーンマネージャの初期化（必要なリソースのセットアップなどを行う）
    /// </summary>
    void Initialize();

    /// <summary>
    /// シーンマネージャの終了処理（現在のシーンのFinalizeを呼び出すなど、クリーンアップを行う）
    /// </summary>
    void Finalize();

    /// <summary>
    /// シーンコンテキストの設定（シーンに共通のリソースや状態を渡すための関数）
    /// </summary>
    void SetContext(const SceneContext& ctx);

    /// <summary>
    /// 更新処理（引数は前のフレームからの経過時間）。現在のシーンのUpdateを呼び出す。
    /// </summary>
    void Update(float dt);

    /// <summary>
    /// 描画処理。現在のシーンのDrawを呼び出す。
    /// </summary>
    void Draw();
/// <summary>
    /// ウィンドウリサイズをシーンへ伝播する
    /// </summary>
    void OnWindowResize(uint32_t width, uint32_t height);

    /// <summary>
    /// シーンの切り替え。現在のシーンがあればFinalizeを呼び出してクリーンアップし、新しいシーンをセットしてInitializeを呼び出す。
    /// </summary>
    void ChangeScene(std::unique_ptr<IScene> newScene);

    /// <summary>
    /// 現在のシーンを初期化済みのままスタックへ退避し、新しいシーンを初期化して切り替える
    /// </summary>
    void PushScene(std::unique_ptr<IScene> newScene);

    /// <summary>
    /// 現在のシーンを終了し、スタックから初期化済みのシーンを復帰させる
    /// </summary>
    void PopScene();

    /// <summary>
    /// 現在のシーンを取得する関数。現在のシーンが存在しない場合は nullptr を返す。
    /// </summary>
    IScene* GetCurrent() const;

    /// <summary>
    /// 現在のシーンの名前を取得する関数。現在のシーンが存在しない場合は空文字列を返す。
    /// </summary>
    std::string GetCurrentSceneName() const;

private: // メンバ変数
    std::unique_ptr<IScene> current_; // 現在のシーン
    // PushSceneで退避した初期化済み・非アクティブ状態のシーン
    std::vector<std::unique_ptr<IScene>> stack_;
    SceneContext ctx_; // シーンコンテキスト
};

} // namespace MyEngine
