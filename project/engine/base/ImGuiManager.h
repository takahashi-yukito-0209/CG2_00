#pragma once

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

#endif

// 前方宣言: ID3D12GraphicsCommandList インターフェースを宣言（Render 関数の引数でポインタ参照するため）
struct ID3D12GraphicsCommandList;

// 前方宣言: ParticleEmitter クラスを宣言（ImGuiManager でポインタ参照するため）
class ParticleEmitter;

// 前方宣言: Sprite クラスと SpriteCommon クラスを宣言（ImGuiManager でポインタ参照するため）
namespace MyEngine {
class Object3dCommon;
class Object3d;
class ParticleManager;
class PostProcess;
}

#include <cstdint>
#include <vector>

namespace MyEngine {

// 前方宣言: SrvManager クラスを宣言（ImGuiManager でポインタ参照するため）
class SrvManager;

/// <summary>
/// ImGui管理クラス
/// </summary>
class ImGuiManager {
public: // メンバ関数
    /// <summary>
    /// ImGuiとバックエンドの初期化
    /// </summary>
    void Initialize(void* hwnd, class SrvManager* srvManager);

    /// <summary>
    /// ImGuiとバックエンドのシャットダウン
    /// </summary>
    void Shutdown();

    /// <summary>
    /// 新しいフレームの開始（ImGui::NewFrame とバックエンドの NewFrame を呼び出す）
    /// </summary>
    void NewFrame();

    // ImGui コントロールの構築に必要なコンテキストをまとめた構造体
    struct Context {
        // ここに ImGui コントロールの構築に必要なオブジェクトや状態を追加

        // パーティクルエミッタへのポインタ
        ParticleEmitter* particleEmitter = nullptr;
        // Object3dCommon へのポインタ
        Object3dCommon* object3dCommon = nullptr;
        // 3Dオブジェクトのリストへのポインタ
        std::vector<Object3d*>* objects3d = nullptr;
        // スプライトのリストへのポインタ
        std::vector<class Sprite*>* sprites = nullptr;
        // SpriteCommon へのポインタ
        class SpriteCommon* spriteCommon = nullptr;
        // ビルボード描画の有効フラグへのポインタ
        bool* useBillboard = nullptr;
        // ParticleManager へのポインタ
        class ParticleManager* particleManager = nullptr;
        // フレームのデルタタイム
        float dt = 0.0f;
        // レンダリングにデバッグカメラを使うかどうかのフラグへのポインタ
        bool* useDebugCameraForRender = nullptr;
        int* selectedDrawType = nullptr; // 現在の描画対象を選択する値
        // 現在のシーン名へのポインタ
        const char* currentSceneName = nullptr;
        PostProcess* postProcess = nullptr; // 現在のシーンが使用しているポストプロセス
        SrvManager* srvManager = nullptr; // Scene View用SRVをGPUハンドルへ変換するSRV管理
        uint32_t sceneViewSrvIndex = UINT32_MAX; // Scene Viewに表示するシーン描画結果のSRV番号
        float sceneViewWidth = 0.0f; // Scene Viewに表示するシーン描画結果の横幅
        float sceneViewHeight = 0.0f; // Scene Viewに表示するシーン描画結果の縦幅
        // シーン変更要求のコールバック関数（引数は新しいシーン名）
    };

    /// <summary>
    /// ImGui コントロールの構築（Context 構造体を引数にして、必要な情報を渡す）
    /// </summary>
    void BuildUI(Context& ctx);

    /// <summary>
    /// 描画コマンドの発行（ImGui::Render とバックエンドの Render を呼び出す）
    /// </summary>
    void Render(ID3D12GraphicsCommandList* commandList);

    /// <summary>
    /// ImGui が現在 UI によってマウス/入力をキャプチャしているかを返す
    /// </summary>
    bool IsCapturingInput();

private: // メンバ関数
    /// <summary>
    /// シーン表示用テクスチャをImGuiウィンドウ内に描画する
    /// </summary>
    void DrawSceneViewWindow(Context& ctx);

    /// <summary>
    /// シーン情報をImGuiで描画する
    /// </summary>
    void DrawSceneSection(Context& ctx);

    /// <summary>
    /// メインウィンドウ全体に ImGui のドッキング領域を作成する
    /// </summary>
    void DrawDockSpace();

    /// <summary>
    /// ポストエフェクトの設定と状態をImGuiへ表示する
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
    /// 共通設定のImGuiを描画する
    /// </summary>
    void DrawCommonSection(Context& ctx);

    /// <summary>
    /// カメラ関連のImGuiを描画する
    /// </summary>
    void DrawCameraWindow(Context& ctx);
};
} // namespace MyEngine






