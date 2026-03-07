#pragma once
#include <memory>
#include "IScene.h"
#include <vector>
#include <string>

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
    /// シーンの切り替え。現在のシーンがあればFinalizeを呼び出してクリーンアップし、新しいシーンをセットしてInitializeを呼び出す。
    /// </summary>
    void ChangeScene(std::unique_ptr<IScene> newScene);

    /// <summary>
    /// シーンのプッシュ。現在のシーンをスタックに保存して新しいシーンをセットし、Initializeを呼び出す。PopSceneで前のシーンに戻れるようにする。
    /// </summary>
    // ※ 実装詳細:
    // - 現在のシーンがあれば OnExit を呼び、スタックにムーブ保存する（Finalize は呼ばない、サスペンド扱い）。
    // - 新しいシーンを current_ にセットして Initialize/OnEnter を呼ぶ。
    void PushScene(std::unique_ptr<IScene> newScene);

    /// <summary>
    /// シーンのポップ。スタックから前のシーンを取り出してセットし、Initializeを呼び出す。現在のシーンはFinalizeを呼び出してクリーンアップする。
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
    // シーンスタック: PushScene したときに以前のシーンを保存する
    std::vector<std::unique_ptr<IScene>> stack_;
    SceneContext ctx_; // シーンコンテキスト
};

} // namespace MyEngine
