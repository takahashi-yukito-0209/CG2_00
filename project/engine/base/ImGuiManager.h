#pragma once
#include <MathTypes.h>

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

#endif

// 描画時にコマンドリストを受け取るための前方宣言
struct ID3D12GraphicsCommandList;

// ImGui表示対象として参照するクラスの前方宣言
class ParticleEmitter;

namespace MyEngine {
class Object3dCommon;
class Object3d;
class ParticleManager;
class PostProcess;
}

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace MyEngine {

class SrvManager;

/// <summary>
/// ImGuiの初期化、フレーム管理、デバッグUI構築を担当するクラス
/// </summary>
class ImGuiManager {
public: // メンバ関数
    /// <summary>
    /// ImGuiとバックエンドを初期化する
    /// </summary>
    void Initialize(void* hwnd, class SrvManager* srvManager);

    /// <summary>
    /// ImGuiとバックエンドを終了する
    /// </summary>
    void Shutdown();

    /// <summary>
    /// ImGuiの新しいフレームを開始する
    /// </summary>
    void NewFrame();

    // ImGuiの構築に必要な参照をまとめた構造体
    struct Context {
        ParticleEmitter* particleEmitter = nullptr; // パーティクルエミッター
        std::vector<ParticleEmitter*>* particleEmitters = nullptr; // 表示・編集対象のパーティクルエミッター一覧
        Object3dCommon* object3dCommon = nullptr; // 3Dオブジェクト共通設定
        std::vector<Object3d*>* objects3d = nullptr; // 表示・編集対象の3Dオブジェクト一覧
        std::vector<class Sprite*>* sprites = nullptr; // 表示・編集対象のスプライト一覧
        class SpriteCommon* spriteCommon = nullptr; // スプライト共通設定
        bool* useBillboard = nullptr; // ビルボード描画の有効フラグ
        class ParticleManager* particleManager = nullptr; // パーティクル管理
        float dt = 0.0f; // フレームのデルタタイム
        bool* useDebugCameraForRender = nullptr; // 描画にデバッグカメラを使用するか
        std::function<void(const char*)> requestSceneChange; // ImGuiからのシーン切替要求
        const char* currentSceneName = nullptr; // 現在のシーン名
        PostProcess* postProcess = nullptr; // 現在のシーンが使用しているポストプロセス
        SrvManager* srvManager = nullptr; // Scene View用SRVをGPUハンドルへ変換する管理クラス
        uint32_t sceneViewSrvIndex = UINT32_MAX; // Scene Viewに表示するシーン描画結果のSRV番号
        float sceneViewWidth = 0.0f; // Scene Viewに表示する画像の横幅
        float sceneViewHeight = 0.0f; // Scene Viewに表示する画像の縦幅
        const Math::Matrix4x4* sceneViewMatrix = nullptr; // Scene View描画に使うビュー行列
        const Math::Matrix4x4* sceneProjectionMatrix = nullptr; // Scene View描画に使う射影行列
    };

    /// <summary>
    /// Contextの情報を使ってImGuiのデバッグUIを構築する
    /// </summary>
    void BuildUI(Context& ctx);

    /// <summary>
    /// ImGuiの描画コマンドを発行する
    /// </summary>
    void Render(ID3D12GraphicsCommandList* commandList);

    /// <summary>
    /// ImGuiが現在マウスまたはキーボード入力を使用しているかを取得する
    /// </summary>
    bool IsCapturingInput();

    /// <summary>
    /// Scene Viewの描画領域にマウスが乗っているかを取得する
    /// </summary>
    bool IsSceneViewHovered() const { return sceneViewHovered_; }

    /// <summary>
    /// 3Dオブジェクト削除後の選択状態を補正する。
    /// </summary>
    void NotifyObjectDeleted(size_t deletedObjectIndex, size_t remainingObjectCount);

    /// <summary>
    /// スプライト削除後の選択状態を補正する。
    /// </summary>
    void NotifySpriteDeleted(size_t deletedSpriteIndex, size_t remainingSpriteCount);

    /// <summary>
    /// パーティクルエミッター削除後の選択状態を補正する。
    /// </summary>
    void NotifyParticleEmitterDeleted(size_t deletedEmitterIndex, size_t remainingEmitterCount);

private: // メンバ関数
    /// <summary>
    /// シーン描画結果をScene Viewウィンドウへ表示する
    /// </summary>
    void DrawSceneViewWindow(Context& ctx);

#ifdef USE_IMGUI
    /// <summary>
    /// Scene View上に選択中オブジェクトの移動ギズモを描画する
    /// </summary>
    void DrawTranslationGizmo(Context& ctx, const ImVec2& imageMin, const ImVec2& imageSize);

    /// <summary>
    /// ワールド座標をScene View上のスクリーン座標へ変換する
    /// </summary>
    bool ProjectWorldToSceneView(const Math::Vector3& worldPosition, const Math::Matrix4x4& viewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize, ImVec2* outScreenPosition) const;

    /// <summary>
    /// Scene View上のクリック位置に近い3Dオブジェクトを選択する
    /// </summary>
    void SelectObjectBySceneViewClick(Context& ctx, const Math::Matrix4x4& viewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize);

    /// <summary>
    /// Scene View上のクリック位置に重なるスプライトを選択する
    /// </summary>
    void SelectSpriteBySceneViewClick(Context& ctx, const ImVec2& imageMin, const ImVec2& imageSize);

    /// <summary>
    /// Scene View上に選択中スプライトの2Dギズモを描画する
    /// </summary>
    void DrawSprite2DGizmo(Context& ctx, const ImVec2& imageMin, const ImVec2& imageSize);

    /// <summary>
    /// Scene View上に選択中パーティクルエミッターの3Dギズモを描画する
    /// </summary>
    void DrawParticleEmitterGizmo(Context& ctx, const Math::Matrix4x4& viewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize);

    /// <summary>
    /// Scene View上にGPU Emitterの3Dギズモを描画する
    /// </summary>
    void DrawGpuParticleEmitterGizmo(Context& ctx, const Math::Matrix4x4& viewProjectionMatrix, const ImVec2& imageMin, const ImVec2& imageSize);

    /// <summary>
    /// 移動ギズモの1軸を描画し、ドラッグ時は平行移動量を返す
    /// </summary>
    bool DrawGizmoAxis(const char* id, const ImVec2& origin, const ImVec2& axisEnd, ImU32 color, float axisWorldLength, bool drawAxis, float* outMoveAmount, bool* outIsActive, bool* outIsActivated);

    /// <summary>
    /// 現在の描画対象に対応するギズモ操作対象番号を取得する
    /// </summary>
    int ResolveGizmoObjectIndex(const Context& ctx) const;
#endif

    /// <summary>
    /// シーン情報のImGuiを描画する
    /// </summary>
    void DrawSceneSection(Context& ctx);

    /// <summary>
    /// ImGuiのドッキング領域を作成する
    /// </summary>
    void DrawDockSpace();

    /// <summary>
    /// ポストプロセス関連のImGuiを描画する
    /// </summary>
    void DrawPostProcessSection(Context& ctx);

    /// <summary>
    /// 3Dオブジェクト関連のImGuiを描画する
    /// </summary>
    void DrawObjectSection(Context& ctx);

    /// <summary>
    /// パーティクル関連のImGuiを描画する
    /// </summary>
    void DrawParticleSection(Context& ctx);

    /// <summary>
    /// スプライト関連のImGuiを描画する
    /// </summary>
    void DrawSpriteSection(Context& ctx);

    /// <summary>
    /// 共通設定関連のImGuiを描画する
    /// </summary>
    void DrawCommonSection(Context& ctx);

    /// <summary>
    /// カメラ関連のImGuiを描画する
    /// </summary>
    void DrawCameraWindow(Context& ctx);

private:
    bool sceneViewHovered_ = false; // Scene Viewの描画領域にマウスが乗っているか
    int selectedObjectIndex_ = 0; // ImGuiで選択中の3Dオブジェクト番号
    int selectedSpriteIndex_ = 0; // ImGuiで選択中のスプライト番号
    int selectedEmitterIndex_ = 0; // ImGuiで選択中のパーティクルエミッター番号
    int gizmoTargetMode_ = 0; // Scene Viewギズモの対象種別
    int gizmoOperationMode_ = 0; // Scene Viewギズモの操作モード
    int gizmoTransformSpaceMode_ = 0; // Scene Viewギズモの座標空間
    int activeGizmoOperationMode_ = -1; // ドラッグ中のギズモ操作
    int activeGizmoAxisIndex_ = -1; // ドラッグ中のギズモ軸
};
} // namespace MyEngine
