#include "Game.h"
#include <Windows.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
// #include <format>
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include "externals/imgui/imgui.h"
#include "mathUtility.h"
#include <cassert>
#include <cmath>
#include <d3d12.h>
#include <dbghelp.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
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
#include "engine/base/SrvManager.h"
#include "engine/sound/Sound.h"
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

    ParticleEmitter pmEmitter; // パーティクルエミッタ

    DrawType selectedDrawType = DRAW_SPHERE; // 描画する内容の種類を選択するための変数

    DebugCamera debugCamera; // デバッグカメラ
    bool isDebugCameraControl = true; // デバッグカメラ操作フラグ
    bool useBillboard = true; // ビルボードの使用フラグ

    ImGuiManager imguiManager; // ImGui管理
    std::unique_ptr<D3DResourceLeakChecker> leakChecker; // D3D リソースリークチェッカ
};

// Game クラスのデストラクタでは、動的に確保した Impl を解放する
Game::~Game()
{
    if (impl_)
    {
        delete impl_;
        impl_ = nullptr;
    }
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
        impl_ = new Impl();
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
        debugController->SetEnableGPUBasedValidation(TRUE); // GPUベースのバリデーションを有効にする
    }

#endif

    // D3Dリソースリークチェッカーのインスタンスを作成（Impl のメンバとして保持）
    impl_->leakChecker = std::make_unique<D3DResourceLeakChecker>();

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
    // DirectXCommonの初期化
    DirectXCommon::GetInstance()->Initialize(&impl_->winApp);

    // SpriteCommonのインスタンスを作成して初期化
    impl_->spriteCommon = std::make_unique<SpriteCommon>();
    impl_->spriteCommon->Initialize(DirectXCommon::GetInstance());

    // SrvManagerの初期化
    impl_->srvManager.Initialize(DirectXCommon::GetInstance());

    // TextureManagerの初期化
    TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), &impl_->srvManager);

    // Object3dCommonのインスタンスを作成して初期化
    impl_->object3dCommon = std::make_unique<Object3dCommon>();
    impl_->object3dCommon->Initialize(DirectXCommon::GetInstance());

    // カメラの生成と初期設定
    impl_->camera = std::make_unique<Camera>();
    impl_->camera->SetRotate({ -0.1f, 0.0f, 0.0f });
    impl_->camera->SetTranslate({ 0.0f, 1.0f, -20.0f });
    impl_->camera->Update();
    // Object3dCommon にデフォルトカメラをセット
    impl_->object3dCommon->SetDefaultCamera(impl_->camera.get());

    // シーンで使用するテクスチャをロード
    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
    TextureManager::GetInstance()->LoadTexture("resources/circle.png");

    // シーン用スプライトの生成
    const uint32_t kSpriteCount = 5; // 生成するスプライトの数
    // スプライトに使用するテクスチャのファイルパスを配列で定義
    std::array<std::string, 2> spriteNames = {
        "resources/uvChecker",
        "resources/monsterBall",
    };

    // 既存のスプライトがあればクリアする
    impl_->sprites.clear();

    // kSpriteCount の数だけスプライトを生成し、交互にテクスチャを設定してリストに追加する
    for (uint32_t i = 0; i < kSpriteCount; ++i) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(impl_->spriteCommon.get(), spriteNames[(i / 2) == 0 ? 0 : 1] + ".png");
        impl_->sprites.push_back(std::move(sprite));
    }

    // シーン用 3D オブジェクトの生成
    const uint32_t kObject3DCount = 6; // 生成する3Dオブジェクトの数
    // 3Dオブジェクトに使用するモデルファイルの名前を配列で定義
    std::vector<std::string> modelFileNames = {
        "plane.gltf",
        "bunny.obj",
        "teapot.obj",
        "models/fence/fence.obj",
        "models/sphere/sphere.gltf",
        "models/terrain/terrain.obj"
    };

    // 既存の3Dオブジェクトがあればクリアする
    impl_->objects3d.clear();

    // kObject3DCount の数だけ3Dオブジェクトを生成し、モデルファイルを交互に設定してリストに追加する
    for (uint32_t i = 0; i < kObject3DCount; ++i) {
        auto obj = std::make_unique<Object3d>();
        obj->Initialize(impl_->object3dCommon.get());
        std::string modelFile = modelFileNames.empty() ? std::string("plane.obj") : modelFileNames[i % modelFileNames.size()];
        obj->SetModel(modelFile);
        // モデルファイル名に "fence" が含まれている場合は、アルファカットアウト用のサンプラーを使用するフラグを設定する
        if (modelFile.find("fence") != std::string::npos) {
            obj->SetUseAlphaCutoutSampler(true);
        }
        impl_->objects3d.push_back(std::move(obj));
    }

    // パーティクル描画用のプレーンを生成してテクスチャを設定
    impl_->particlePlane = std::make_unique<Object3d>();
    impl_->particlePlane->Initialize(impl_->object3dCommon.get());
    impl_->particlePlane->SetModel("plane.obj");
    impl_->particlePlane->SetTexture("resources/circle.png");

    ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), impl_->object3dCommon.get(), &impl_->srvManager, TextureManager::GetInstance());
    ParticleManager::GetInstance()->SetParticlePlane(impl_->particlePlane.get());
    // パーティクルグループの作成とテクスチャの割り当て
    ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/circle.png");
    ParticleManager::GetInstance()->CreateParticleGroup("Checker", "resources/uvChecker.png");
    ParticleManager::GetInstance()->CreateParticleGroup("Ball", "resources/monsterBall.png");

    MathUtility math; // MathUtility のインスタンスを作成して使用する

    // ライトの設定
    DirectionalLight* directionalLightData = impl_->object3dCommon->GetDirectionalLightData();
    // directionalLightData が存在する場合は、ライトの強度、色、方向を設定する
    if (directionalLightData) {
        directionalLightData->intensity = 0.05f;
        directionalLightData->color = { 0.2f, 0.25f, 0.3f, 1.0f };
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
            sl->direction = math.Normalize({ -1.0f, -1.0f, 0.0f });
            sl->decay = 2.0f;
            sl->cosAngle = cosf(3.14159265358979323846f / 3.0f);
            sl->cosFalloffStart = cosf(3.14159265358979323846f / 2.0f);
            sl->enabled = 1;
        }
    }

    TextureManager::GetInstance()->ExecuteResourceUpload(); // テクスチャのGPUへの転送を実行

    // デバッグログ出力: 読み込まれたテクスチャ情報をログに出す
    {
        uint32_t count = TextureManager::GetInstance()->GetLoadedTextureCount();
        char buf[256];
        sprintf_s(buf, "Debug: Loaded texture count = %u\n", count);
        Logger::Log(buf);
        for (uint32_t ti = 0; ti < count; ++ti) {
            auto meta = TextureManager::GetInstance()->GetMetadata(ti);
            auto handle = TextureManager::GetInstance()->GetSrvHandleGPU(ti);
            std::ostringstream oss;
            oss << "Debug: Texture[" << ti << "] size=" << meta.width << " x " << meta.height
                << " format=" << static_cast<int>(meta.format)
                << " srv.ptr=0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << handle.ptr;
            Logger::Log(oss.str());
        }
    }

    // パーティクルエミッタの初期設定
    impl_->pmEmitter.groupName = "Circle"; // 使用するパーティクルグループの名前を設定
    impl_->pmEmitter.transform.translate = { 0.0f, 0.0f, 0.0f }; // エミッタの位置を設定
    impl_->pmEmitter.count = 3; // 1回あたりの発生数を設定
    impl_->pmEmitter.frequency = 0.5f; // 発生間隔を設定（0.5秒ごとに発生）

    // 初期状態でいくつかパーティクルを発生させておく
    for (int i = 0; i < 20; ++i) {
        impl_->pmEmitter.Emit();
    }

    // デバッグカメラの初期化（ウィンドウ解像度を指定）
    impl_->debugCamera.Initialize(1280.0f, 720.0f);

    // ImGuiManagerの初期化
    impl_->imguiManager.Initialize(impl_->hwnd, &impl_->srvManager);

    // 初期化完了フラグを立てる
    impl_->initialized = true;
    impl_->endRequested = false;

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
    if (impl_->isDebugCameraControl) {
        long deltaX = InputManager::GetInstance()->GetMouseDeltaX(); // 前フレームからのマウスのX移動量を取得
        long deltaY = InputManager::GetInstance()->GetMouseDeltaY(); // 前フレームからのマウスのY移動量を取得
        long wheelDelta = InputManager::GetInstance()->GetMouseDeltaZ(); // 前フレームからのマウスホイールの移動量を取得

        // ImGui のウィンドウやアイテムがアクティブでない場合にのみ、マウスドラッグとホイールの入力をデバッグカメラの操作に使用する
        if (!ImGui::IsAnyItemActive() && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
            impl_->debugCamera.OnMouseDrag(float(deltaX), float(deltaY));
            impl_->debugCamera.OnMouseWheel(float(wheelDelta));
        }

        // デバッグカメラの更新を行う
        impl_->debugCamera.Update();
    }

    // スペースキーが押された瞬間にサウンドを再生する。スペースキーが離された瞬間は再生しないようにする
    if (InputManager::GetInstance()->IsKeyJustPressed(DIK_SPACE) && !InputManager::GetInstance()->IsKeyJustReleased(DIK_SPACE)) {
        impl_->soundSystem.Play(impl_->soundData1); // サウンドシステムを使用してサウンドデータを再生する
    }

    // ImGui UI is built once per frame in Draw() to avoid calling NewFrame() multiple times during fixed-step updates.

    // 固定タイムステップで更新（ここでは 1/60 秒固定）
    const float dt = 1.0f / 60.0f;
    // パーティクルエミッタの Update を呼び出して、パーティクルの発生や状態の更新を行う
    impl_->pmEmitter.Update(dt);
    auto* pm = ParticleManager::GetInstance();
    // ParticleManager の Update を呼び出して、すべてのパーティクルの状態の更新や寿命の管理を行う
    pm->Update(dt);
    // SoundSystem の Poll を呼び出して、サウンドの再生状態の更新やリソースの管理を行う
    impl_->soundSystem.Poll();

    // 3Dオブジェクトのワールド行列、ビュー行列、プロジェクション行列を計算して、各オブジェクトの Update を呼び出す
    MathUtility math;
    Matrix4x4 worldMatrix = math.MakeAffineMatrix(impl_->transform.scale, impl_->transform.rotate, impl_->transform.translate);
    if (impl_->camera) {
        impl_->camera->Update();
    }
    Matrix4x4 viewMatrix = impl_->camera ? impl_->camera->GetViewMatrix() : impl_->debugCamera.GetViewMatrix();
    Matrix4x4 projectionMatrix = impl_->camera ? impl_->camera->GetProjectionMatrix() : impl_->debugCamera.GetProjectionMatrix();
    Matrix4x4 worldViewProjectionMatrix = math.Multiply(worldMatrix, math.Multiply(viewMatrix, projectionMatrix));

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
    std::vector<Object3d*> objPtrs;
    objPtrs.reserve(impl_->objects3d.size());
    for (auto& u : impl_->objects3d) objPtrs.push_back(u.get());
    ctx.objects3d = &objPtrs;
    // SpriteCommon のポインタをセットして、UIでスプライトの共通設定や情報にアクセスできるようにする
    std::vector<Sprite*> spritePtrs;
    spritePtrs.reserve(impl_->sprites.size());
    for (auto& u : impl_->sprites) spritePtrs.push_back(u.get());
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
    // ImGuiManager の BuildUI を呼び出して、UIの構築を行う。これにより、UIが描画される準備が整う
    impl_->imguiManager.BuildUI(ctx);

    // 描画前の共通処理を呼び出す（バックバッファのクリアやコマンドリストの開始など）
    DirectXCommon::GetInstance()->PreDraw();
    // SrvManager の PreDraw を呼び出して、描画に必要なシェーダーリソースビューのセットアップを行う
    impl_->srvManager.PreDraw();

    MathUtility math; // MathUtility のインスタンスを作成して使用する

    // 描画する内容の種類に応じて、適切な描画処理を行うための switch 文
    switch (impl_->selectedDrawType) {

    case DRAW_PARTICLE: // パーティクル描画

        // パーティクル描画の前に、Object3dCommon にビルボード用のカメラ情報をセット
        if (impl_->object3dCommon) {
            // デバッグカメラのビュー行列とプロジェクション行列を取得して、ビルボード描画に必要な情報を計算する
            Matrix4x4 view = impl_->debugCamera.GetViewMatrix();
            Matrix4x4 proj = impl_->debugCamera.GetProjectionMatrix();
            Matrix4x4 vp = math.Multiply(view, proj);

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
        Matrix4x4 vp = math.Multiply(view, proj);

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
