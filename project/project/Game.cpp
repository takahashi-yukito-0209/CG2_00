// Game クラスの実装: 元の WinMain の処理をここに移行してクラス化する
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

Game::Game()
    : impl_(nullptr)
{
}
Game::~Game()
{
    if (impl_)
        delete impl_;
}

// Impl で使用する補助列挙型の前方宣言
enum DrawType { DRAW_MODEL,
    DRAW_PARTICLE,
    DRAW_SPRITE,
    DRAW_BUNNY,
    DRAW_FENCE,
    DRAW_CHECKER,
    DRAW_SPHERE,
    DRAW_ALL };

// Impl は実行時の状態を保持し、Game の処理を分割可能にする
struct Game::Impl {
    // basic runtime flags
    bool initialized = false;
    bool endRequested = false;

    // global transform for the scene (used when constructing world matrix)
    Transform transform = {};

    // window / input / sound
    MyEngine::WinApp winApp;
    HWND hwnd = nullptr;
    Microsoft::WRL::ComPtr<IDirectInput8> directInput;
    MyEngine::SoundSystem soundSystem;
    std::shared_ptr<MyEngine::SoundClip> soundData1;

    // core systems
    std::unique_ptr<SpriteCommon> spriteCommon;
    MyEngine::SrvManager srvManager;
    std::unique_ptr<Object3dCommon> object3dCommon;

    std::unique_ptr<Camera> camera;

    // scene
    std::vector<std::unique_ptr<Sprite>> sprites;
    std::vector<std::unique_ptr<Object3d>> objects3d;
    std::unique_ptr<Object3d> particlePlane;

    // misc
    ParticleEmitter pmEmitter;
    DrawType selectedDrawType = DRAW_SPHERE;

    DebugCamera debugCamera;
    bool isDebugCameraControl = true;
    bool useBillboard = true;

    MyEngine::ImGuiManager imguiManager;
};

// すべてのサブシステムを初期化し、初期シーンを構築する
// 戻り値: 成功なら true、失敗なら false
bool Game::Initialize(HINSTANCE hInstance, int nCmdShow)
{
    // 基底クラスの初期化処理を先に呼ぶ
    if (!Framework::Initialize(hInstance, nCmdShow))
        return false;

    // 初回 Initialize 時に impl を動的確保
    if (!impl_)
        impl_ = new Impl();
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
    });

    std::filesystem::create_directory("logs");
    std::time_t now_c = std::time(nullptr);
    struct tm local_tm;
    localtime_s(&local_tm, &now_c);
    char dateBuf[32];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d_%H%M%S", &local_tm);
    std::string dateString(dateBuf);
    std::string logFilePath = std::string("logs/") + dateString + ".log";
    Logger::SetLogFile(logFilePath);
    Logger::SetErrorLogFile(logFilePath);

    // ウィンドウを作成してハンドルを取得
    impl_->winApp.Initialize(hInstance, nCmdShow, L"GE3_LE2B_15_タカハシ_ユキト");
    impl_->hwnd = impl_->winApp.GetHwnd();

#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif

    D3DResourceLeakChecker leakCheck;

    // サウンドシステムからサウンドデータを読み込む
    impl_->soundData1 = impl_->soundSystem.LoadFromFile("resources/mokugyo.wav");

    // DirectInput を初期化
    HRESULT result = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, reinterpret_cast<void**>(impl_->directInput.GetAddressOf()), nullptr);
    if (FAILED(result)) {
        Logger::Log("Error: DirectInput8Create failed.\n");
        impl_->winApp.Finalize();
        CoUninitialize();
        return false;
    }

    InputManager::GetInstance()->Initialize(impl_->directInput.Get(), impl_->hwnd);
    DirectXCommon::GetInstance()->Initialize(&impl_->winApp);

    // コアシステムの初期化: SpriteCommon, SrvManager, TextureManager, Object3dCommon など
    impl_->spriteCommon = std::make_unique<SpriteCommon>();
    impl_->spriteCommon->Initialize(DirectXCommon::GetInstance());
    impl_->srvManager.Initialize(DirectXCommon::GetInstance());
    TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), &impl_->srvManager);
    impl_->object3dCommon = std::make_unique<Object3dCommon>();
    impl_->object3dCommon->Initialize(DirectXCommon::GetInstance());

    impl_->camera = std::make_unique<Camera>();
    impl_->camera->SetRotate({ -0.1f, 0.0f, 0.0f });
    impl_->camera->SetTranslate({ 0.0f, 1.0f, -20.0f });
    impl_->camera->Update();
    impl_->object3dCommon->SetDefaultCamera(impl_->camera.get());

    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
    TextureManager::GetInstance()->LoadTexture("resources/circle.png");

    // シーン用スプライトの生成
    const uint32_t kSpriteCount = 5;
    std::array<std::string, 2> spriteNames = {
        "resources/uvChecker",
        "resources/monsterBall",
    };
    impl_->sprites.clear();
    for (uint32_t i = 0; i < kSpriteCount; ++i) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(impl_->spriteCommon.get(), spriteNames[(i / 2) == 0 ? 0 : 1] + ".png");
        impl_->sprites.push_back(std::move(sprite));
    }

    // シーン用 3D オブジェクトの生成
    const uint32_t kObject3DCount = 6;
    std::vector<std::string> modelFileNames = { "plane.obj", "bunny.obj", "teapot.obj", "models/fence/fence.obj", "models/sphere/sphere.obj", "models/terrain/terrain.obj" };
    impl_->objects3d.clear();
    for (uint32_t i = 0; i < kObject3DCount; ++i) {
        auto obj = std::make_unique<Object3d>();
        obj->Initialize(impl_->object3dCommon.get());
        std::string modelFile = modelFileNames.empty() ? std::string("plane.obj") : modelFileNames[i % modelFileNames.size()];
        obj->SetModel(modelFile);
        if (modelFile.find("fence") != std::string::npos)
            obj->SetUseAlphaCutoutSampler(true);
        impl_->objects3d.push_back(std::move(obj));
    }

    // パーティクル描画用のプレーンを生成してテクスチャを設定
    impl_->particlePlane = std::make_unique<Object3d>();
    impl_->particlePlane->Initialize(impl_->object3dCommon.get());
    impl_->particlePlane->SetModel("plane.obj");
    impl_->particlePlane->SetTexture("resources/circle.png");

    ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), impl_->object3dCommon.get(), &impl_->srvManager, TextureManager::GetInstance());
    ParticleManager::GetInstance()->SetParticlePlane(impl_->particlePlane.get());
    ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/circle.png");
    ParticleManager::GetInstance()->CreateParticleGroup("Checker", "resources/uvChecker.png");
    ParticleManager::GetInstance()->CreateParticleGroup("Ball", "resources/monsterBall.png");

    MathUtility math;

    DirectionalLight* directionalLightData = impl_->object3dCommon->GetDirectionalLightData();
    if (directionalLightData) {
        directionalLightData->intensity = 0.05f;
        directionalLightData->color = { 0.2f, 0.25f, 0.3f, 1.0f };
        directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
    }

    if (impl_->object3dCommon) {
        MyEngine::Object3d::PointLight pl = {};
        pl.position = { 0.0f, 1.5f, 0.0f, 0.0f };
        pl.color = { 1.0f, 1.0f, 1.0f, 6.0f };
        pl.radius = 6.0f;
        pl.decay = 2.0f;
        pl.enabled = 1;
        impl_->object3dCommon->AddPointLight(pl);
    }

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

    TextureManager::GetInstance()->ExecuteResourceUpload();

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

    // パーティクルエミッタの初期設定と初期スポーン
    impl_->pmEmitter.groupName = "Circle";
    impl_->pmEmitter.transform.translate = { 0.0f, 0.0f, 0.0f };
    impl_->pmEmitter.count = 3;
    impl_->pmEmitter.frequency = 0.5f;
    for (int i = 0; i < 20; ++i)
        impl_->pmEmitter.Emit();

    // デバッグカメラの初期化（ウィンドウ解像度を指定）
    impl_->debugCamera.Initialize(1280.0f, 720.0f);

    impl_->imguiManager.Initialize(impl_->hwnd, &impl_->srvManager);

    // 初期化完了フラグを立てる
    impl_->initialized = true;
    impl_->endRequested = false;
    return true;
}

// 毎フレーム更新処理
// - ウィンドウメッセージ処理
// - 入力更新
// - ImGui フレーム構築
// - パーティクル等のシミュレーション更新
void Game::Update()
{
    // 基底クラスの更新処理を先に呼び出す
    Framework::Update();

    if (!impl_ || !impl_->initialized)
        return;

    // ウィンドウメッセージを処理。WM_QUIT を受けたら終了要求を立てる
    if (!impl_->winApp.ProcessMessage()) {
        impl_->endRequested = true;
        return;
    }

    // ImGui フレーム開始
    impl_->imguiManager.NewFrame();
    InputManager::GetInstance()->Update();

    if (impl_->isDebugCameraControl) {
        long deltaX = InputManager::GetInstance()->GetMouseDeltaX();
        long deltaY = InputManager::GetInstance()->GetMouseDeltaY();
        long wheelDelta = InputManager::GetInstance()->GetMouseDeltaZ();
        if (!ImGui::IsAnyItemActive() && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
            impl_->debugCamera.OnMouseDrag(float(deltaX), float(deltaY));
            impl_->debugCamera.OnMouseWheel(float(wheelDelta));
        }
        impl_->debugCamera.Update();
    }

    if (InputManager::GetInstance()->IsKeyJustPressed(DIK_SPACE) && !InputManager::GetInstance()->IsKeyJustReleased(DIK_SPACE)) {
        impl_->soundSystem.Play(impl_->soundData1);
    }

    // ImGui に渡すコンテキストを構築
    MyEngine::ImGuiManager::Context ctx;
    ctx.particleEmitter = &impl_->pmEmitter;
    ctx.object3dCommon = impl_->object3dCommon.get();
    std::vector<MyEngine::Object3d*> objPtrs;
    objPtrs.reserve(impl_->objects3d.size());
    for (auto& u : impl_->objects3d)
        objPtrs.push_back(u.get());
    ctx.objects3d = &objPtrs;
    std::vector<MyEngine::Sprite*> spritePtrs;
    spritePtrs.reserve(impl_->sprites.size());
    for (auto& u : impl_->sprites)
        spritePtrs.push_back(u.get());
    ctx.sprites = &spritePtrs;
    ctx.spriteCommon = impl_->spriteCommon.get();
    ctx.selectedDrawType = reinterpret_cast<int*>(&impl_->selectedDrawType);
    ctx.useBillboard = &impl_->useBillboard;
    ctx.particleManager = ParticleManager::GetInstance();
    ctx.dt = 1.0f / 60.0f;
    impl_->imguiManager.BuildUI(ctx);

    // 固定タイムステップで更新（ここでは 1/60 秒固定）
    const float dt = 1.0f / 60.0f;
    impl_->pmEmitter.Update(dt);
    auto* pm = ParticleManager::GetInstance();
    pm->Update(dt);
    impl_->soundSystem.Poll();

    MathUtility math;
    Matrix4x4 worldMatrix = math.MakeAffineMatrix(impl_->transform.scale, impl_->transform.rotate, impl_->transform.translate);
    if (impl_->camera)
        impl_->camera->Update();
    Matrix4x4 viewMatrix = impl_->camera ? impl_->camera->GetViewMatrix() : impl_->debugCamera.GetViewMatrix();
    Matrix4x4 projectionMatrix = impl_->camera ? impl_->camera->GetProjectionMatrix() : impl_->debugCamera.GetProjectionMatrix();
    Matrix4x4 worldViewProjectionMatrix = math.Multiply(worldMatrix, math.Multiply(viewMatrix, projectionMatrix));

    if (impl_->object3dCommon) {
        auto cam = impl_->object3dCommon->GetCameraData();
        if (cam)
            cam->worldPosition = impl_->debugCamera.GetTranslation();
    }

    for (auto& obj : impl_->objects3d)
        if (obj)
            obj->Update(viewMatrix, projectionMatrix);
    for (uint32_t i = 0; i < impl_->sprites.size(); i++)
        if (impl_->sprites[i])
            impl_->sprites[i]->Update();
}

// 毎フレーム描画処理
// - 描画前の共通処理 -> 各描画タイプに応じた描画 -> ImGui 描画 -> バッファスワップ
void Game::Draw()
{
    if (!impl_ || !impl_->initialized)
        return;

    // フレーム描画の準備
    DirectXCommon::GetInstance()->PreDraw();
    impl_->srvManager.PreDraw();

    MathUtility math;
    switch (impl_->selectedDrawType) {
    case DRAW_PARTICLE: {
        if (impl_->object3dCommon) {
            Matrix4x4 view = impl_->debugCamera.GetViewMatrix();
            Matrix4x4 proj = impl_->debugCamera.GetProjectionMatrix();
            Matrix4x4 vp = math.Multiply(view, proj);
            Vector3 right = { view.m[0][0], view.m[1][0], view.m[2][0] };
            Vector3 up = { view.m[0][1], view.m[1][1], view.m[2][1] };
            impl_->object3dCommon->SetBillboardCameraWithVP(right, up, vp, impl_->useBillboard);
            ParticleManager::GetInstance()->Draw();
        }
    } break;
    case DRAW_MODEL: {
        impl_->object3dCommon->SetCommonDrawSetting();
        if (impl_->objects3d.size() > 0 && impl_->objects3d[0])
            impl_->objects3d[0]->Draw();
    } break;
    case DRAW_SPRITE: {
        impl_->spriteCommon->SetCommonDrawSetting();
        for (uint32_t i = 0; i < impl_->sprites.size(); i++)
            impl_->sprites[i]->Draw();
    } break;
    case DRAW_BUNNY: {
        impl_->object3dCommon->SetCommonDrawSetting();
        if (impl_->objects3d.size() > 1 && impl_->objects3d[1])
            impl_->objects3d[1]->Draw();
    } break;
    case DRAW_FENCE: {
        impl_->object3dCommon->SetCommonDrawSetting();
        if (impl_->objects3d.size() > 3 && impl_->objects3d[3])
            impl_->objects3d[3]->Draw();
    } break;
    case DRAW_CHECKER: {
        impl_->object3dCommon->SetCommonDrawSetting();
        if (impl_->objects3d.size() > 2 && impl_->objects3d[2])
            impl_->objects3d[2]->Draw();
    } break;
    case DRAW_SPHERE: {
        impl_->object3dCommon->SetCommonDrawSetting();
        if (impl_->objects3d.size() > 4 && impl_->objects3d[4])
            impl_->objects3d[4]->Draw();
        if (impl_->objects3d.size() > 5 && impl_->objects3d[5])
            impl_->objects3d[5]->Draw();
    } break;
    case DRAW_ALL: {
        impl_->object3dCommon->SetCommonDrawSetting();
        for (auto& obj : impl_->objects3d)
            if (obj)
                obj->Draw();
        Matrix4x4 view = impl_->debugCamera.GetViewMatrix();
        Matrix4x4 proj = impl_->debugCamera.GetProjectionMatrix();
        Matrix4x4 vp = math.Multiply(view, proj);
        Vector3 right = { view.m[0][0], view.m[1][0], view.m[2][0] };
        Vector3 up = { view.m[0][1], view.m[1][1], view.m[2][1] };
        impl_->object3dCommon->SetBillboardCameraWithVP(right, up, vp, impl_->useBillboard);
        ParticleManager::GetInstance()->Draw();
        impl_->spriteCommon->SetCommonDrawSetting();
        for (uint32_t i = 0; i < impl_->sprites.size(); i++)
            impl_->sprites[i]->Draw();
    } break;
    }

    impl_->imguiManager.Render(DirectXCommon::GetInstance()->GetCommandList());
    DirectXCommon::GetInstance()->PostDraw();
}

// 終了処理 / リソース解放
void Game::Finalize()
{
    if (!impl_)
        return;

    CloseWindow(impl_->hwnd);

    impl_->objects3d.clear();
    impl_->object3dCommon.reset();
    TextureManager::GetInstance()->Finalize();
    impl_->imguiManager.Shutdown();
    impl_->srvManager.Finalize();
    ModelManager::GetInstance()->Finalize();
    impl_->spriteCommon.reset();
    impl_->sprites.clear();
    DirectXCommon::GetInstance()->Finalize();
    impl_->soundData1.reset();
    InputManager::GetInstance()->Finalize();
    impl_->directInput.Reset();
    impl_->winApp.Finalize();
    // COM のクリーンアップ
    CoUninitialize();

    // 基底クラスの終了処理を最後に呼び出す
    Framework::Finalize();
}

bool Game::IsEndRequest() const
{
    return impl_ ? impl_->endRequested : true;
}

// Framework::PollEvents をオーバーライドして WinApp のメッセージ処理を呼び出す
bool Game::PollEvents()
{
    if (!impl_)
        return false;
    // WinApp::ProcessMessage は WM_QUIT の検出で false を返す設計
    return impl_->winApp.ProcessMessage();
}

// Run は Framework::Run に移行したため、ここでの実装は不要になった

