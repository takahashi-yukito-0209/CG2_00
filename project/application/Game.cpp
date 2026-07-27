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
#include "engine/debug/DebugRenderer.h"
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
constexpr Vector3 kInitialCameraRotate = { 5.9f, -7.43f, 0.0f };
constexpr Vector3 kInitialCameraTranslate = { 0.0f, 1.0f, -18.0f };
constexpr Vector4 kWhiteColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色
constexpr Vector4 kPointLightPosition = { 0.0f, 1.5f, 0.0f, 0.0f };
constexpr Vector4 kPointLightColor = { 1.0f, 1.0f, 1.0f, 1.5f };
constexpr float kPointLightRadius = 6.0f;
constexpr float kLightDecay = 2.0f;
constexpr Vector4 kSpotLightPosition = { 2.0f, 1.25f, -3.0f, 0.0f };
constexpr Vector4 kSpotLightColor = { 1.0f, 1.0f, 1.0f, 2.0f };
constexpr float kSpotLightDistance = 7.0f;
constexpr Vector3 kSpotLightDirection = { -1.0f, -1.0f, 0.0f };
constexpr float kPi = 3.14159265358979323846f;
constexpr float kSpotLightAngle = kPi / 3.0f;
constexpr float kSpotLightFalloffStartAngle = kPi / 2.0f;
constexpr float kInitialDebugCameraWidth = 1280.0f;
constexpr float kInitialDebugCameraHeight = 720.0f;
constexpr float kDefaultCameraRotateSpeed = 0.01f;
constexpr float kDefaultCameraZoomSpeed = 0.05f;
constexpr float kFixedDeltaTime = 1.0f / 60.0f;
constexpr size_t kLogDateBufferSize = 32;
constexpr size_t kDebugCameraLogBufferSize = 128;
}

Game::Game()
    : impl_(nullptr)
{
}


struct Game::Impl {

    bool initialized = false;
    bool endRequested = false;


    Transform transform = {};

    WinApp winApp;
    HWND hwnd = nullptr;
    Microsoft::WRL::ComPtr<IDirectInput8> directInput;
    SoundSystem soundSystem;
    std::shared_ptr<SoundClip> soundData1;

    std::unique_ptr<SpriteCommon> spriteCommon;
    SrvManager srvManager;
    std::unique_ptr<Object3dCommon> object3dCommon;

    std::unique_ptr<DebugRenderer> debugRenderer; // デバッグ描画管理
    std::unique_ptr<Camera> camera;
    std::vector<std::unique_ptr<Sprite>> sprites;
    std::vector<std::unique_ptr<Object3d>> objects3d;
    std::unique_ptr<Object3d> particlePlane;

    std::unique_ptr<SceneManager> sceneManager;

    ParticleEmitter pmEmitter;

    DebugCamera debugCamera;
    bool isDebugCameraControl = true;
    bool useBillboard = true;
    bool useDebugCameraForRender = false;

    ImGuiManager imguiManager;

    std::string pendingSceneName;
};

/// <summary>
/// Game クラスのデストラクタ。
/// </summary>
Game::~Game()
{
}

/// <summary>
/// クラッシュダンプ出力を設定する。
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
    });
}

/// <summary>
/// ログファイル出力を設定する。
/// </summary>
void Game::SetupLogFile()
{
    std::filesystem::create_directory("logs");
    std::time_t now_c = std::time(nullptr);
    struct tm local_tm;
    localtime_s(&local_tm, &now_c);
    char dateBuf[kLogDateBufferSize];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d_%H%M%S", &local_tm);
    std::string dateString(dateBuf);
    std::string logFilePath = std::string("logs/") + dateString + ".log";
    Logger::SetLogFile(logFilePath);
    Logger::SetErrorLogFile(logFilePath);
}

/// <summary>
/// ウィンドウと入力を初期化する。
/// </summary>
bool Game::InitializeWindowAndInput(HINSTANCE hInstance, int nCmdShow)
{
    impl_->winApp.Initialize(hInstance, nCmdShow, L"CG2_00");
    impl_->hwnd = impl_->winApp.GetHwnd();

#ifdef _DEBUG

    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(FALSE);
    }

#endif

    HRESULT result = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, reinterpret_cast<void**>(impl_->directInput.GetAddressOf()), nullptr);
    if (FAILED(result)) {
        Logger::Log("Error: DirectInput8Create failed.\n");
        impl_->winApp.Finalize();
        return false;
    }

    if (!InputManager::GetInstance()->Initialize(impl_->directInput.Get(), impl_->hwnd)) {
        Logger::Log("Error: InputManager::Initialize failed.\n");
        impl_->winApp.Finalize();
        return false;
    }

    return true;
}

/// <summary>
/// エンジン共通リソースを初期化する。
/// </summary>
bool Game::InitializeEngineResources(HINSTANCE hInstance)
{
    std::unique_ptr<SpriteCommon> spriteCommonTmp;
    std::unique_ptr<Object3dCommon> object3dCommonTmp;

    if (!Framework::InitializeEngine(hInstance, &impl_->winApp, impl_->hwnd,
            spriteCommonTmp, impl_->srvManager, object3dCommonTmp)) {
        Logger::Log("Error: Framework::InitializeEngine failed\n");
        impl_->winApp.Finalize();
        return false;
    }


    impl_->spriteCommon = std::move(spriteCommonTmp);
    impl_->object3dCommon = std::move(object3dCommonTmp);
    impl_->debugRenderer = std::make_unique<DebugRenderer>();
    impl_->debugRenderer->Initialize(DirectXCommon::GetInstance());

    return true;
}

/// <summary>
/// カメラとライトを初期化する。
/// </summary>
void Game::InitializeCameraAndLighting()
{
    impl_->camera = std::make_unique<Camera>();
    impl_->camera->SetRotate(kInitialCameraRotate);
    impl_->camera->SetTranslate(kInitialCameraTranslate);
    impl_->camera->Update();
    impl_->object3dCommon->SetDefaultCamera(impl_->camera.get());

    Object3d::DirectionalLight* directionalLightData = impl_->object3dCommon->GetDirectionalLightData();
    if (directionalLightData) {
        directionalLightData->intensity = kWhiteColor.w;
        directionalLightData->color = kWhiteColor;
        directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
    }

    if (impl_->object3dCommon) {
        Object3d::PointLight pl = {};
        pl.position = kPointLightPosition;
        pl.color = kPointLightColor;
        pl.radius = kPointLightRadius;
        pl.decay = kLightDecay;
        pl.enabled = 1;
        impl_->object3dCommon->AddPointLight(pl);
    }

    if (impl_->object3dCommon) {
        auto sl = impl_->object3dCommon->GetSpotLightData();
        if (sl) {
            sl->position = kSpotLightPosition;
            sl->color = kSpotLightColor;
            sl->distance = kSpotLightDistance;
            sl->direction = MathUtil::Normalize(kSpotLightDirection);
            sl->decay = kLightDecay;
            sl->cosAngle = cosf(kSpotLightAngle);
            sl->cosFalloffStart = cosf(kSpotLightFalloffStartAngle);
            sl->enabled = 1;
        }
    }

    impl_->debugCamera.Initialize(kInitialDebugCameraWidth, kInitialDebugCameraHeight);

    if (impl_->object3dCommon) {
        impl_->object3dCommon->SetDebugCamera(&impl_->debugCamera);
        impl_->object3dCommon->SetUseDebugCameraForRender(impl_->useDebugCameraForRender);
        impl_->object3dCommon->SetEnableDebugCameraInput(impl_->isDebugCameraControl);
    }
}

/// <summary>
/// デバッグ機能、ImGui、サウンドを初期化する。
/// </summary>
void Game::InitializeDebugToolsAndSound()
{
    impl_->imguiManager.Initialize(impl_->hwnd, &impl_->srvManager);

    if (impl_->soundSystem.Initialize()) {
        impl_->soundData1 = impl_->soundSystem.LoadFromFile("resources/mokugyo.wav");
    } else {
        Logger::Warn("Game::Initialize: sound system initialization failed.\n");
    }
}

/// <summary>
/// リサイズ通知を設定する。
/// </summary>
void Game::SetupResizeCallbacks()
{
    DirectXCommon::GetInstance()->SetOnResizeCallback([this](uint32_t w, uint32_t h) {
        if (impl_->camera) {
            float aspect = (h != 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
            impl_->camera->SetAspectRatio(aspect);
            impl_->camera->Update();
        }
        impl_->debugCamera.Initialize(static_cast<float>(w), static_cast<float>(h));
        if (impl_->sceneManager) {
            impl_->sceneManager->OnWindowResize(w, h);
        }
    });

    impl_->winApp.SetResizeCallback([](uint32_t width, uint32_t height) {
        DirectXCommon::GetInstance()->OnWindowResize(width, height);
    });
}

/// <summary>
/// シーン管理を初期化する。
/// </summary>
void Game::InitializeScene()
{
    impl_->sceneManager = std::make_unique<SceneManager>();
    impl_->sceneManager->Initialize();
    SetupResizeCallbacks();

    SceneContext sctx;
    sctx.object3dCommon = impl_->object3dCommon.get();
    sctx.spriteCommon = impl_->spriteCommon.get();
    sctx.camera = impl_->camera.get();
    sctx.particleManager = ParticleManager::GetInstance();
    sctx.textureManager = TextureManager::GetInstance();
    sctx.srvManager = &impl_->srvManager;
    sctx.directXCommon = DirectXCommon::GetInstance();
    sctx.debugRenderer = impl_->debugRenderer.get();
    sctx.imguiManager = &impl_->imguiManager;
    sctx.requestSceneChange = [this](const std::string& sceneName) {
        impl_->pendingSceneName = sceneName;
    };
    impl_->sceneManager->SetContext(sctx);

    auto initial = GameApp::SceneFactory::Create("Title");
    if (initial) {
        impl_->sceneManager->ChangeScene(std::move(initial));
    }
}

/// <summary>
/// ゲームの初期化処理を行う。
/// </summary>
bool Game::Initialize(HINSTANCE hInstance, int nCmdShow)
{
    if (!Framework::Initialize(hInstance, nCmdShow)) {
        return false;
    }

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

    ParticleManager::GetInstance()->Initialize(
        DirectXCommon::GetInstance(),
        impl_->object3dCommon.get(),
        &impl_->srvManager,
        TextureManager::GetInstance(),
        &impl_->imguiManager);

    impl_->initialized = true;
    impl_->endRequested = false;

    InitializeScene();

    return true;
}

/// <summary>
/// ゲームの更新処理を行う。
/// </summary>
void Game::Update()
{
    Framework::Update();

    if (!impl_ || !impl_->initialized) {
        return;
    }

    if (!impl_->winApp.ProcessMessage()) {
        impl_->endRequested = true;
        return;
    }

    InputManager::GetInstance()->Update();

    bool debugInputEnabled = impl_->object3dCommon ? impl_->object3dCommon->GetEnableDebugCameraInput() : impl_->isDebugCameraControl;
    if (debugInputEnabled) {
        auto* input = InputManager::GetInstance();
        const long deltaX = input->GetMouseDeltaX();
        const long deltaY = input->GetMouseDeltaY();
        const long wheelDelta = input->GetMouseDeltaZ();

#ifdef USE_IMGUI
        const bool canUseCameraInput = !impl_->imguiManager.IsCapturingInput() || impl_->imguiManager.IsSceneViewHovered();
#else
        const bool canUseCameraInput = true;
#endif
        const bool isLeftDragging = canUseCameraInput && input->IsMouseButtonPressed(0) && (deltaX != 0 || deltaY != 0);
        const bool isWheelZoom = canUseCameraInput && wheelDelta != 0;
        const bool useDebugRender = impl_->object3dCommon ? impl_->object3dCommon->GetUseDebugCameraForRender() : impl_->useDebugCameraForRender;

        if (useDebugRender) {
            if (isLeftDragging) {
                impl_->debugCamera.OnMouseDrag(static_cast<float>(deltaX), static_cast<float>(deltaY));
            }
            if (isWheelZoom) {
                impl_->debugCamera.OnMouseWheel(static_cast<float>(wheelDelta));
            }
            impl_->debugCamera.Update();
        } else if (impl_->camera) {
            bool cameraChanged = false;
            if (isLeftDragging) {
                Vector3 cameraRotate = impl_->camera->GetRotate();
                cameraRotate.y += static_cast<float>(deltaX) * kDefaultCameraRotateSpeed;
                cameraRotate.x += static_cast<float>(deltaY) * kDefaultCameraRotateSpeed;
                impl_->camera->SetRotate(cameraRotate);
                cameraChanged = true;
            }
            if (isWheelZoom) {
                Vector3 cameraTranslate = impl_->camera->GetTranslate();
                cameraTranslate.z += static_cast<float>(wheelDelta) * kDefaultCameraZoomSpeed;
                impl_->camera->SetTranslate(cameraTranslate);
                cameraChanged = true;
            }
            if (cameraChanged) {
                impl_->camera->Update();
            }
        }
    }

    if (InputManager::GetInstance()->IsKeyJustPressed(DIK_SPACE) && !InputManager::GetInstance()->IsKeyJustReleased(DIK_SPACE)) {
        impl_->soundSystem.Play(impl_->soundData1);
    }

    const float dt = kFixedDeltaTime;
    impl_->soundSystem.Poll();

    Matrix4x4 worldMatrix = MathUtil::MakeAffineMatrix(impl_->transform.scale, impl_->transform.rotate, impl_->transform.translate);
    Matrix4x4 viewMatrix;
    Matrix4x4 projectionMatrix;

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
        impl_->debugCamera.Update();
        viewMatrix = impl_->debugCamera.GetViewMatrix();
        projectionMatrix = impl_->debugCamera.GetProjectionMatrix();
    } else {
        if (impl_->camera) {
            impl_->camera->Update();
            viewMatrix = impl_->camera->GetViewMatrix();
            projectionMatrix = impl_->camera->GetProjectionMatrix();
        } else {
            impl_->debugCamera.Update();
            viewMatrix = impl_->debugCamera.GetViewMatrix();
            projectionMatrix = impl_->debugCamera.GetProjectionMatrix();
        }
    }

    Matrix4x4 worldViewProjectionMatrix = MathUtil::Multiply(worldMatrix, MathUtil::Multiply(viewMatrix, projectionMatrix));

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

    for (auto& obj : impl_->objects3d) {
        if (obj) {
            obj->Update(viewMatrix, projectionMatrix);
        }
    }

    for (uint32_t i = 0; i < impl_->sprites.size(); i++) {
        if (impl_->sprites[i]) {
            impl_->sprites[i]->Update();
        }
    }

    if (impl_->sceneManager) {
        impl_->sceneManager->Update(dt);
    }
}

/// <summary>
/// ゲームの描画処理を行う。
/// </summary>
void Game::Draw()
{

    if (!impl_ || !impl_->initialized) {
        return;
    }

    impl_->imguiManager.NewFrame();

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
    Matrix4x4 sceneViewMatrix = MathUtil::MakeIdentity4x4();
    Matrix4x4 sceneProjectionMatrix = MathUtil::MakeIdentity4x4();
    const bool useDebugSceneViewCamera = impl_->object3dCommon ? impl_->object3dCommon->GetUseDebugCameraForRender() : impl_->useDebugCameraForRender;
    if (useDebugSceneViewCamera) {
        impl_->debugCamera.Update();
        sceneViewMatrix = impl_->debugCamera.GetViewMatrix();
        sceneProjectionMatrix = impl_->debugCamera.GetProjectionMatrix();
    } else if (impl_->camera) {
        impl_->camera->Update();
        sceneViewMatrix = impl_->camera->GetViewMatrix();
        sceneProjectionMatrix = impl_->camera->GetProjectionMatrix();
    }
    ctx.sceneViewMatrix = &sceneViewMatrix;
    ctx.sceneProjectionMatrix = &sceneProjectionMatrix;
    ctx.particleEmitter = &impl_->pmEmitter;
    ctx.object3dCommon = impl_->object3dCommon.get();
    std::vector<Object3d*> objPtrs;
    if (impl_->sceneManager && impl_->sceneManager->GetCurrent()) {
        impl_->sceneManager->GetCurrent()->FillObject3dPointers(&objPtrs);
    } else {
        objPtrs.reserve(impl_->objects3d.size());
        for (auto& o : impl_->objects3d) {
            objPtrs.push_back(o.get());
        }
    }
    ctx.objects3d = &objPtrs;
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
    std::vector<ParticleEmitter*> emitterPtrs; // Scene Viewギズモで操作するエミッター一覧
    if (impl_->sceneManager && impl_->sceneManager->GetCurrent()) {
        impl_->sceneManager->GetCurrent()->FillParticleEmitterPointers(&emitterPtrs);
    }
    if (emitterPtrs.empty()) {
        emitterPtrs.push_back(&impl_->pmEmitter);
    }
    ctx.particleEmitters = &emitterPtrs;
    ctx.spriteCommon = impl_->spriteCommon.get();
    ctx.useBillboard = &impl_->useBillboard;
    ctx.particleManager = ParticleManager::GetInstance();
    ctx.dt = kFixedDeltaTime;
    ctx.useDebugCameraForRender = &impl_->useDebugCameraForRender;
    ctx.requestSceneChange = [this](const char* sceneName) {
        if (sceneName) {
            impl_->pendingSceneName = sceneName;
        }
    };
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
    impl_->imguiManager.BuildUI(ctx);
    if (impl_->sceneManager && impl_->sceneManager->GetCurrent()) {
        impl_->sceneManager->GetCurrent()->DrawImGui();
    }

    DirectXCommon::GetInstance()->PreDraw();
    impl_->srvManager.PreDraw();

    if (impl_->sceneManager) {
        if (impl_->sceneManager->GetCurrent()) {
#ifdef USE_IMGUI
            impl_->sceneManager->GetCurrent()->SetSceneViewOnly(true);
#else
            impl_->sceneManager->GetCurrent()->SetSceneViewOnly(false);
#endif
        }
        if (impl_->debugRenderer) {
            impl_->debugRenderer->BeginFrame();
        }
        impl_->sceneManager->Draw();
    }

    impl_->imguiManager.Render(DirectXCommon::GetInstance()->GetCommandList());
    DirectXCommon::GetInstance()->PostDraw();
}

/// <summary>
/// ゲームの終了処理を行う。
/// </summary>
void Game::Finalize()
{
    if (!impl_) {
        return;
    }

    if (!impl_->initialized) {
        return;
    }

    impl_->initialized = false;
    impl_->endRequested = true;

    CloseWindow(impl_->hwnd);

    if (impl_->sceneManager) {
        impl_->sceneManager->Finalize();
        impl_->sceneManager.reset();
    }

    impl_->particlePlane.reset();
    impl_->objects3d.clear();
    impl_->sprites.clear();
    impl_->camera.reset();

    ParticleManager::GetInstance()->Finalize();

    impl_->soundData1.reset();
    impl_->soundSystem.Finalize();

    impl_->imguiManager.Shutdown();

    impl_->debugRenderer.reset();
    impl_->spriteCommon.reset();
    if (impl_->object3dCommon) {
        impl_->object3dCommon->Finalize();
        impl_->object3dCommon.reset();
    }

    TextureManager::GetInstance()->Finalize();

    DirectXCommon::GetInstance()->DestroyAllRenderTargets();

    impl_->srvManager.Finalize();

    ModelManager::GetInstance()->Finalize();

    DirectXCommon::GetInstance()->Finalize();


    InputManager::GetInstance()->Finalize();

    impl_->directInput.Reset();

    impl_->winApp.Finalize();

    Framework::Finalize();
}

/// <summary>
/// 終了要求が出ているかを返す。
/// </summary>
/// <returns>終了要求がある場合は true。</returns>
bool Game::IsEndRequest() const
{
    return impl_ ? impl_->endRequested : true;
}

/// <summary>
/// ウィンドウメッセージを処理し、継続可否を返す。
/// </summary>
/// <returns>処理を継続できる場合は true。</returns>
bool Game::PollEvents()
{
    if (!impl_) {
        return false;
    }

    return impl_->winApp.ProcessMessage();
}
