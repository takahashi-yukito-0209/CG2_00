#pragma once

#include <string>

namespace MyEngine {

// 前方宣言
class Object3dCommon;
class SpriteCommon;
class Camera;
class ParticleManager;
class TextureManager;
class SrvManager;
class DirectXCommon;

// シーンコンテキスト構造体（シーンに渡される共通リソースの集約）
struct SceneContext {
    Object3dCommon* object3dCommon = nullptr;
    SpriteCommon* spriteCommon = nullptr;
    Camera* camera = nullptr;
    ParticleManager* particleManager = nullptr;
    TextureManager* textureManager = nullptr;
    SrvManager* srvManager = nullptr;
    DirectXCommon* directXCommon = nullptr;
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
};

} // namespace MyEngine
