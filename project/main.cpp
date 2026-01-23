#include <Windows.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
// #include <format>
#include "externals/imgui/imgui.h"
#include "mathUtility.h"
#include <cmath>
#include <cassert>
#include <d3d12.h>
#include <dbghelp.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <fstream>
#include <string>
#include <strsafe.h>
#include <list>
#include <random>
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include <sstream>
#include <iomanip>
#include <vector>
#include <wrl.h>
#include <memory>
#include <xaudio2.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include "DebugCamera.h"
#include "InputManager.h"
#include "WinApp.h"
#include <dinput.h>
#include"DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#include "D3DResourceLeakChecker.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "engine/base/SrvManager.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "ModelManager.h"
#include "Camera.h"
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include "ImGuiManager.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dinput8.lib")

using namespace MyEngine;

// チャンクヘッダ
struct ChunkHeader {
    char id[4]; // チャンク毎のID
    int32_t size; // チャンクサイズ
};

// RIFFヘッダチャンク
struct RiffHeader {
    ChunkHeader chunk; //"RIFF"
    char type[4]; //"WAVE"
};

// FMTチャンク
struct FormatChunk {
    ChunkHeader chunk; //"fmt"
    WAVEFORMATEX fmt; // 波型フォーマット
};

// 音声データ
struct SoundData {
    // 波型フォーマット
    WAVEFORMATEX wfex;
    // バッファの先頭アドレス
    BYTE* pBuffer;
    // バッファのサイズ
    unsigned int bufferSize;
};

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception)
{
    // 時刻を取得して、時刻を名前にいれたファイルを作成。Dumpsディレクトリ以下に出力
    SYSTEMTIME time;
    GetLocalTime(&time);
    wchar_t filePath[MAX_PATH] = { 0 };
    CreateDirectory(L"./Dumps", nullptr);
    StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
    HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

    // processID(このexeのID)とクラッシュ(例外)の発生したthreadIDを取得
    DWORD processId = GetCurrentProcessId();
    DWORD threadId = GetCurrentThreadId();

    // 設定情報を入力
    MINIDUMP_EXCEPTION_INFORMATION minidumpInformation { 0 };
    minidumpInformation.ThreadId = threadId;
    minidumpInformation.ExceptionPointers = exception;
    minidumpInformation.ClientPointers = TRUE;

    // Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
    MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

    // ダンプファイルハンドルを閉じる
    CloseHandle(dumpFileHandle);

    // 他に関連付けられているSEH例外ハンドラがあれば実行。通常はプロセスを終了する
    return EXCEPTION_EXECUTE_HANDLER;
}

SoundData SoundLoadWave(const char* filename)
{
    // 1.ファイルオープン

    // ファイル入力ストリームのインスタンス
    std::ifstream file;
    //.wavファイルをバイナリモードで開く
    file.open(filename, std::ios_base::binary);
    // ファイルオープン失敗を検知する
    if (!file.is_open()) {
        char buf[256];
        sprintf_s(buf, "Error: Failed to open WAV file: %s\n", filename);
        Logger::Log(buf);
        return SoundData{};
    }

    // 2..wavデータ読み込み

    // RIFFヘッダーの読み込み
    RiffHeader riff;
    file.read((char*)&riff, sizeof(riff));
    // ファイルがRIFFかチェック
    if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
        Logger::Log("Error: Not a RIFF file.\n");
        file.close();
        return SoundData{};
    }
    // タイプがWAVEかチェック
    if (strncmp(riff.type, "WAVE", 4) != 0) {
        Logger::Log("Error: Not a WAVE file.\n");
        file.close();
        return SoundData{};
    }

    // Formatチャンクの読み込み
    FormatChunk format = {};
    // チャンクヘッダーの確認
    file.read((char*)&format, sizeof(ChunkHeader));
    if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
        Logger::Log("Error: fmt chunk not found.\n");
        file.close();
        return SoundData{};
    }

    // チャンク本体の読み込み
    if (format.chunk.size > sizeof(format.fmt)) {
        Logger::Log("Error: fmt chunk size too large.\n");
        file.close();
        return SoundData{};
    }
    file.read((char*)&format.fmt, format.chunk.size);

    // Dataチャンクの読み込み
    ChunkHeader data;
    file.read((char*)&data, sizeof(data));
    // JUNKチャンクを検出した場合
    if (strncmp(data.id, "JUNK", 4) == 0) {
        // 読み取り位置をJUNKチャンクの終わりまで進める
        file.seekg(data.size, std::ios_base::cur);
        // 再読み込み
        file.read((char*)&data, sizeof(data));
    }

    if (strncmp(data.id, "data", 4) != 0) {
        Logger::Log("Error: data chunk not found.\n");
        file.close();
        return SoundData{};
    }

    // Dataチャンクのデータ部(波型データ)の読み込み
    // バッファを BYTE 配列として確保して読み込む
    BYTE* pBuffer = new BYTE[data.size];
    file.read(reinterpret_cast<char*>(pBuffer), data.size);

    // 3.ファイルクローズ

    // waveファイルを閉じる
    file.close();

    // 4.読み込んだ音声データをreturn

    // returnするための音声データ
    SoundData soundData = {};

    soundData.wfex = format.fmt;
    soundData.pBuffer = pBuffer;
    soundData.bufferSize = data.size;

    return soundData;
}

// 音声データ解放
void SoundUnload(SoundData* soundData)
{
    // バッファのメモリを解放
    delete[] soundData->pBuffer;

    soundData->pBuffer = 0;
    soundData->bufferSize = 0;
    soundData->wfex = {};
}

// 音声再生
void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData)
{
    HRESULT result;

    // 波型フォーマットを元にSourceVoiceの生成
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(result));

    // 再生する波型データの設定
    XAUDIO2_BUFFER buf {};
    buf.pAudioData = soundData.pBuffer;
    buf.AudioBytes = soundData.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;

    // 波型データの再生
    result = pSourceVoice->SubmitSourceBuffer(&buf);
    result = pSourceVoice->Start();
}

// Transform変数を作る
Transform transform = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

// Windowsアプリでのエントリーポイント（main関数）
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // COMの初期化
    CoInitializeEx(0, COINIT_MULTITHREADED);

    // 誰も捕捉しなかった場合に(Unhandled)、補足する関数を登録
    // main関数始まってすぐに登録すると良い
    SetUnhandledExceptionFilter(ExportDump);

    // log出力用のフォルダ「logs」の作成
    std::filesystem::create_directory("logs");

    // ここからログファイルのパスを生成してLoggerへ設定
    // 現在時刻を取得してログ名用の文字列を作成（C++14互換）
    std::time_t now_c = std::time(nullptr);
    struct tm local_tm;
    localtime_s(&local_tm, &now_c);
    char dateBuf[32];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d_%H%M%S", &local_tm);
    std::string dateString(dateBuf);
    // 時刻を使ってファイル名を指定
    std::string logFilePath = std::string("logs/") + dateString + ".log";
    // ロガーへ出力先を設定（一般ログ）
    Logger::SetLogFile(logFilePath);
    // 異常系ログ（Warn/Error）も同じファイルへ出力
    Logger::SetErrorLogFile(logFilePath);

    // WinAppを通常のインスタンスとして生成
    MyEngine::WinApp winApp;
    winApp.Initialize(hInstance, nCmdShow, L"GE3_LE2B_15_タカハシ_ユキト");

    // HWND取得
    HWND hwnd = winApp.GetHwnd();

#ifdef _DEBUG

    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        // デバックレイヤーを有効化する
        debugController->EnableDebugLayer();
        // さらにGPU側でもチェックを行うようにする
        debugController->SetEnableGPUBasedValidation(TRUE);
    }

#endif

    // リークチェッカーの生成
    D3DResourceLeakChecker leakCheck;

    // XAudio2
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice;

    // COMライブラリの初期化は先頭で行っているためここではHRESULT変数のみ用意する
    HRESULT result = S_OK;
    // XAudioエンジンのインスタンスを生成
    result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    // マスターボイスを生成
    result = xAudio2->CreateMasteringVoice(&masterVoice);

    // 音声読み込み
    SoundData soundData1 = SoundLoadWave("resources/mokugyo.wav");

    // DirectInputの初期化 (ComPtrで管理)
    Microsoft::WRL::ComPtr<IDirectInput8> directInput;
    result = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, reinterpret_cast<void**>(directInput.GetAddressOf()), nullptr);
    if (FAILED(result)) {
        Logger::Log("Error: DirectInput8Create failed.\n");
        // フェールセーフでウィンドウを破棄して終了
        winApp.Finalize();
        CoUninitialize();
        return 1;
    }

    // 初期化時
    InputManager::GetInstance()->Initialize(directInput.Get(), hwnd);

    // DirectXの初期化
    DirectXCommon::GetInstance()->Initialize(&winApp);

#pragma region 基盤システムの初期化

    std::unique_ptr<SpriteCommon> spriteCommon = std::make_unique<SpriteCommon>();
    // スプライト共通部の初期化
    spriteCommon->Initialize(DirectXCommon::GetInstance());

    //テクスチャマネージャーの初期化
    SrvManager srvManager;
    srvManager.Initialize(DirectXCommon::GetInstance());
    // ImGui の初期化 is delegated to ImGuiManager (will call SrvManager->InitImGui)
    TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), &srvManager);

    std::unique_ptr<Object3dCommon> object3dCommon = std::make_unique<Object3dCommon>();
    // 3Dオブジェクト共通部の初期化
    object3dCommon->Initialize(DirectXCommon::GetInstance());

    // デフォルトカメラを生成して共通部に登録
    auto camera = std::make_unique<Camera>();
    camera->SetRotate({-0.1f, 0.0f, 0.0f});
    camera->SetTranslate({0.0f, 1.0f, -20.0f});
    camera->Update();
    object3dCommon->SetDefaultCamera(camera.get());

#pragma endregion 基盤システムの初期化

    // Textureを読んで転送する
    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
    TextureManager::GetInstance()->LoadTexture("resources/circle.png");

#pragma region 最初のシーンの初期化
    
    //スプライト複数設定
    std::vector<std::unique_ptr<Sprite>> sprites; // Sprite クラスのポインタを格納するための動的配列
    const uint32_t kSpriteCount = 5; // 描画対象とするスプライトの総数
    // スプライトに使用するテクスチャのファイル名を格納する配列
    std::array<std::string, 2> spriteNames = {
        "resources/uvChecker",
        "resources/monsterBall",
    };

    // 指定された数だけスプライトを生成・設定するループ
    for (uint32_t i = 0; i < kSpriteCount; ++i) {
        if (i / 2 == 0) {
            auto sprite = std::make_unique<Sprite>();
            sprite->Initialize(spriteCommon.get(), spriteNames[0] + ".png");
            sprites.push_back(std::move(sprite));
        } else {
            auto sprite = std::make_unique<Sprite>();
            sprite->Initialize(spriteCommon.get(), spriteNames[1] + ".png");
            sprites.push_back(std::move(sprite));
        }
    }

    // 3dオブジェクト複数初期化（通常描画用）
    std::vector<std::unique_ptr<Object3d>> objects3d;// Object3d クラスのポインタを格納するための動的配列
    const uint32_t kObject3DCount = 6; // 描画対象とする 3D オブジェクトの総数
    // 複数モデルを割り当てるためのファイル名リスト
    std::vector<std::string> modelFileNames = {
        "plane.obj",
        "bunny.obj",
        "teapot.obj",
        "models/fence/fence.obj",
        "models/sphere/sphere.obj", 
        "models/terrain/terrain.obj"
    };

    // 指定された数だけ 3D オブジェクトを生成・設定するループ
    for (uint32_t i = 0; i < kObject3DCount; ++i) {
        // オブジェクトの生成
        auto obj = std::make_unique<Object3d>();
        obj->Initialize(object3dCommon.get());

        // モデルファイル名を選択（リストの範囲外アクセスを避けるため modulo を使用）
        std::string modelFile = modelFileNames.empty() ? std::string("plane.obj") : modelFileNames[i % modelFileNames.size()];
        // 指定したファイルのモデルを読み込んでオブジェクトに紐づける
        obj->SetModel(modelFile);

        // fence モデルの場合、アルファカットアウトサンプラーを使用する設定にする
        if (modelFile.find("fence") != std::string::npos) {
            obj->SetUseAlphaCutoutSampler(true);
        }

        // 配列に格納
        objects3d.push_back(std::move(obj));
    }

    // パーティクル専用のプレーン（通常描画リストには含めない）
    std::unique_ptr<Object3d> particlePlane = std::make_unique<Object3d>();
    particlePlane->Initialize(object3dCommon.get());
    particlePlane->SetModel("plane.obj");
    particlePlane->SetTexture("resources/circle.png");

    // ParticleManager 初期化とグループ作成
    ParticleManager::GetInstance()->Initialize(
        DirectXCommon::GetInstance(),
        object3dCommon.get(),
        &srvManager,
        TextureManager::GetInstance());
    ParticleManager::GetInstance()->SetParticlePlane(particlePlane.get());
    ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/circle.png");
    ParticleManager::GetInstance()->CreateParticleGroup("Checker", "resources/uvChecker.png");
    ParticleManager::GetInstance()->CreateParticleGroup("Ball", "resources/monsterBall.png");

#pragma endregion 最初のシーンの終了

    // 自作した数学関数の使用
    MathUtility math;
   
    // 平行光源データの取得
    DirectionalLight* directionalLightData = object3dCommon->GetDirectionalLightData();
    if (!directionalLightData) {
        Logger::Log("Warning: Failed to get shared directional light data from Object3dCommon\n");
    }

    // シーン照明の設定: 暗めの環境に明るいスポットを作る
    if (directionalLightData) {
        // 環境/平行光を暗めにして点光源を目立たせる
        directionalLightData->intensity = 0.05f;
        directionalLightData->color = {0.2f, 0.25f, 0.3f, 1.0f};
        directionalLightData->direction = {0.0f, -1.0f, 0.0f};
    }

    // 単一の明るい点光源を球体近傍に配置する
    if (object3dCommon) {
        MyEngine::Object3d::PointLight pl = {};
        pl.position = { 0.0f, 1.5f, 0.0f, 0.0f }; // オブジェクトの上方
        pl.color = { 1.0f, 1.0f, 1.0f, 6.0f }; // rgb + 強度を w 成分に格納
        pl.radius = 6.0f;
        pl.decay = 2.0f;
        pl.enabled = 1;
        int idx = object3dCommon->AddPointLight(pl);
        if (idx < 0) {
            Logger::Log("Warning: failed to add initial point light\n");
        }
    }

    // 単一スポットライトを設定して集中的な明るい領域を作る
    if (object3dCommon) {
        // Object3dCommon が所有するマップ済みのスポットライトデータへアクセス
        auto sl = object3dCommon->GetSpotLightData();
        if (sl) {
            // スポットライトの位置をわずかに横と上にずらす
            sl->position = { 2.0f, 1.25f, -3.0f, 0.0f };
            // RGB 色。w 成分は強度として使う
            sl->color = { 1.0f, 1.0f, 1.0f, 4.0f };
            // スポットライトの有効範囲
            sl->distance = 7.0f;
            // ターゲット（原点/オブジェクト）に向ける
            sl->direction = math.Normalize({ -1.0f, -1.0f, 0.0f });
            // 距離減衰の指数
            sl->decay = 2.0f;
            // 内側コーン角のコサイン（中心）、ここでは60度を使用
            sl->cosAngle = cosf(3.14159265358979323846f / 3.0f);
            // フォールオフ開始角のコサイン、90度を使用して90->60度でフェードする
            sl->cosFalloffStart = cosf(3.14159265358979323846f / 2.0f);
            // スポットライトを有効化
            sl->enabled = 1;
        }
    }

    // 全てのロード完了後、まとめて転送を実行
    TextureManager::GetInstance()->ExecuteResourceUpload();

    // デバッグ: 読み込んだテクスチャ情報をログ出力
    {
        uint32_t count = TextureManager::GetInstance()->GetLoadedTextureCount();
        char buf[256];
        sprintf_s(buf, "Debug: Loaded texture count = %u\n", count);
        Logger::Log(buf);
        for (uint32_t ti = 0; ti < count; ++ti) {
            auto meta = TextureManager::GetInstance()->GetMetadata(ti);
            auto handle = TextureManager::GetInstance()->GetSrvHandleGPU(ti);
            {
                std::ostringstream oss;
                oss << "Debug: Texture[" << ti << "] size=" << meta.width << " x " << meta.height
                    << " format=" << static_cast<int>(meta.format)
                    << " srv.ptr=0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << handle.ptr;
                Logger::Log(oss.str());
            }
        }
    }

    // 描画対象をUIで切り替えるための変数と選択肢
    enum DrawType {
        DRAW_MODEL,
        DRAW_PARTICLE,
        DRAW_SPRITE,
        DRAW_BUNNY,
        DRAW_FENCE,
        DRAW_CHECKER,
        DRAW_SPHERE,
        DRAW_ALL
    };

    DrawType selectedDrawType = DRAW_SPHERE; // 初期値

    // パーティクルエミッタ
    ParticleEmitter pmEmitter;
    pmEmitter.groupName = "Circle";
    pmEmitter.transform.translate = {0.0f,0.0f,0.0f};
    pmEmitter.count = 3;
    pmEmitter.frequency = 0.5f;

    // 起動時の初期バースト
    for (int i = 0; i < 20; ++i) {
        pmEmitter.Emit();
    }

    // ParticleManager UI のデフォルト値
    bool uiFieldEnabled = false;
    Vector3 uiFieldAccel { 15.0f, 0.0f, 0.0f };
    Vector3 uiFieldMin { -100.0f, -100.0f, -100.0f };
    Vector3 uiFieldMax {  100.0f,  100.0f,  100.0f };
    float uiLifeMin = 1.0f;
    float uiLifeMax = 3.0f;
    // パーティクル数の取得
    int particleCount = 1;
    if (object3dCommon) {
        particleCount = static_cast<int>(object3dCommon->GetInstancingSlotCount());
        if (particleCount <= 0) particleCount = 1;
    }

    const char* drawOptions[] = {
        "Model", // モデルのみ描画
        "Particle", // インスタンシング（パーティクル）描画
        "Sprite", // スプライトのみ描画
        "Bunny", // bunnyのみ描画
        "Fence", // fenceのみ描画
        "Checker", // ティーポットのみを描画
        "Sphere", // 球体のみを描画
        "All" // 両方描画
    };

    // デバッグカメラの生成
    DebugCamera debugCamera;
    debugCamera.Initialize(1280.0f, 720.0f); // 画面サイズを指定
    bool isDebugCameraControl = true; // カメラ操作を有効にするか
    bool useBillboard = true; // パーティクルのビルボード有効/無効

    // Create ImGui manager and initialize
    MyEngine::ImGuiManager imguiManager;
    imguiManager.Initialize(hwnd, &srvManager);

    MSG msg {};
    // ウィンドウのxボタンが押されるまでループ
    while (msg.message != WM_QUIT) {
        // windowにメッセージが来てたら最優先で処理させる
        if (!winApp.ProcessMessage()) {
            break;
        } else {
            // ImGuiフレーム開始
            imguiManager.NewFrame();

            //--------------------
            // ゲームの処理(UpDate)
            //--------------------

            // キー入力毎フレーム更新
            InputManager::GetInstance()->Update();

            // 入力チェック
            if (InputManager::GetInstance()->IsKeyJustPressed(DIK_SPACE)) {
                // スペースキーが押された瞬間の処理
                OutputDebugStringA("Hit 1\n");
            }

            // カメラ操作処理
            if (isDebugCameraControl) {

                // マウス入力取得
                long deltaX = InputManager::GetInstance()->GetMouseDeltaX();
                long deltaY = InputManager::GetInstance()->GetMouseDeltaY();
                long wheelDelta = InputManager::GetInstance()->GetMouseDeltaZ();

                // ImGui操作中ならマウスによるカメラ回転・ズームは止める
                if (!ImGui::IsAnyItemActive() && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
                    debugCamera.OnMouseDrag(float(deltaX), float(deltaY));
                    debugCamera.OnMouseWheel(float(wheelDelta));
                }

                // デバックカメラの更新
                debugCamera.Update();
            }

            // 音声再生
            if (InputManager::GetInstance()->IsKeyJustPressed(DIK_SPACE) && !InputManager::GetInstance()->IsKeyJustReleased(DIK_SPACE)) {
                SoundPlayWave(xAudio2.Get(), soundData1);
            }


            // Build UI via ImGuiManager
            MyEngine::ImGuiManager::Context ctx;
            ctx.particleEmitter = &pmEmitter;
            ctx.object3dCommon = object3dCommon.get();
            // build a temporary raw pointer list for objects3d
            std::vector<MyEngine::Object3d*> objPtrs;
            objPtrs.reserve(objects3d.size());
            for (auto &u : objects3d) objPtrs.push_back(u.get());
            ctx.objects3d = &objPtrs;
            // build sprite pointer list
            std::vector<MyEngine::Sprite*> spritePtrs;
            spritePtrs.reserve(sprites.size());
            for (auto &u : sprites) spritePtrs.push_back(u.get());
            ctx.sprites = &spritePtrs;
            ctx.spriteCommon = spriteCommon.get();
            ctx.selectedDrawType = reinterpret_cast<int*>(&selectedDrawType);
            ctx.useBillboard = &useBillboard;
            ctx.particleManager = ParticleManager::GetInstance();
            ctx.dt = 1.0f / 60.0f;

            imguiManager.BuildUI(ctx);

            // Manager 設定反映と更新 (particle manager is updated in main loop as before)
            {
                const float dt = 1.0f / 60.0f;
                pmEmitter.Update(dt);
                auto* pm = ParticleManager::GetInstance();
                // ParticleManager will be manipulated by ImGui via ctx.particleManager where available
                pm->Update(dt);
            }

            // WorldMatrix作成(model)
            Matrix4x4 worldMatrix = math.MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
            // まずデフォルトカメラを更新
            if (camera) { camera->Update(); }
            Matrix4x4 viewMatrix = camera ? camera->GetViewMatrix() : debugCamera.GetViewMatrix();
            Matrix4x4 projectionMatrix = camera ? camera->GetProjectionMatrix() : debugCamera.GetProjectionMatrix();
            // WVPMatrixを作る
            Matrix4x4 worldViewProjectionMatrix = math.Multiply(worldMatrix, math.Multiply(viewMatrix, projectionMatrix));

            // CPU側 Particles を GPU インスタンシングへ同期（生成時刻で安定ソート）
            // Camera position to GPU (Object3dCommon camera CB)
            if (object3dCommon) {
                auto cam = object3dCommon->GetCameraData();
                if (cam) {
                    cam->worldPosition = debugCamera.GetTranslation();
                }
            }

            // Object3Dの更新（複数）
            for (auto& obj : objects3d) {
                if (obj) {
                    obj->Update(viewMatrix, projectionMatrix);
                }
            }

            //スプライトの更新
            for (uint32_t i = 0; i <kSpriteCount; i++) {
                if (sprites[i]) sprites[i]->Update();
            }

            // UI is built via ImGuiManager::BuildUI(ctx). Remove duplicate
            // direct ImGui code to avoid duplication and keep UI centralized.

            //--------------------
            // 画面のクリア処理(Draw)
            //--------------------

            // 描画前処理
            DirectXCommon::GetInstance()->PreDraw();
            // SRVヒープをセット（1フレーム1回）
            srvManager.PreDraw();

            // 描画準備は描画対象ごとに行う（PSO/RootSignatureを切り替えるため）

            // NOTE: ImGui must be rendered after scene (sprites/3D) so UI stays on top.
            // We only set up descriptor heaps here; actual ImGui render call is moved to after scene draw.

            // 描画対象に応じた処理
            switch (selectedDrawType) {

            case DRAW_PARTICLE:

                // 3D描画の共通設定
                if (object3dCommon) {
                    // カメラベクトルを更新（右/上）
                    // 簡易算出: View行列からRight/Upを取り出す
                    Matrix4x4 view = debugCamera.GetViewMatrix();
                    Matrix4x4 proj = debugCamera.GetProjectionMatrix();
                    Matrix4x4 vp = math.Multiply(view, proj);
                    Vector3 right = { view.m[0][0], view.m[1][0], view.m[2][0] };
                    Vector3 up    = { view.m[0][1], view.m[1][1], view.m[2][1] };
                    object3dCommon->SetBillboardCameraWithVP(right, up, vp, useBillboard);
                    ParticleManager::GetInstance()->Draw();
                }

                break;

            case DRAW_MODEL:

                // 3D描画の共通設定
                object3dCommon->SetCommonDrawSetting();

                // planeモデルのみ描画
                if (objects3d.size() > 0 && objects3d[0]) {
                    objects3d[0]->Draw();
                }

                break;

            case DRAW_SPRITE:

                // Sprite描画準備
                spriteCommon->SetCommonDrawSetting();

                // スプライトの描画
                for (uint32_t i = 0; i < kSpriteCount; i++) {
                    sprites[i]->Draw();
                }

                break;

            case DRAW_BUNNY:

                // 3D描画の共通設定
                object3dCommon->SetCommonDrawSetting();

                // bunnyモデルのみ描画
                if (objects3d.size() > 1 && objects3d[1]) {
                    objects3d[1]->Draw();
                }

                break;

            case DRAW_FENCE:
                
                // 3D描画の共通設定
                object3dCommon->SetCommonDrawSetting();

                // fenceモデルのみ描画
                if (objects3d.size() > 3 && objects3d[3]) {
                    objects3d[3]->Draw();
                }

                break;

            case DRAW_CHECKER:
                
                // 3D描画の共通設定
                object3dCommon->SetCommonDrawSetting();
                
                // teapotモデルのみ描画
                if (objects3d.size() > 2 && objects3d[2]) {
                    objects3d[2]->Draw();
                }

                break;

            case DRAW_SPHERE: 

                // 3D描画の共通設定
                object3dCommon->SetCommonDrawSetting();

                // sphereモデルのみ描画
                if (objects3d.size() > 4 && objects3d[4]) {
                    objects3d[4]->Draw();
                }

                if (objects3d.size() > 5 && objects3d[5]) {
                    objects3d[5]->Draw();
                }
                break;

            case DRAW_ALL:

                // 3Dオブジェクトの描画（複数）
                object3dCommon->SetCommonDrawSetting();
                for (auto& obj : objects3d) {
                    if (obj) {
                        obj->Draw();
                    }
                }

                // パーティクルの描画
                {
                    Matrix4x4 view = debugCamera.GetViewMatrix();
                    Matrix4x4 proj = debugCamera.GetProjectionMatrix();
                    Matrix4x4 vp = math.Multiply(view, proj);
                    Vector3 right = { view.m[0][0], view.m[1][0], view.m[2][0] };
                    Vector3 up    = { view.m[0][1], view.m[1][1], view.m[2][1] };
                    object3dCommon->SetBillboardCameraWithVP(right, up, vp, useBillboard);
                    ParticleManager::GetInstance()->Draw();
                }

                // スプライトの描画
                spriteCommon->SetCommonDrawSetting();
                for (uint32_t i = 0; i < kSpriteCount; i++) {
                    sprites[i]->Draw();
                }

                break;
            }

            // Render ImGui last so it appears on top of scene
            imguiManager.Render(DirectXCommon::GetInstance()->GetCommandList());

            // 描画後処理
            DirectXCommon::GetInstance()->PostDraw();
        }
    }

    CloseWindow(hwnd);

    // 自作リソース解放
    // 3D/2D オブジェクト（unique_ptrで管理しているため自動破棄）
    objects3d.clear();
    object3dCommon.reset();

    // テクスチャ/モデル管理の解放
    TextureManager::GetInstance()->Finalize();
    // Shutdown ImGui (will shutdown platform and context)
    imguiManager.Shutdown();
    srvManager.Finalize();
    ModelManager::GetInstance()->Finalize();

    // スプライト等
    spriteCommon.reset();
    sprites.clear();

    // DirectX のシステム系は最後に破棄
    DirectXCommon::GetInstance()->Finalize(); 

    // 音・入力など DirectX 依存していないもの
    // マスターボイスが存在する場合は破棄しておく
    if (masterVoice) {
        masterVoice->DestroyVoice();
        masterVoice = nullptr;
    }
    xAudio2.Reset();
    SoundUnload(&soundData1);

    InputManager::GetInstance()->Finalize();
    directInput.Reset();

    // OS側のウィンドウ破棄
    winApp.Finalize();

    // COMの終了処理
    CoUninitialize();

    return 0;
}