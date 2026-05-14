#include "Game.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#if 0 // imgui includes centralized in ImGuiManager.h
#endif
#include "mathUtility.h"
#include <Windows.h>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <d3d12.h>
#include <dbghelp.h>
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
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include "Camera.h"
#include "D3DResourceLeakChecker.h"
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
#include "engine/base/SceneManager.h"
#include "engine/base/SrvManager.h"
#include "engine/sound/Sound.h"
#include "project/scenes/SceneFactory.h"
#include <dinput.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dinput8.lib")

using namespace MyEngine;

// （前方宣言を削除し、詳細な Impl を下に定義）

// Game クラスのコンストラクタでは、Impl ポインタを nullptr に初期化する
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

    DrawType selectedDrawType = DRAW_SPHERE; // 描画する内容の種類を選択するための変数

    DebugCamera debugCamera; // デバッグカメラ (global DebugCamera)
    bool isDebugCameraControl = true; // デバッグカメラ操作フラグ
    bool useBillboard = true; // ビルボードの使用フラグ
    bool useDebugCameraForRender = false; // レンダリングにデバッグカメラを使うか

    ImGuiManager imguiManager; // ImGui管理
    std::unique_ptr<D3DResourceLeakChecker> leakChecker; // D3D リソースリークチェッカ
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
/// ゲームの初期化処理
/// </summary>
bool Game::Initialize(HINSTANCE hInstance, int nCmdShow)
{
    // 基底クラスの初期化を呼び出す。失敗したら false を返す
    if (!Framework::Initialize(hInstance, nCmdShow)) {
        return false;
    }

    // Impl がまだ存在しない場合は新たに作成する
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }

    // COMの初期化
    CoInitializeEx(0, COINIT_MULTITHREADED);
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

    // ログファイルの設定: "logs" フォルダに日付と時刻を含むファイル名でログを出力する
    std::filesystem::create_directory("logs"); // "logs" フォルダが存在しない場合は作成する
    std::time_t now_c = std::time(nullptr); // 現在の時刻を取得
    struct tm local_tm; // ローカルタイムに変換するための構造体
    localtime_s(&local_tm, &now_c); // 現在の時刻をローカルタイムに変換
    char dateBuf[32]; // 日付と時刻を "YYYYMMDD_HHMMSS" 形式の文字列にフォーマットするためのバッファ
    std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d_%H%M%S", &local_tm); // 日付と時刻を "YYYYMMDD_HHMMSS" 形式の文字列にフォーマット
    std::string dateString(dateBuf); // フォーマットした日付と時刻を std::string に変換
    std::string logFilePath = std::string("logs/") + dateString + ".log"; // ログファイルのパスを "logs/YYYYMMDD_HHMMSS.log" 形式で作成
    Logger::SetLogFile(logFilePath); // ログファイルのパスを Logger に設定
    Logger::SetErrorLogFile(logFilePath); // エラーログファイルのパスも同じログファイルに設定

    // ウィンドウを作成してハンドルを取得
    impl_->winApp.Initialize(hInstance, nCmdShow, L"GE3_LE2B_15_タカハシ_ユキト");
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

    // D3Dリソースリークチェッカはドライバの相互作用で不安定になる環境があるため
    // 一時的に注入を無効化する（必要なら手動で有効化してください）
#if 0
    impl_->leakChecker = std::make_unique<D3DResourceLeakChecker>();
#endif

    // サウンドシステムからサウンドデータを読み込む
    impl_->soundData1 = impl_->soundSystem.LoadFromFile("resources/mokugyo.wav");

    // DirectInput を初期化
    HRESULT result = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, reinterpret_cast<void**>(impl_->directInput.GetAddressOf()), nullptr);
    // DirectInput8Create を呼び出して DirectInput インターフェースを取得し、失敗した場合はエラーログを出力して初期化を終了する
    if (FAILED(result)) {
        Logger::Log("Error: DirectInput8Create failed.\n");
        impl_->winApp.Finalize();
        CoUninitialize();
        return false;
    }

    // InputManagerの初期化
    InputManager::GetInstance()->Initialize(impl_->directInput.Get(), impl_->hwnd);

    // スプライト共通管理の一時的なユニークポインタ
    std::unique_ptr<SpriteCommon> spriteCommonTmp;
    // 3Dオブジェクト共通管理の一時的なユニークポインタ
    std::unique_ptr<Object3dCommon> object3dCommonTmp;

    // エンジンの初期化処理を呼び出す。失敗した場合はエラーログを出力して初期化を終了する
    if (!Framework::InitializeEngine(hInstance, &impl_->winApp, impl_->hwnd,
            spriteCommonTmp, impl_->srvManager, object3dCommonTmp)) {
        Logger::Log("Error: Framework::InitializeEngine failed\n");
        impl_->winApp.Finalize();
        CoUninitialize();
        return false;
    }

    // 共通管理オブジェクトを Impl に移動
    
    // スプライト共通管理を Impl に移動
    impl_->spriteCommon = std::move(spriteCommonTmp);
    // 3Dオブジェクト共通管理を Impl に移動
    impl_->object3dCommon = std::move(object3dCommonTmp);

    // カメラの生成と初期設定
    impl_->camera = std::make_unique<Camera>();
    impl_->camera->SetRotate({ -0.1f, 0.0f, 0.0f });
    impl_->camera->SetTranslate({ 0.0f, 1.0f, -20.0f });
    impl_->camera->Update();
    // Object3dCommon にデフォルトカメラをセット
    impl_->object3dCommon->SetDefaultCamera(impl_->camera.get());

    // ライトの設定
    Object3d::DirectionalLight* directionalLightData = impl_->object3dCommon->GetDirectionalLightData();
    // directionalLightData が存在する場合は、ライトの強度、色、方向を設定する
    if (directionalLightData) {
        // ライトの強度を 1.0f に設定
        directionalLightData->intensity = 1.0f;
        directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
        directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
    }

    // 点光源の設定
    if (impl_->object3dCommon) {
        Object3d::PointLight pl = {};
        pl.position = { 0.0f, 1.5f, 0.0f, 0.0f };
        pl.color = { 1.0f, 1.0f, 1.0f, 6.0f };
        pl.radius = 6.0f;
        pl.decay = 2.0f;
        pl.enabled = 1;
        impl_->object3dCommon->AddPointLight(pl);
    }

    // スポットライトの設定
    if (impl_->object3dCommon) {
        auto sl = impl_->object3dCommon->GetSpotLightData();
        if (sl) {
            sl->position = { 2.0f, 1.25f, -3.0f, 0.0f };
            sl->color = { 1.0f, 1.0f, 1.0f, 4.0f };
            sl->distance = 7.0f;
            sl->direction = MathUtil::Normalize({ -1.0f, -1.0f, 0.0f });
            sl->decay = 2.0f;
            sl->cosAngle = cosf(3.14159265358979323846f / 3.0f);
            sl->cosFalloffStart = cosf(3.14159265358979323846f / 2.0f);
            sl->enabled = 1;
        }
    }

    // デバッグカメラの初期化（ウィンドウ解像度を指定）
    impl_->debugCamera.Initialize(1280.0f, 720.0f);

    // Object3dCommon にデバッグカメラをセットして、UI側で編集できるようにする
    if (impl_->object3dCommon) {
        impl_->object3dCommon->SetDebugCamera(&impl_->debugCamera);
        impl_->object3dCommon->SetUseDebugCameraForRender(impl_->useDebugCameraForRender);
        impl_->object3dCommon->SetEnableDebugCameraInput(impl_->isDebugCameraControl);
    }

    // ImGuiManagerの初期化
    impl_->imguiManager.Initialize(impl_->hwnd, &impl_->srvManager);

    // 初期化完了フラグを立てる
    impl_->initialized = true;
    impl_->endRequested = false;

    // シーンマネージャ初期化と初期シーン設定
    impl_->sceneManager = std::make_unique<SceneManager>();
    impl_->sceneManager->Initialize();
    // SceneContext を構築して SceneManager に渡す
    SceneContext sctx;
    sctx.object3dCommon = impl_->object3dCommon.get();
    sctx.spriteCommon = impl_->spriteCommon.get();
    sctx.camera = impl_->camera.get();
    sctx.selectedDrawType = static_cast<int>(impl_->selectedDrawType);
    sctx.particleManager = ParticleManager::GetInstance();
    sctx.textureManager = TextureManager::GetInstance();
    sctx.srvManager = &impl_->srvManager;
    sctx.directXCommon = DirectXCommon::GetInstance();
    // Provide ImGuiManager to scenes so they can register UI callbacks for objects
    sctx.imguiManager = &impl_->imguiManager;
    impl_->sceneManager->SetContext(sctx);

    auto initial = GameApp::SceneFactory::Create("Title");
    if (initial) {
        impl_->sceneManager->ChangeScene(std::move(initial));
    }

    // 初期化が成功したので true を返す
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
    // Query input-enabled flag from Object3dCommon if available
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
        if (InputManager::GetInstance()->IsMouseButtonPressed(0) ||
            InputManager::GetInstance()->IsMouseButtonPressed(1) ||
            InputManager::GetInstance()->IsMouseButtonPressed(2)) {
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
                    const float rotateSpeed = 0.01f;
                    Vector3 crot = impl_->camera->GetRotate();
                    crot.y += float(deltaX) * rotateSpeed;
                    crot.x += float(deltaY) * rotateSpeed;
                    impl_->camera->SetRotate(crot);

                    // ホイールで前後移動（ズーム）
                    const float zoomSpeed = 0.1f;
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

        // タイトル画面にいるときはスペースでプレイシーンへ切り替える
        if (impl_->sceneManager) {
            try {
                const std::string cur = impl_->sceneManager->GetCurrentSceneName();
                if (cur == "Title") {
                    // pending にセットして後続の処理で安全に切り替える
                    impl_->pendingSceneName = "Play";
                }
            }
            catch (...) {
                // 念のため例外は握り潰す（GetCurrentSceneName が例外を投げる想定は低いが安全措置）
            }
        }
    }

    // 固定タイムステップで更新（ここでは 1/60 秒固定）
    const float dt = 1.0f / 60.0f;
    // パーティクルのインスタンス生成
    auto* pm = ParticleManager::GetInstance();
    // ParticleManager の Update を呼び出して、すべてのパーティクルの状態の更新や寿命の管理を行う
    pm->Update(dt);
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
            char buf[128];
            sprintf_s(buf, "Debug camera for render: %s\n", !cur ? "ON" : "OFF");
            Logger::Log(buf);
        } else {
            impl_->useDebugCameraForRender = !impl_->useDebugCameraForRender;
            char buf[128];
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
        // カメラのワールド位置を取得してセット
        if (cam) {
            cam->worldPosition = impl_->debugCamera.GetTranslation();
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
    // 描画する内容の種類を選択するための変数へのポインタをセットして、UIで描画内容の切り替えができるようにする
    ctx.selectedDrawType = reinterpret_cast<int*>(&impl_->selectedDrawType);
    // ビルボードの使用フラグへのポインタをセットして、UIでビルボードのオンオフができるようにする
    ctx.useBillboard = &impl_->useBillboard;
    // ParticleManager のポインタをセットして、UIでパーティクルの情報や設定にアクセスできるようにする
    ctx.particleManager = ParticleManager::GetInstance();
    // デルタタイムをセットして、UIでフレームごとの時間の情報にアクセスできるようにする
    ctx.dt = 1.0f / 60.0f;
    ctx.useDebugCameraForRender = &impl_->useDebugCameraForRender;
    // シーン名を ImGui に渡す
    if (impl_->sceneManager) {
        static std::string sname;
        sname = impl_->sceneManager->GetCurrentSceneName();
        ctx.currentSceneName = sname.c_str();
        // シーン切替要求のコールバックを渡す
        ctx.requestSceneChange = [this](const char* name) {
            if (!impl_ || !impl_->sceneManager)
                return;
            impl_->pendingSceneName = std::string(name);
        };
    }
    // ImGuiManager の BuildUI を呼び出して、UIの構築を行う。これにより、UIが描画される準備が整う
    impl_->imguiManager.BuildUI(ctx);

    // BuildUI 内でシーン切替要求があった場合は、ctx.requestSceneChange を通じて impl_->pendingSceneName にシーン名がセットされる
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

    // 描画前の共通処理を呼び出す（バックバッファのクリアやコマンドリストの開始など）
    DirectXCommon::GetInstance()->PreDraw();
    // SrvManager の PreDraw を呼び出して、描画に必要なシェーダーリソースビューのセットアップを行う
    impl_->srvManager.PreDraw();

    // シーン側にも現在の描画モードを伝えて、シーン自身が必要な要素だけ描画できるようにする
    if (impl_->sceneManager) {
        impl_->sceneManager->SetSelectedDrawType(static_cast<int>(impl_->selectedDrawType));
        impl_->sceneManager->Draw();
    }

    // Game 側の既存描画を常に行う（シーンの描画とは併行して実行されます）
    // 描画する内容の種類に応じて、適切な描画処理を行うための switch 文
    switch (impl_->selectedDrawType) {

    case DRAW_PARTICLE: // パーティクル描画

        // パーティクル描画の前に、Object3dCommon にビルボード用のカメラ情報をセット
        if (impl_->object3dCommon) {
            // デバッグカメラのビュー行列とプロジェクション行列を取得して、ビルボード描画に必要な情報を計算する
            Matrix4x4 view = impl_->debugCamera.GetViewMatrix();
            Matrix4x4 proj = impl_->debugCamera.GetProjectionMatrix();
            Matrix4x4 vp = MathUtil::Multiply(view, proj);

            // ビルボードの描画に必要なカメラの右ベクトルと上ベクトルをビュー行列から抽出してセット
            Vector3 right = { view.m[0][0], view.m[1][0], view.m[2][0] };
            Vector3 up = { view.m[0][1], view.m[1][1], view.m[2][1] };

            // Object3dCommon にビルボード用のカメラ情報をセットして、ビルボード描画の準備をする
            impl_->object3dCommon->SetBillboardCameraWithVP(right, up, vp, impl_->useBillboard);

            // ParticleManager の Draw を呼び出して、すべてのパーティクルを描画する
            ParticleManager::GetInstance()->Draw();
        }

        break;

    case DRAW_MODEL: // モデル描画

        // モデル描画の前に、Object3dCommon に共通の描画設定をセット
        impl_->object3dCommon->SetCommonDrawSetting();
        // 3Dオブジェクトのリストの最初のオブジェクトが存在する場合は、そのオブジェクトの Draw を呼び出して描画する
        if (impl_->objects3d.size() > 0 && impl_->objects3d[0]) {
            impl_->objects3d[0]->Draw();
        }

        break;

    case DRAW_SPRITE:

        // スプライト描画の前に、SpriteCommon に共通の描画設定をセット
        impl_->spriteCommon->SetCommonDrawSetting();
        // スプライトのリストをループして、各スプライトの Draw を呼び出して描画する
        for (uint32_t i = 0; i < impl_->sprites.size(); i++) {
            impl_->sprites[i]->Draw();
        }

        break;

    case DRAW_BUNNY: // バニー描画

        // バニー描画の前に、Object3dCommon に共通の描画設定をセット
        impl_->object3dCommon->SetCommonDrawSetting();
        // 3Dオブジェクトのリストの2番目のオブジェクトが存在する場合は、そのオブジェクトの Draw を呼び出して描画する
        if (impl_->objects3d.size() > 1 && impl_->objects3d[1]) {
            impl_->objects3d[1]->Draw();
        }

        break;

    case DRAW_FENCE: // フェンス描画

        // フェンス描画の前に、Object3dCommon に共通の描画設定をセット
        impl_->object3dCommon->SetCommonDrawSetting();
        // 3Dオブジェクトのリストの4番目のオブジェクトが存在する場合は、そのオブジェクトの Draw を呼び出して描画する
        if (impl_->objects3d.size() > 3 && impl_->objects3d[3]) {
            impl_->objects3d[3]->Draw();
        }

        break;

    case DRAW_CHECKER: // チェッカー描画

        // チェッカー描画の前に、Object3dCommon に共通の描画設定をセット
        impl_->object3dCommon->SetCommonDrawSetting();
        // 3Dオブジェクトのリストの3番目のオブジェクトが存在する場合は、そのオブジェクトの Draw を呼び出して描画する
        if (impl_->objects3d.size() > 2 && impl_->objects3d[2]) {
            impl_->objects3d[2]->Draw();
        }

        break;

    case DRAW_SPHERE: // 球描画

        // 球描画の前に、Object3dCommon に共通の描画設定をセット
        impl_->object3dCommon->SetCommonDrawSetting();
        // 3Dオブジェクトのリストの5番目が存在する場合は、そのオブジェクトの Draw を呼び出して描画する
        if (impl_->objects3d.size() > 4 && impl_->objects3d[4]) {
            impl_->objects3d[4]->Draw();
        }
        // 3Dオブジェクトのリストの6番目が存在する場合は、そのオブジェクトの Draw を呼び出して描画する
        if (impl_->objects3d.size() > 5 && impl_->objects3d[5]) {
            impl_->objects3d[5]->Draw();
        }

        break;

    case DRAW_ALL: // すべて描画

        // すべて描画の前に、Object3dCommon に共通の描画設定をセット
        impl_->object3dCommon->SetCommonDrawSetting();
        // 3Dオブジェクトのリストをループして、存在するオブジェクトの Draw を呼び出して描画する
        for (auto& obj : impl_->objects3d) {
            // obj が存在する場合は、そのオブジェクトの Draw を呼び出して描画する
            if (obj) {
                obj->Draw();
            }
        }

        // パーティクル描画の前に、Object3dCommon にビルボード用のカメラ情報をセット
        Matrix4x4 view = impl_->debugCamera.GetViewMatrix();
        Matrix4x4 proj = impl_->debugCamera.GetProjectionMatrix();
        Matrix4x4 vp = MathUtil::Multiply(view, proj);

        // ビルボードの描画に必要なカメラの右ベクトルと上ベクトルをビュー行列から抽出してセット
        Vector3 right = { view.m[0][0], view.m[1][0], view.m[2][0] };
        Vector3 up = { view.m[0][1], view.m[1][1], view.m[2][1] };

        // Object3dCommon にビルボード用のカメラ情報をセットして、ビルボード描画の準備をする
        impl_->object3dCommon->SetBillboardCameraWithVP(right, up, vp, impl_->useBillboard);

        // ParticleManager の Draw を呼び出して、すべてのパーティクルを描画する
        ParticleManager::GetInstance()->Draw();

        // スプライト描画の前に、SpriteCommon に共通の描画設定をセット
        impl_->spriteCommon->SetCommonDrawSetting();
        // スプライトのリストをループして、存在するスプライトの Draw を呼び出して描画する
        for (uint32_t i = 0; i < impl_->sprites.size(); i++) {
            impl_->sprites[i]->Draw();
        }

        break;
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

    CloseWindow(impl_->hwnd); // ウィンドウを閉じる

    // 終了処理の順番に注意して、リソースの解放やマネージャの終了処理を行う

    // 3Dオブジェクトのリストをクリアして、Object3dCommon のリセットを行う
    impl_->objects3d.clear();
    impl_->object3dCommon.reset();

    // TextureManager の終了処理を呼び出す
    TextureManager::GetInstance()->Finalize();

    // ImGuiManager の終了処理を呼び出す
    impl_->imguiManager.Shutdown();

    // SrvManager の終了処理を呼び出す
    impl_->srvManager.Finalize();

    // モデルマネージャの終了処理を呼び出す
    ModelManager::GetInstance()->Finalize();

    // スプライトのリストをクリアして、SpriteCommon のリセットを行う
    impl_->spriteCommon.reset();
    impl_->sprites.clear();

    // DirectXCommon の終了処理を呼び出す
    DirectXCommon::GetInstance()->Finalize();

    // リークチェッカーは DirectX のリソース解放後、かつ COM がまだ有効なうちに破棄する
    if (impl_->leakChecker) {
        impl_->leakChecker.reset();
    }

    // サウンドデータのリセット
    impl_->soundData1.reset();

    // InputManager の終了処理を呼び出す
    InputManager::GetInstance()->Finalize();

    // DirectInput のリセット
    impl_->directInput.Reset();

    // WinApp の終了処理を呼び出す
    impl_->winApp.Finalize();

    // COM のクリーンアップ
    // リークチェッカーは COM が有効なうちに破棄する
    if (impl_->leakChecker) {
        impl_->leakChecker.reset();
    }

    // シーンマネージャーの終了処理を呼び出す
    if (impl_->sceneManager) {
        impl_->sceneManager->Finalize();
        impl_->sceneManager.reset();
    }

    CoUninitialize();

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
