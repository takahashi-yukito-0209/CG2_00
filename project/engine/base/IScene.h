#pragma once

#include "../utility/MathTypes.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class ParticleEmitter;

namespace MyEngine {

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
class DebugRenderer;
struct SceneContext {
    Object3dCommon* object3dCommon = nullptr; // 3D オブジェクト共通管理
    SpriteCommon* spriteCommon = nullptr; // スプライト共通管理
    Camera* camera = nullptr; // 通常描画用カメラ
    ParticleManager* particleManager = nullptr; // パーティクル管理
    TextureManager* textureManager = nullptr; // テクスチャ管理
    SrvManager* srvManager = nullptr; // SRV 管理
    DirectXCommon* directXCommon = nullptr; // DirectX 共通管理
    DebugRenderer* debugRenderer = nullptr; // デバッグ描画管理
    class ImGuiManager* imguiManager = nullptr; // ImGui 管理
    std::function<void(const std::string&)> requestSceneChange; // シーン切り替え要求の通知先
};

/// <summary>
/// シーンの共通インターフェース。
/// </summary>
class IScene {
public:
    /// <summary>
    /// 仮想デストラクタ。
    /// </summary>
    virtual ~IScene() { }

    /// <summary>
    /// シーンを初期化する。
    /// </summary>
    virtual void Initialize(const SceneContext& ctx) = 0;

    /// <summary>
    /// シーンを終了する。
    /// </summary>
    virtual void Finalize() = 0;

    /// <summary>
    /// シーンを更新する。
    /// </summary>
    virtual void Update(float dt) = 0;

    /// <summary>
    /// シーンを描画する。
    /// </summary>
    virtual void Draw() = 0;

    /// <summary>
    /// シーンへ入るときの処理を行う。
    /// </summary>
    virtual void OnEnter() { }

    /// <summary>
    /// シーンから出るときの処理を行う。
    /// </summary>
    virtual void OnExit() { }

    /// <summary>
    /// シーン名を取得する。
    /// </summary>
    virtual std::string GetName() const { return std::string(); }

    /// <summary>
    /// Scene View 用の描画だけにするかを設定する。
    /// </summary>
    virtual void SetSceneViewOnly(bool) { }

    /// <summary>
    /// シーンが保持する 3D オブジェクトのポインタを取得する。
    /// </summary>
    virtual void FillObject3dPointers(std::vector<class Object3d*>* out) { }

    /// <summary>
    /// Scene ViewのGizmoで3DオブジェクトのTransformが編集されたことを通知する。
    /// </summary>
    virtual void NotifyObjectTransformEdited(size_t /*objectIndex*/) { }

    /// <summary>
    /// Scene View画像上へシーン固有の編集表示を重ねて描画する。
    /// </summary>
    virtual void DrawSceneViewOverlay(const Math::Matrix4x4& /*viewProjectionMatrix*/, float /*imageMinX*/, float /*imageMinY*/, float /*imageWidth*/, float /*imageHeight*/) { }

    /// <summary>
    /// シーンが保持するスプライトのポインタを取得する。
    /// </summary>
    virtual void FillSpritePointers(std::vector<class Sprite*>* out) { }

    /// <summary>
    /// シーンが保持するパーティクルエミッターのポインタを取得する。
    /// </summary>
    virtual void FillParticleEmitterPointers(std::vector<::ParticleEmitter*>* out) { }

    /// <summary>
    /// Scene View テクスチャの SRV 番号を取得する。
    /// </summary>
    virtual uint32_t GetSceneViewSrvIndex() const { return UINT32_MAX; }

    /// <summary>
    /// シーンが使用するポストプロセスを取得する。
    /// </summary>
    virtual PostProcess* GetPostProcess() { return nullptr; }

    /// <summary>
    /// シーン固有の ImGui を描画する。
    /// </summary>
    virtual void DrawImGui() { }

    /// <summary>
    /// ウィンドウリサイズ時の処理を行う。
    /// </summary>
    virtual void OnWindowResize(uint32_t /*width*/, uint32_t /*height*/) { }
};

} // namespace MyEngine
