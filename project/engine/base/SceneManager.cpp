#include "SceneManager.h"
#include "../utility/Logger.h"
#include <utility>

namespace MyEngine {

/// <summary>
/// デストラクタ: 現在のシーンがあればFinalizeを呼び出してクリーンアップする
/// </summary>
SceneManager::~SceneManager()
{
    Finalize();
}

/// <summary>
/// ウィンドウリサイズをシーンへ伝播する
/// </summary>
void SceneManager::OnWindowResize(uint32_t width, uint32_t height)
{
    // 必要ならシーンコンテキスト中の値を更新しておく
    // (SceneContext 自体には幅/高さのフィールドがないため、各シーンがカメラ等を参照している想定)
    if (current_) {
        try {
            current_->OnWindowResize(width, height);
        } catch (...) {
            Logger::Log("SceneManager::OnWindowResize: exception in current_->OnWindowResize\n");
        }
    }
}

/// <summary>
/// シーンマネージャの初期化（必要なリソースのセットアップなどを行う）。現状は特に処理なし。
/// </summary>
void SceneManager::Initialize()
{
}

/// <summary>
/// シーンコンテキストの設定（シーンに共通のリソースや状態を渡すための関数）
/// </summary>
void SceneManager::SetContext(const SceneContext& ctx)
{
    ctx_ = ctx;
}

/// <summary>
/// シーンマネージャの終了処理
/// </summary>
void SceneManager::Finalize()
{
    // 現在のシーンがあればFinalizeを呼び出してクリーンアップする
    if (current_) {
        // 現在のシーンから退出処理を行い、Finalizeして破棄する
        current_->OnExit();
        current_->Finalize();
        current_.reset();
    }

    // スタックに残っているシーンもすべてFinalizeして破棄する
    for (auto& sc : stack_) {
        if (sc) {
            sc->OnExit();
            sc->Finalize();
        }
    }
    stack_.clear();
}

/// <summary>
/// 更新処理（引数は前のフレームからの経過時間）。現在のシーンのUpdateを呼び出す。
/// </summary>
void SceneManager::Update(float dt)
{
    // 現在のシーンがあればUpdateを呼び出す
    if (current_) {
        current_->Update(dt);
    }
}

/// <summary>
/// 描画処理。現在のシーンのDrawを呼び出す。
/// </summary>
void SceneManager::Draw()
{
    // 現在のシーンがあればDrawを呼び出す
    if (current_) {
        current_->Draw();
    }
}

/// <summary>
/// 描画モードをシーンコンテキストに設定する
/// </summary>
void SceneManager::SetSelectedDrawType(int t)
{
    ctx_.selectedDrawType = t;
    // 現在のシーンがあれば、描画タイプの更新を通知するためのフックを呼び出す
    if (current_) {
        current_->SetSelectedDrawType(t);
    }
}

/// <summary>
/// シーンの切り替え。現在のシーンがあればFinalizeを呼び出してクリーンアップし、新しいシーンをセットしてInitializeを呼び出す。
/// </summary>
void SceneManager::ChangeScene(std::unique_ptr<IScene> newScene)
{
    // 防御: 同じシーン名への切替は無視する
    if (current_ && newScene) {
        try {
            const std::string cur = current_->GetName();
            const std::string nxt = newScene->GetName();
            if (cur == nxt) {
                char buf[256];
                sprintf_s(buf, "SceneManager::ChangeScene: request to change to same scene '%s' ignored\n", cur.c_str());
                Logger::Log(buf);
                return;
            }
        } catch (...) {
            // GetName は例外を投げない想定だが、念のため保護する
        }
    }

    // 現在のシーンがあればFinalizeを呼び出してクリーンアップする
    if (current_) {
        current_->OnExit();
        current_->Finalize();
    }
    // 新しいシーンをセットする
    current_ = std::move(newScene);

    // 新しいシーンがあればInitializeを呼び出す
    if (current_) {
        current_->Initialize(ctx_);
        current_->OnEnter();
    }
}

/// <summary>
/// シーンのプッシュ。現在のシーンをスタックに保存して新しいシーンをセットし、Initializeを呼び出す。PopSceneで前のシーンに戻れるようにする。
/// </summary>
void SceneManager::PushScene(std::unique_ptr<IScene> newScene)
{
    // 引数チェック: newScene が nullptr なら何もしない
    if (!newScene) {
        return;
    }

    // 現在のシーンが存在する場合、OnExit を呼んでからスタックにムーブして保存する
    if (current_) {
        // シーンを一時停止扱いとするため Finalize は呼ばない
        current_->OnExit();
        stack_.push_back(std::move(current_));
    }

    // 新しいシーンをセットして初期化・入場処理を行う
    current_ = std::move(newScene);
    if (current_) {
        current_->Initialize(ctx_);
        current_->OnEnter();
    }
}

/// <summary>
/// シーンのポップ。スタックから前のシーンを取り出してセットし、Initializeを呼び出す。現在のシーンはFinalizeを呼び出してクリーンアップする。
/// </summary>
void SceneManager::PopScene()
{
    // 現在のシーンが存在すれば終了処理を行って破棄する
    if (current_) {
        current_->OnExit();
        current_->Finalize();
        current_.reset();
    }

    // スタックが空でなければ、最後にプッシュしたシーンを復帰させる
    if (!stack_.empty()) {
        // スタックからムーブして現在のシーンに戻す
        current_ = std::move(stack_.back());
        stack_.pop_back();

        // 復帰したシーンを初期化して入場処理を行う
        if (current_) {
            current_->Initialize(ctx_);
            current_->OnEnter();
        }
    }
}

/// <summary>
/// 現在のシーンを取得する関数。現在のシーンが存在しない場合は nullptr を返す。
/// </summary>
IScene* SceneManager::GetCurrent() const
{
    return current_.get();
}

/// <summary>
/// 現在のシーンの名前を取得する関数。現在のシーンが存在しない場合は空文字列を返す。
/// </summary>
std::string SceneManager::GetCurrentSceneName() const
{
    // 現在のシーンがあれば名前を取得して返す。存在しない場合は空文字列を返す。
    if (current_) {
        return current_->GetName();
    }

    // 現在のシーンが存在しない場合は空文字列を返す
    return std::string();
}

} // namespace MyEngine
