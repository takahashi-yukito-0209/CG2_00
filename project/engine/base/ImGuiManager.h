#pragma once

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
        Object3dCommon* object3dCommon = nullptr; // 3Dオブジェクト共通設定
        std::vector<Object3d*>* objects3d = nullptr; // 表示・編集対象の3Dオブジェクト一覧
        std::vector<class Sprite*>* sprites = nullptr; // 表示・編集対象のスプライト一覧
        class SpriteCommon* spriteCommon = nullptr; // スプライト共通設定
        bool* useBillboard = nullptr; // ビルボード描画の有効フラグ
        class ParticleManager* particleManager = nullptr; // パーティクル管理
        float dt = 0.0f; // フレームのデルタタイム
        bool* useDebugCameraForRender = nullptr; // 描画にデバッグカメラを使用するか
        int* selectedDrawType = nullptr; // 現在の描画対象を選択する値
        std::function<void(const char*)> requestSceneChange; // ImGuiからのシーン切替要求
        const char* currentSceneName = nullptr; // 現在のシーン名
        PostProcess* postProcess = nullptr; // 現在のシーンが使用しているポストプロセス
        SrvManager* srvManager = nullptr; // Scene View用SRVをGPUハンドルへ変換する管理クラス
        uint32_t sceneViewSrvIndex = UINT32_MAX; // Scene Viewに表示するシーン描画結果のSRV番号
        float sceneViewWidth = 0.0f; // Scene Viewに表示する画像の横幅
        float sceneViewHeight = 0.0f; // Scene Viewに表示する画像の縦幅
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

private: // メンバ関数
    /// <summary>
    /// シーン描画結果をScene Viewウィンドウへ表示する
    /// </summary>
    void DrawSceneViewWindow(Context& ctx);

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
};
} // namespace MyEngine