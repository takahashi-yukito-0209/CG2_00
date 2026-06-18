#pragma once

#include <string>
#include <vector>

namespace MyEngine {

// 前方宣言
class Object3dCommon;
class SpriteCommon;
class Camera;
class ParticleManager;
class TextureManager;
class SrvManager;
class DirectXCommon;
class Object3d;
class Sprite;
class PostProcess;

// シーンコンテキスト構造体（シーンに渡される共通リソースの集約）
struct SceneContext {
    Object3dCommon* object3dCommon = nullptr;
    SpriteCommon* spriteCommon = nullptr;
    Camera* camera = nullptr;
    ParticleManager* particleManager = nullptr;
    TextureManager* textureManager = nullptr;
    SrvManager* srvManager = nullptr;
    DirectXCommon* directXCommon = nullptr;
    // ImGuiManagerへのポインタ（シーンが必要に応じてImGui描画用のフックを提供するために使用）
    class ImGuiManager* imguiManager = nullptr;
    // 描画タイプ（Game の ImGui で選択された描画モードを渡すため）
    // 0=Model,1=Particle,2=Sprite,3=Bunny,4=Fence,5=Checker,6=Sphere,7=All
    int selectedDrawType = -1;
};

/// <summary>
/// シーンのインターフェースクラス
/// </summary>
class IScene {
public: // メンバ関数

    /// <summary>
    /// 仮想デストラクタ（インターフェースクラスには必須）
    /// </summary>
    virtual ~IScene() {}

    /// <summary>
    /// 初期化処理
    /// </summary>
    virtual void Initialize(const SceneContext& ctx) = 0;

    /// <summary>
    /// 終了処理
    /// </summary>
    virtual void Finalize() = 0;

    /// <summary>
    /// 更新処理（引数は前のフレームからの経過時間）
    /// </summary>
    virtual void Update(float dt) = 0; 

    /// <summary>
    /// 描画処理
    /// </summary>
    virtual void Draw() = 0; 

    /// <summary>
    /// シーンに入るときの処理（オプション、必要に応じてオーバーライド）
    /// </summary>
    virtual void OnEnter() {}

    /// <summary>
    /// シーンから出るときの処理（オプション、必要に応じてオーバーライド）
    /// </summary>
    virtual void OnExit() {}

    /// <summary>
    /// シーンの名前を取得（オプション、必要に応じてオーバーライド）
    /// </summary>
    virtual std::string GetName() const { return std::string(); }

    /// <summary>
    /// 外部から描画モードの更新を通知するためのフック（デフォルトは何もしない）
    /// </summary>
    virtual void SetSelectedDrawType(int) {}

    /// <summary>
    /// シーンが所有するオブジェクトポインタ群を ImGui に渡すために埋めるフック
    /// デフォルト実装は何もしない
    /// </summary>
    virtual void FillObject3dPointers(std::vector<class Object3d*>* out) {}

    /// <summary>
    /// シーンが所有するスプライトポインタ群を ImGui に渡すために埋めるフック
    /// デフォルト実装は何もしない
    /// </summary>
    virtual void FillSpritePointers(std::vector<class Sprite*>* out) {}

    /// <summary>
    /// シーンが使用しているポストプロセスを取得する
    /// </summary>
    virtual PostProcess* GetPostProcess() { return nullptr; }

    /// <summary>
    /// ウィンドウリサイズ通知: シーン固有のリサイズ処理が必要な場合にオーバーライドする
    /// </summary>
    virtual void OnWindowResize(uint32_t /*width*/, uint32_t /*height*/) {}

};

} // namespace MyEngine
