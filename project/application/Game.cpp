#include "Game.h"
#include "Camera.h"
#include "DebugCamera.h"
#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "InputManager.h"
#include "Logger.h"
#include "Model.h"
#include "ModelManager.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "StringUtility.h"
#include "TextureManager.h"
#include "WinApp.h"
#include "application/scenes/SceneFactory.h"
#include "engine/base/SceneManager.h"
#include "engine/base/SrvManager.h"
#include "engine/sound/Sound.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include "mathUtility.h"
#include <Windows.h>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <d3d12.h>
#include <dbghelp.h>
#include <dinput.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <list>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <strsafe.h>
#include <vector>
#include <wrl.h>
#include <xaudio2.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dinput8.lib")

using namespace MyEngine;
using namespace Math;

namespace {
constexpr Vector3 kInitialCameraRotate = { 5.9f, -7.43f, 0.0f }; // 初期カメラの回転角
constexpr Vector3 kInitialCameraTranslate = { 0.0f, 1.0f, -18.0f }; // 初期カメラの位置
constexpr Vector4 kWhiteColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色
constexpr Vector4 kPointLightPosition = { 0.0f, 1.5f, 0.0f, 0.0f }; // 点光源の位置
constexpr Vector4 kPointLightColor = { 1.0f, 1.0f, 1.0f, 1.5f }; // 点光源の色と輝度
constexpr float kPointLightRadius = 6.0f; // 点光源の有効半径
constexpr float kLightDecay = 2.0f; // ライトの減衰率
constexpr Vector4 kSpotLightPosition = { 2.0f, 1.25f, -3.0f, 0.0f }; // スポットライトの位置
constexpr Vector4 kSpotLightColor = { 1.0f, 1.0f, 1.0f, 2.0f }; // スポットライトの色と輝度
constexpr float kSpotLightDistance = 7.0f; // スポットライトの有効距離
constexpr Vector3 kSpotLightDirection = { -1.0f, -1.0f, 0.0f }; // スポットライトの向き
constexpr float kPi = 3.14159265358979323846f; // 円周率
constexpr float kSpotLightAngle = kPi / 3.0f; // スポットライトの照射角
constexpr float kSpotLightFalloffStartAngle = kPi / 2.0f; // スポットライトの減衰開始角
constexpr float kInitialDebugCameraWidth = 1280.0f; // デバッグカメラの初期横解像度
constexpr float kInitialDebugCameraHeight = 720.0f; // デバッグカメラの初期縦解像度
constexpr float kDefaultCameraRotateSpeed = 0.01f; // 通常カメラのマウス回転速度
constexpr float kDefaultCameraZoomSpeed = 0.1f; // 通常カメラのホイール移動速度
constexpr float kFixedDeltaTime = 1.0f / 60.0f; // 固定更新のデルタタイム
constexpr size_t kLogDateBufferSize = 32; // ログファイル日時文字列のバッファサイズ
constexpr size_t kDebugCameraLogBufferSize = 128; // デバッグカメラログ用バッファサイズ
}

Game::Game()
    : impl_(nullptr)
{
}

// 描画する内容の種類を選択するための列挙型
enum DrawType {
    DRAW_MODEL, // モデル描画
    DRAW_PARTICLE, // パーティクル描画
    DRAW_SPRITE, // スプライト描画
    DRAW_BUNNY, // バニー描画
    DRAW_FENCE, // フェンス描画
    DRAW_CHECKER, // チェッカー描画
    DRAW_SPHERE, // 球描画
    DRAW_SIMPLE_SKIN, // simpleSkinモデル描画
    DRAW_HUMAN_SNEAK_WALK, // human/sneakWalkモデル描画
    DRAW_HUMAN_WALK, // human/walkモデル描画
    DRAW_ALL, // すべて描画
};

// Game クラスの実装を隠蔽するための Impl 構造体
struct Game::Impl {

    // 初期化フラグと終了要求フラグ
    bool initialized = false; // 初期化完了フラグ
    bool endRequested = false; // 終了要求フラグ

    // ここにゲームの状態やリソースを管理するメンバ変数を追加

    // トランスフォーム（スケール、回転、平行移動）をまとめた構造体
    Transform transform = {};

    WinApp winApp; // ウィンドウアプリケーション管理
    HWND hwnd = nullptr; // ウィンドウハンドル
    Microsoft::WRL::ComPtr<IDirectInput8> directInput; // DirectInputインターフェース
    SoundSystem soundSystem; // サウンドシステム
    std::shared_ptr<SoundClip> soundData1; // サウンドデータの共有ポインタ

    std::unique_ptr<SpriteCommon> spriteCommon; // スプライト共通管理
    SrvManager srvManager; // シェーダーリソースビュー管理
    std::unique_ptr<Object3dCommon> object3dCommon; // 3Dオブジェクト共通管理

    std::unique_ptr<Camera> camera; // カメラ
    std::vector<std::unique_ptr<Sprite>> sprites; // スプライトのリスト
    std::vector<std::unique_ptr<Object3d>> objects3d; // 3Dオブジェクトのリスト
    std::unique_ptr<Object3d> particlePlane; // パーティクル描画用の平面オブジェクト

    std::unique_ptr<SceneManager> sceneManager; // シーンマネージャ

    ParticleEmitter pmEmitter; // パーティクルエミッタ (UIの操作用など軽量なまま保持)

    int selectedDrawType = DRAW_PARTICLE; // 描画する内容の種類を選択するための変数

    DebugCamera debugCamera; // デバッグカメラ (global DebugCamera)
    bool isDebugCameraControl = true; // デバッグカメラ操作フラグ
    bool useBillboard = true; // ビルボードの使用フラグ
    bool useDebugCameraForRender = false; // レンダリングにデバッグカメラを使うか

    ImGuiManager imguiManager; // ImGui管理

    // 保留中のシーン切替要求を格納する（ImGui コールバックから直接 ChangeScene を呼ばないため）
    std::string pendingSceneName;
};

/// <summary>
/// Game クラスのデストラクタ
/// </summary>
Game::~Game()
{
}

/// <summary>
/// クラッシュダンプ出力を設定する
/// </summary>
void Game::SetupCrashDumpHandler()
{
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* exception) -> LONG {
        SYSTEMTIME time;
        GetLocalTime(&time);
        wchar_t filePath[MAX_PATH] = { 0 };
        CreateDirectory(L"./Dumps", nullptr);
        StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
        HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
        DWORD processId = GetCurrentProcessId();
        DWORD threadId = GetCurrentThreadId();
        MINIDUMP_EXCEPTION_INFORMATION minidumpInformation { 0 };
        minidumpInformation.ThreadId = threadId;
        minidumpInformation.ExceptionPointers = exception;
        minidumpInformation.ClientPointers = TRUE;
        MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
        CloseHandle(dumpFileHandle);
        return EXCEPTION_EXECUTE_HANDLER;
    }); // クラッシュダンプの出力先を D:\Dumps\ に設定
}

/// <summary>
/// ログファイル出力を設定する
/// </summary>
void Game::SetupLogFile()
{
    std::filesystem::create_directory("logs"); // "logs" フォルダが存在しない場合は作成する
    std::time_t now_c = std::time(nullptr); // 現在の時刻を取得
    struct tm local_tm; // ローカルタイムに変換するための構造体
    localtime_s(&local_tm, &now_c); // 現在の時刻をローカルタイムに変換
    char dateBuf[kLogDateBufferSize]; // 日付と時刻を "YYYYMMDD_HHMMSS" 形式の文字列にフォーマットするためのバッファ
    std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d_%H%M%S", &local_tm); // 日付と時刻を "YYYYMMDD_HHMMSS" 形式の文字列にフォーマット
    std::string dateString(dateBuf); // フォーマットした日付と時刻を std::string に変換
    std::string logFilePath = std::string("logs/") + dateString + ".log"; // ログファイルのパスを "logs/YYYYMMDD_HHMMSS.log" 形式で作成
    Logger::SetLogFile(logFilePath); // ログファイルのパスを Logger に設定
    Logger::SetErrorLogFile(logFilePath); // エラーログファイルのパスも同じログファイルに設定
}

/// <summary>
/// ウィンドウと入力を初期化する
/// </summary>
bool Game::InitializeWindowAndInput(HINSTANCE hInstance, int nCmdShow)
{
    impl_->winApp.Initialize(hInstance, nCmdShow, L"LE3C_13_タカハシ_ユキト");
    impl_->hwnd = impl_->winApp.GetHwnd();

#ifdef _DEBUG

    // デバッグレイヤーとGPUベースのバリデーションを有効にする
    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
    // D3D12GetDebugInterface を呼び出して ID3D12Debug1 インターフェースを取得し、成功したらデバッグレイヤーとGPUベースのバリデーションを有効にする
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer(); // デバッグレイヤーを有効にする
        // GPUベースのバリデーションはドライバに強い負荷を与えることがあるため無効化する
        debugController->SetEnableGPUBasedValidation(FALSE);
    }

#endif

    // DirectInput を初期化
    HRESULT result = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, reinterpret_cast<void**>(impl_->directInput.GetAddressOf()), nullptr);
    // DirectInput8Create を呼び出して DirectInput インターフェースを取得し、失敗した場合はエラーログを出力して初期化を終了する
    if (FAILED(result)) {
        Logger::Log("Error: DirectInput8Create failed.\n");
        impl_->winApp.Finalize();
        return false;
    }

    // InputManagerの初期化
    if (!InputManager::GetInstance()->Initialize(impl_->directInput.Get(), impl_->hwnd)) {
        Logger::Log("Error: InputManager::Initialize failed.\n");
        impl_->winApp.Finalize();
        return false;
    }

    return true;
}

/// <summary>
/// エンジン共通リソースを初期化する
/// </summary>
bool Game::InitializeEngineResources(HINSTANCE hInstance)
{
    std::unique_ptr<SpriteCommon> spriteCommonTmp;
    // 3Dオブジェクト共通管理の一時的なユニークポインタ
    std::unique_ptr<Object3dCommon> object3dCommonTmp;

    // エンジンの初期化処理を呼び出す。失敗した場合はエラーログを出力して初期化を終了する
    if (!Framework::InitializeEngine(hInstance, &impl_->winApp, impl_->hwnd,
            spriteCommonTmp, impl_->srvManager, object3dCommonTmp)) {
        Logger::Log("Error: Framework::InitializeEngine failed\n");
        impl_->winApp.Finalize();
        return false;
    }

    // 共通管理オブジェクトを Impl に移動

    // スプライト共通管理を Impl に移動
    impl_->spriteCommon = std::move(spriteCommonTmp);
    // 3Dオブジェクト共通管理を Impl に移動
    impl_->object3dCommon = std::move(object3dCommonTmp);

    return true;
}

/// <summary>
/// カメラとライトを初期化する
/// </summary>
void Game::InitializeCameraAndLighting()
{
    impl_->camera = std::make_unique<Camera>();
    impl_->camera->SetRotate(kInitialCameraRotate);
    impl_->camera->SetTranslate(kInitialCameraTranslate);
    impl_->camera->Update();
    // Object3dCommon にデフォルトカメラをセット
    impl_->object3dCommon->SetDefaultCamera(impl_->camera.get());

    // ライトの設定
    Object3d::DirectionalLight* directionalLightData = impl_->object3dCommon->GetDirectionalLightData();
    // directionalLightData が存在する場合は、ライトの強度、色、方向を設定する
    if (directionalLightData) {
        // ライトの強度を 1.0f に設定
        directionalLightData->intensity = kWhiteColor.w;
        directionalLightData->color = kWhiteColor;
        // ライトの方向を下向きに設定
        directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
    }

    // 点光源の設定
    if (impl_->object3dCommon) {
        Object3d::PointLight pl = {};
        pl.position = kPointLightPosition;
        // 点光源の色を白に設定し、w成分に輝度を指定する（ここでは1.5fで少し強めの光にしている）
        pl.color = kPointLightColor;
        pl.radius = kPointLightRadius; // 点光源の有効範囲を半径6.0fに設定
        pl.decay = kLightDecay; // 減衰を2.0fに設定
        pl.enabled = 1; // 点光源を有効にする
        // Object3dCommon に点光源を追加する
        impl_->object3dCommon->AddPointLight(pl);
    }

    // スポットライトの設定
    if (impl_->object3dCommon) {
        auto sl = impl_->object3dCommon->GetSpotLightData();
        if (sl) {
            sl->position = kSpotLightPosition;
            // スポットライトの色を白に設定し、w成分に輝度を指定する（ここでは2.0fで少し強めの光にしている）
            sl->color = kSpotLightColor;
            sl->distance = kSpotLightDistance; // スポットライトの有効範囲を距離7.0fに設定
            sl->direction = MathUtil::Normalize(kSpotLightDirection); // スポットライトの向きを下斜め左に設定
            sl->decay = kLightDecay; // 減衰を2.0fに設定
            sl->cosAngle = cosf(kSpotLightAngle); // スポットライトの照射角を60度に設定（コサイン値で指定）
            sl->cosFalloffStart = cosf(kSpotLightFalloffStartAngle); // スポットライトの減衰開始角を90度に設定（コサイン値で指定）
            sl->enabled = 1; // スポットライトを有効にする
        }
    }

    // デバッグカメラの初期化（ウィンドウ解像度を指定）
    impl_->debugCamera.Initialize(kInitialDebugCameraWidth, kInitialDebugCameraHeight);

    // Object3dCommon にデバッグカメラをセットして、UI側で編集できるようにする
    if (impl_->object3dCommon) {
        impl_->object3dCommon->SetDebugCamera(&impl_->debugCamera);
        impl_->object3dCommon->SetUseDebugCameraForRender(impl_->useDebugCameraForRender);
        impl_->object3dCommon->SetEnableDebugCameraInput(impl_->isDebugCameraControl);
    }
}

/// <summary>
/// デバッグ機能、ImGui、サウンドを初期化する
/// </summary>
void Game::InitializeDebugToolsAndSound()
{
    impl_->imguiManager.Initialize(impl_->hwnd, &impl_->srvManager);

    // Media FoundationとXAudio2を明示的に初期化する
    if (impl_->soundSystem.Initialize()) {
        impl_->soundData1 = impl_->soundSystem.LoadFromFile("resources/mokugyo.wav");
    } else {
        Logger::Warn("Game::Initialize: sound system initialization failed.\n");
    }
}

/// <summary>
/// リサイズ通知を設定する
/// </summary>
void Game::SetupResizeCallbacks()
{
    // DirectXCommon のリサイズ通知を受け取り、カメラとシーンへ反映する
    DirectXCommon::GetInstance()->SetOnResizeCallback([this](uint32_t w, uint32_t h) {
        // カメラのアスペクト比を更新する
        if (impl_->camera) {
            float aspect = (h != 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.0f; // 画面のアスペクト比
            impl_->camera->SetAspectRatio(aspect);
            impl_->camera->Update();
        }
        // デバッグカメラへ現在の画面サイズを反映する
        impl_->debugCamera.Initialize(static_cast<float>(w), static_cast<float>(h));
        // 現在のシーンへリサイズを通知する
        if (impl_->sceneManager) {
            impl_->sceneManager->OnWindowResize(w, h);
        }
    });

    // WinApp のリサイズ通知を DirectXCommon 側へ橋渡しする
    impl_->winApp.SetResizeCallback([](uint32_t width, uint32_t height) {
        DirectXCommon::GetInstance()->OnWindowResize(width, height);
    });
}

/// <summary>
/// シーン管理を初期化する
/// </summary>
void Game::InitializeScene()
{
    impl_->sceneManager = std::make_unique<SceneManager>();
    impl_->sceneManager->Initialize();
    SetupResizeCallbacks();

    SceneContext sctx; // シーンへ渡す共通コンテキスト
    sctx.object3dCommon = impl_->object3dCommon.get();
    sctx.spriteCommon = impl_->spriteCommon.get();
    sctx.camera = impl_->camera.get();
    sctx.selectedDrawType = static_cast<int>(impl_->selectedDrawType);
    sctx.particleManager = ParticleManager::GetInstance();
    sctx.textureManager = TextureManager::GetInstance();
    sctx.srvManager = &impl_->srvManager;
    sctx.directXCommon = DirectXCommon::GetInstance();
    sctx.imguiManager = &impl_->imguiManager;
    sctx.requestSceneChange = [this](const std::string& sceneName) {
        impl_->pendingSceneName = sceneName;
    };
    impl_->sceneManager->SetContext(sctx);

    auto initial = GameApp::SceneFactory::Create("Title"); // 最初に表示するシーン
    if (initial) {
        impl_->sceneManager->ChangeScene(std::move(initial));
    }
}

/// <summary>
/// ゲームの初期化処理
/// </summary>
bool Game::Initialize(HINSTANCE hInstance, int nCmdShow)
{
    // 基底クラスの初期化を行う
    if (!Framework::Initialize(hInstance, nCmdShow)) {
        return false;
    }

    // 実装データを必要に応じて生成する
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }

    SetupCrashDumpHandler();
    SetupLogFile();

    if (!InitializeWindowAndInput(hInstance, nCmdShow)) {
        return false;
    }

    if (!InitializeEngineResources(hInstance)) {
        return false;
    }

    InitializeCameraAndLighting();
    InitializeDebugToolsAndSound();

    impl_->initialized = true;
    impl_->endRequested = false;

    InitializeScene();

    return true;
}

/// <summary>
/// ゲームの更新処理
/// </summary>
void Game::Update()
{
    // 基底クラスの更新処理を呼び出す
    Framework::Update();

    // Impl が存在しないか、初期化が完了していない場合は更新処理をスキップする
    if (!impl_ || !impl_->initialized) {
        return;
    }

    // ウィンドウメッセージの処理。終了要求があればフラグを立てて更新処理を終了する
    if (!impl_->winApp.ProcessMessage()) {
        impl_->endRequested = true;
        return;
    }

    // 入力の更新
    InputManager::GetInstance()->Update();

    // デバッグカメラの操作: マウスドラッグとホイールでカメラを操作する。ただし、ImGui のウィンドウやアイテムがアクティブな場合は操作しない
    bool debugInputEnabled = impl_->object3dCommon ? impl_->object3dCommon->GetEnableDebugCameraInput() : impl_->isDebugCameraControl;
    if (debugInputEnabled) {
        long deltaX = InputManager::GetInstance()->GetMouseDeltaX(); // 前フレームからのマウスのX移動量を取得
        long deltaY = InputManager::GetInstance()->GetMouseDeltaY(); // 前フレームからのマウスのY移動量を取得
        long wheelDelta = InputManager::GetInstance()->GetMouseDeltaZ(); // 前フレームからのマウスホイールの移動量を取得

        // ImGui がマウスをキャプチャしていると通常はカメラ操作を受け付けないが、
        // 右ボタンでドラッグしたときは UI 上でもカメラ操作できるようにする。
        bool allowCameraControl = false;
        // ImGui 上の入力やウィンドウがアクティブでない場合は許可
#ifdef USE_IMGUI
        if (!impl_->imguiManager.IsCapturingInput()) {
            allowCameraControl = true;
        }
#else
        // ImGui を使わない場合は常にカメラ操作を許可
        allowCameraControl = true;
#endif

        // いずれかのマウスボタンを押している場合はカメラ操作を許可
        if (InputManager::GetInstance()->IsMouseButtonPressed(0) || InputManager::GetInstance()->IsMouseButtonPressed(1) || InputManager::GetInstance()->IsMouseButtonPressed(2)) {
            allowCameraControl = true;
        }

        // ホイールが動いた場合はカメラのズームとして処理を許可（UI スクロールと競合する可能性あり）
        if (wheelDelta != 0) {
            allowCameraControl = true;
        }

        if (allowCameraControl) {
            bool useDebugRender = impl_->object3dCommon ? impl_->object3dCommon->GetUseDebugCameraForRender() : impl_->useDebugCameraForRender;
            if (useDebugRender) {
                // デバッグカメラを操作
                impl_->debugCamera.OnMouseDrag(float(deltaX), float(deltaY));
                impl_->debugCamera.OnMouseWheel(float(wheelDelta));
                impl_->debugCamera.Update();
            } else {
                // デフォルトカメラを操作（UIで編集しているカメラ）
                if (impl_->camera) {
                    // 回転はラジアン単位で適用
                    const float rotateSpeed = kDefaultCameraRotateSpeed;
                    Vector3 crot = impl_->camera->GetRotate();
                    crot.y += float(deltaX) * rotateSpeed;
                    crot.x += float(deltaY) * rotateSpeed;
                    impl_->camera->SetRotate(crot);

                    // ホイールで前後移動（ズーム）
                    const float zoomSpeed = kDefaultCameraZoomSpeed;
                    Vector3 cpos = impl_->camera->GetTranslate();
                    cpos.z += float(wheelDelta) * zoomSpeed;
                    impl_->camera->SetTranslate(cpos);

                    // カメラの行列を更新
                    impl_->camera->Update();
                }
            }
        } else {
            // allowCameraControl が false でもデバッグカメラの Update は必要
            impl_->debugCamera.Update();
        }
    }

    // スペースキーが押された瞬間にサウンドを再生する。スペースキーが離された瞬間は再生しないようにする
    if (InputManager::GetInstance()->IsKeyJustPressed(DIK_SPACE) && !InputManager::GetInstance()->IsKeyJustReleased(DIK_SPACE)) {
        impl_->soundSystem.Play(impl_->soundData1); // サウンドシステムを使用してサウンドデータを再生する
    }

    // 固定タイムステップで更新（ここでは 1/60 秒固定）
    const float dt = kFixedDeltaTime;
    // SoundSystem の Poll を呼び出して、サウンドの再生状態の更新やリソースの管理を行う
    impl_->soundSystem.Poll();

    // デバッグカメラのマウス/ホイール入力を ImGui が入力を奪っていない場合にフォワードする
    bool debugForwardEnabled = impl_->object3dCommon ? impl_->object3dCommon->GetEnableDebugCameraInput() : impl_->isDebugCameraControl;
    if (debugForwardEnabled) {
        // ImGui が入力を処理している場合はカメラ操作を無効化
        if (!impl_->imguiManager.IsCapturingInput()) {
            auto input = InputManager::GetInstance();
            long dx = input->GetMouseDeltaX();
            long dy = input->GetMouseDeltaY();
            long dz = input->GetMouseDeltaZ();
            if (dx != 0 || dy != 0) {
                impl_->debugCamera.OnMouseDrag(static_cast<float>(dx), static_cast<float>(dy));
            }
            if (dz != 0) {
                impl_->debugCamera.OnMouseWheel(static_cast<float>(dz));
            }
        }
        // キーボードによるデバッグカメラの移動処理（WASD等）
        impl_->debugCamera.Update();
    }

    // 3Dオブジェクトのワールド行列、ビュー行列、プロジェクション行列を計算して、各オブジェクトの Update を呼び出す
    Matrix4x4 worldMatrix = MathUtil::MakeAffineMatrix(impl_->transform.scale, impl_->transform.rotate, impl_->transform.translate);
    Matrix4x4 viewMatrix;
    Matrix4x4 projectionMatrix;

    // F1 でレンダリングにデバッグカメラを切り替えられるようにする
    if (InputManager::GetInstance()->IsKeyJustPressed(DIK_F1)) {
        if (impl_->object3dCommon) {
            bool cur = impl_->object3dCommon->GetUseDebugCameraForRender();
            impl_->object3dCommon->SetUseDebugCameraForRender(!cur);
            char buf[kDebugCameraLogBufferSize];
            sprintf_s(buf, "Debug camera for render: %s\n", !cur ? "ON" : "OFF");
            Logger::Log(buf);
        } else {
            impl_->useDebugCameraForRender = !impl_->useDebugCameraForRender;
            char buf[kDebugCameraLogBufferSize];
            sprintf_s(buf, "Debug camera for render: %s\n", impl_->useDebugCameraForRender ? "ON" : "OFF");
            Logger::Log(buf);
        }
    }

    bool useDebugForRender = impl_->object3dCommon ? impl_->object3dCommon->GetUseDebugCameraForRender() : impl_->useDebugCameraForRender;
    if (useDebugForRender) {
        // デバッグカメラを使ってビュー/射影を取得
        impl_->debugCamera.Update();
        viewMatrix = impl_->debugCamera.GetViewMatrix();
        projectionMatrix = impl_->debugCamera.GetProjectionMatrix();
    } else {
        if (impl_->camera) {
            impl_->camera->Update();
            viewMatrix = impl_->camera->GetViewMatrix();
            projectionMatrix = impl_->camera->GetProjectionMatrix();
        } else {
            // フォールバックとしてデバッグカメラを使う
            impl_->debugCamera.Update();
            viewMatrix = impl_->debugCamera.GetViewMatrix();
            projectionMatrix = impl_->debugCamera.GetProjectionMatrix();
        }
    }

    Matrix4x4 worldViewProjectionMatrix = MathUtil::Multiply(worldMatrix, MathUtil::Multiply(viewMatrix, projectionMatrix));

    // Object3dCommon にカメラのワールド位置をセット
    if (impl_->object3dCommon) {
        auto cam = impl_->object3dCommon->GetCameraData();
        if (cam) {
            if (useDebugForRender) {
                cam->worldPosition = impl_->debugCamera.GetTranslation();
            } else if (impl_->camera) {
                cam->worldPosition = impl_->camera->GetTranslate();
            }
            cam->view = viewMatrix;
        }
    }

    // 3Dオブジェクトのリストをループして、各オブジェクトの Update を呼び出す。これにより、オブジェクトのワールド行列やその他の状態が更新される
    for (auto& obj : impl_->objects3d) {
        if (obj) {
            obj->Update(viewMatrix, projectionMatrix);
        }
    }

    // スプライトのリストをループして、各スプライトの Update を呼び出す。これにより、スプライトの位置やその他の状態が更新される
    for (uint32_t i = 0; i < impl_->sprites.size(); i++) {
        if (impl_->sprites[i]) {
            impl_->sprites[i]->Update();
        }
    }

    // シーンの更新
    if (impl_->sceneManager) {
        impl_->sceneManager->Update(dt);
    }
}

/// <summary>
/// ゲームの描画処理
/// </summary>
void Game::Draw()
{

    // 存在しない場合や初期化が完了していない場合は描画処理をスキップする
    if (!impl_ || !impl_->initialized) {
        return;
    }

    // ImGui の新しいフレームを開始する。これにより、ImGui の内部状態がリセットされ、UIの構築が可能になる
    impl_->imguiManager.NewFrame();

    // 保留中のシーン切替はImGui構築前に反映し、UI表示と描画対象を同じシーンに揃える。
    if (impl_->pendingSceneName.size() > 0) {
        if (impl_->sceneManager && impl_->sceneManager->GetCurrentSceneName() == impl_->pendingSceneName) {
            impl_->pendingSceneName.clear();
        } else {
            auto newScene = GameApp::SceneFactory::Create(impl_->pendingSceneName);
            if (newScene && impl_->sceneManager) {
                impl_->sceneManager->ChangeScene(std::move(newScene));
            }
            impl_->pendingSceneName.clear();
        }
    }
    ImGuiManager::Context ctx;
    // 描画に必要な情報を ImGuiManager::Context にセットして、UIの構築に使用できるようにする
    ctx.particleEmitter = &impl_->pmEmitter; // パーティクルエミッタのポインタをセット
    // Object3dCommon のポインタをセットして、UIで共通の描画設定やライトの情報などにアクセスできるようにする
    ctx.object3dCommon = impl_->object3dCommon.get();
    // Object3dCommon のポインタをセットして、UIで3Dオブジェクトの共通設定や情報にアクセスできるようにする
    std::vector<Object3d*> objPtrs;
    // シーンマネージャが存在し、現在のシーンがある場合は、シーンから3Dオブジェクトのポインタを取得してセットする。
    // そうでない場合は、Impl の objects3d からポインタをセットする
    if (impl_->sceneManager && impl_->sceneManager->GetCurrent()) {
        impl_->sceneManager->GetCurrent()->FillObject3dPointers(&objPtrs);
    } else {
        objPtrs.reserve(impl_->objects3d.size());
        for (auto& o : impl_->objects3d) {
            objPtrs.push_back(o.get());
        }
    }
    ctx.objects3d = &objPtrs;
    // SpriteCommon のポインタをセットして、UIでスプライトの共通設定や情報にアクセスできるようにする
    std::vector<Sprite*> spritePtrs;
    if (impl_->sceneManager && impl_->sceneManager->GetCurrent()) {
        impl_->sceneManager->GetCurrent()->FillSpritePointers(&spritePtrs);
    } else {
        spritePtrs.reserve(impl_->sprites.size());
        for (auto& s : impl_->sprites) {
            spritePtrs.push_back(s.get());
        }
    }
    ctx.sprites = &spritePtrs;
    ctx.spriteCommon = impl_->spriteCommon.get();
    // ビルボードの使用フラグへのポインタをセットして、UIでビルボードのオンオフができるようにする
    ctx.useBillboard = &impl_->useBillboard;
    // ParticleManager のポインタをセットして、UIでパーティクルの情報や設定にアクセスできるようにする
    ctx.particleManager = ParticleManager::GetInstance();
    // デルタタイムをセットして、UIでフレームごとの時間の情報にアクセスできるようにする
    ctx.dt = kFixedDeltaTime;
    ctx.useDebugCameraForRender = &impl_->useDebugCameraForRender;
    ctx.selectedDrawType = &impl_->selectedDrawType;
    ctx.requestSceneChange = [this](const char* sceneName) {
        if (sceneName) {
            impl_->pendingSceneName = sceneName;
        }
    };
    // シーン名を ImGui に渡す
    if (impl_->sceneManager) {
        static std::string sname;
        sname = impl_->sceneManager->GetCurrentSceneName();
        ctx.currentSceneName = sname.c_str();
        if (impl_->sceneManager->GetCurrent()) {
            ctx.postProcess = impl_->sceneManager->GetCurrent()->GetPostProcess();
            ctx.srvManager = &impl_->srvManager;
            ctx.sceneViewSrvIndex = impl_->sceneManager->GetCurrent()->GetSceneViewSrvIndex();
            ctx.sceneViewWidth = DirectXCommon::GetInstance()->GetRenderWidth();
            ctx.sceneViewHeight = DirectXCommon::GetInstance()->GetRenderHeight();
        }
    }
    // ImGuiManager の BuildUI を呼び出して、UIの構築を行う。これにより、UIが描画される準備が整う
    impl_->imguiManager.BuildUI(ctx);
    if (impl_->sceneManager && impl_->sceneManager->GetCurrent()) {
        impl_->sceneManager->GetCurrent()->DrawImGui();
    }

    DirectXCommon::GetInstance()->PreDraw();
    // SrvManager の PreDraw を呼び出して、描画に必要なシェーダーリソースビューのセットアップを行う
    impl_->srvManager.PreDraw();

    // シーン側にも現在の描画モードを伝えて、シーン自身が必要な要素だけ描画できるようにする
    if (impl_->sceneManager) {
        impl_->sceneManager->SetSelectedDrawType(impl_->selectedDrawType);
        if (impl_->sceneManager->GetCurrent()) {
#ifdef USE_IMGUI
            impl_->sceneManager->GetCurrent()->SetSceneViewOnly(true);
#else
            impl_->sceneManager->GetCurrent()->SetSceneViewOnly(false);
#endif
        }
        impl_->sceneManager->Draw();
    }

    // ImGui の描画を行う。DirectXCommon のコマンドリストを渡して、ImGuiManager に描画してもらう
    impl_->imguiManager.Render(DirectXCommon::GetInstance()->GetCommandList());
    // 描画後の共通処理を呼び出す（コマンドリストの終了やバックバッファの表示など）
    DirectXCommon::GetInstance()->PostDraw();
}

/// <summary>
/// ゲームの終了処理
/// </summary>
void Game::Finalize()
{
    // Impl が存在しない場合は終了処理をスキップする
    if (!impl_) {
        return;
    }

    if (!impl_->initialized) {
        return;
    }

    impl_->initialized = false;
    impl_->endRequested = true;

    CloseWindow(impl_->hwnd); // ウィンドウを閉じる

    // シーンは描画共通リソースを参照しているため、エンジン基盤より先に破棄する
    if (impl_->sceneManager) {
        impl_->sceneManager->Finalize();
        impl_->sceneManager.reset();
    }

    // アプリケーション側が直接所有する描画リソースを先に破棄する
    impl_->particlePlane.reset();
    impl_->objects3d.clear();
    impl_->sprites.clear();
    impl_->camera.reset();

    // パーティクルは描画オブジェクトやテクスチャを参照するため、共通基盤より先に終了する
    ParticleManager::GetInstance()->Finalize();

    // サウンドデータのリセット
    impl_->soundData1.reset();
    impl_->soundSystem.Finalize();

    // ImGuiManager の終了処理を呼び出す
    impl_->imguiManager.Shutdown();

    // スプライト/3D共通管理は個別描画リソース破棄後、DirectXCommon より前に破棄する
    impl_->spriteCommon.reset();
    if (impl_->object3dCommon) {
        impl_->object3dCommon->Finalize();
        impl_->object3dCommon.reset();
    }

    // TextureManager の終了処理を呼び出す
    TextureManager::GetInstance()->Finalize();

    // 管理中のオフスクリーンレンダーターゲットをSRV管理の終了前に破棄する
    DirectXCommon::GetInstance()->DestroyAllRenderTargets();

    // SrvManager の終了処理を呼び出す
    impl_->srvManager.Finalize();

    // モデルマネージャの終了処理を呼び出す
    ModelManager::GetInstance()->Finalize();

    // DirectXCommon の終了処理を呼び出す
    DirectXCommon::GetInstance()->Finalize();

    // リークチェッカーは DirectX のリソース解放後、かつ COM がまだ有効なうちに破棄する

    // InputManager の終了処理を呼び出す
    InputManager::GetInstance()->Finalize();

    // DirectInput のリセット
    impl_->directInput.Reset();

    // WinApp の終了処理を呼び出す
    impl_->winApp.Finalize();

    // 基底クラスの終了処理を呼び出す
    Framework::Finalize();
}

/// <summary>
/// ゲームの終了要求があったかどうかを返す
/// </summary>
/// <returns></returns>
bool Game::IsEndRequest() const
{
    // Impl が存在する場合は endRequested フラグの値を返し、存在しない場合は true を返す（終了要求があったとみなす）
    return impl_ ? impl_->endRequested : true;
}

/// <summary>
/// ウィンドウメッセージの処理を行い、終了要求があったかどうかを返す。WM_QUIT が検出された場合は false を返す設計になっている
/// </summary>
/// <returns></returns>
bool Game::PollEvents()
{
    // Impl が存在しない場合は false を返す（終了要求があったとみなす）
    if (!impl_) {
        return false;
    }

    // WinApp の ProcessMessage を呼び出して、ウィンドウメッセージの処理を行う。WM_QUIT が検出された場合は false を返す設計になっている
    return impl_->winApp.ProcessMessage();
}
