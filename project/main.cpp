#include <Windows.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
// #include <format>
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
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
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "ModelManager.h"

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

    // ここからファイルを作成し、ofStreamを取得する
    // 現在時刻を取得してログ名用の文字列を作成（C++14互換）
    std::time_t now_c = std::time(nullptr);
    struct tm local_tm;
    localtime_s(&local_tm, &now_c);
    char dateBuf[32];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d_%H%M%S", &local_tm);
    std::string dateString(dateBuf);
    // 時刻を使ってファイル名を指定
    std::string logFilePath = std::string("logs/") + dateString + ".log";
    // ファイルを作って書き込み準備
    std::ofstream logStream(logFilePath);

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
    TextureManager::GetInstance()->Initialize();

    std::unique_ptr<Object3dCommon> object3dCommon = std::make_unique<Object3dCommon>();
    // 3Dオブジェクト共通部の初期化
    object3dCommon->Initialize(DirectXCommon::GetInstance());

#pragma endregion 基盤システムの初期化

    // Textureを読んで転送する
    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");

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

    // 3dオブジェクト複数初期化
    std::vector<std::unique_ptr<Object3d>> objects3d;// Object3d クラスのポインタを格納するための動的配列
    const uint32_t kObject3DCount = 5; // 描画対象とする 3D オブジェクトの総数
    // 複数モデルを割り当てるためのファイル名リスト
    std::vector<std::string> modelFileNames = {
        "plane.obj",
        "bunny.obj",
        "teapot.obj",
        "models/fence/fence.obj",
        "models/sphere/sphere.obj", 
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

#pragma endregion 最初のシーンの終了

    // 自作した数学関数の使用
    MathUtility math;
    
   
    // 平行光源データの取得
    DirectionalLight* directionalLightData = object3dCommon->GetDirectionalLightData();
    if (!directionalLightData) {
        Logger::Log("Warning: Failed to get shared directional light data from Object3dCommon\n");
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
        DRAW_SPHERE, // <--- 追加
        DRAW_ALL
    };

    DrawType selectedDrawType = DRAW_SPRITE; // 初期値

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

    MSG msg {};
    // ウィンドウのxボタンが押されるまでループ
    while (msg.message != WM_QUIT) {
        // windowにメッセージが来てたら最優先で処理させる
        if (!winApp.ProcessMessage()) {
            break;
        } else {

            // ImGuiフレーム開始
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

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

            // WorldMatrix作成(model)
            Matrix4x4 worldMatrix = math.MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
            Matrix4x4 viewMatrix = debugCamera.GetViewMatrix();
            Matrix4x4 projectionMatrix = debugCamera.GetProjectionMatrix();
            // WVPMatrixを作る
            Matrix4x4 worldViewProjectionMatrix = math.Multiply(worldMatrix, math.Multiply(viewMatrix, projectionMatrix));

            // Camera position to GPU (Object3dCommon camera CB)
            if (object3dCommon) {
                auto cam = object3dCommon->GetCameraData();
                if (cam) {
                    cam->worldPosition = debugCamera.GetTranslation();
                }
            }

            // 平行光源データ設定
            if (object3dCommon) {
                auto instData = object3dCommon->GetInstancingData();
                uint32_t instSlots = object3dCommon->GetInstancingSlotCount();
                if (instData && instSlots > 0) {
                    // Arrange instances in a grid, with a small deterministic jitter per instance
                    int count = std::min<int>(static_cast<int>(instSlots), particleCount);
                    int safeCount = (count > 1) ? count : 1;
                    int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(safeCount))));
                    float spacing = 0.6f;
                    float halfW = (cols - 1) * spacing * 0.5f;
                    for (int i = 0; i < static_cast<int>(instSlots); ++i) {
                        if (i < count) {
                            int col = i % cols;
                            int row = i / cols;
                            // base grid position
                            float baseX = col * spacing - halfW;
                            float baseZ = row * spacing - halfW;
                            // deterministic pseudo-random jitter via simple LCG
                            uint32_t seed = static_cast<uint32_t>(i) * 1664525u + 1013904223u;
                            float jitterX = ((seed & 0xFFFFu) / 65535.0f - 0.5f) * 0.3f; // ±0.15
                            float jitterY = (((seed >> 16) & 0xFFFFu) / 65535.0f - 0.5f) * 0.2f; // ±0.1
                            float jitterZ = ((((seed >> 8) ^ (seed << 8)) & 0xFFFFu) / 65535.0f - 0.5f) * 0.3f;

                            Transform t;
                            t.scale = { 1.0f, 1.0f, 1.0f };
                            t.rotate = { 0.0f, 0.0f, 0.0f };
                            t.translate = { baseX + jitterX, jitterY, baseZ + jitterZ };

                            Matrix4x4 worldI = math.MakeAffineMatrix(t.scale, t.rotate, t.translate);
                            Matrix4x4 wvpI = math.Multiply(worldI, math.Multiply(viewMatrix, projectionMatrix));
                            instData[i].World = worldI;
                            instData[i].WVP = wvpI;
                        } else {
                            // zero/identity for unused slots
                            instData[i].World = math.MakeIdentity4x4();
                            instData[i].WVP = math.MakeIdentity4x4();
                        }
                    }
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

            // imguiの項目内容
            ImGui::Begin("Settings");

            // ImGuiのUIで描画対象を選択
            ImGui::Combo("Model", (int*)&selectedDrawType, drawOptions, IM_ARRAYSIZE(drawOptions));

            // パーティクル数設定UI
            if (selectedDrawType == DRAW_PARTICLE) {
                if (object3dCommon) {
                    int maxSlots = static_cast<int>(object3dCommon->GetInstancingSlotCount());
                    if (maxSlots <= 0) maxSlots = 1;
                    ImGui::SliderInt("Particle Count", &particleCount, 1, maxSlots);
                } else {
                    ImGui::Text("Instancing not available (Object3dCommon not initialized)");
                }
            }

            // 各描画対象の個別編集UI
            if (selectedDrawType == DRAW_ALL) {
                // 各Object3dの個別編集UI (すべて表示)
                for (uint32_t i = 0; i < objects3d.size(); ++i) {
                    Object3d* object = objects3d[i].get();
                    if (!object) {
                        continue;
                    }
                    char headerName[64];
                    sprintf_s(headerName, "Object %d", i);
                    ImGui::PushID(i);
                    if (ImGui::CollapsingHeader(headerName)) {
                        Vector3 scale = object->GetScale();
                        Vector3 rotate = object->GetRotate();
                        Vector3 translate = object->GetTranslate();

                        if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) {
                            object->SetScale(scale);
                        }

                        if (ImGui::DragFloat3("Rotate", &rotate.x, 0.01f)) {
                            object->SetRotate(rotate);
                        }

                        if (ImGui::DragFloat3("Translate", &translate.x, 0.1f)) {
                            object->SetTranslate(translate);
                        }
                        // テクスチャ設定欄
                        static char texPathBufAll[256] = "resources/uvChecker.png";
                        ImGui::InputText("Texture Path", texPathBufAll, sizeof(texPathBufAll));
                        if (ImGui::Button("Apply Texture")) {
                            object->SetTexture(std::string(texPathBufAll));
                        }

                        // ライティング設定を追加
                        bool enableLighting = object->GetEnableLighting();
                        if (ImGui::Checkbox("Enable Lighting", &enableLighting)) {
                            object->SetEnableLighting(enableLighting);
                        }

                        int lm = object->GetLightingMode();
                        const char* lmNames[] = { "None", "Lambert", "Half-Lambert" };
                        if (ImGui::Combo("Lighting Mode", &lm, lmNames, IM_ARRAYSIZE(lmNames))) {
                            object->SetLightingMode(lm);
                        }
                    }
                    ImGui::PopID();
                }
            }  
            
            // planeオブジェクト
            if (selectedDrawType == DRAW_MODEL) {
                const int idx = 0; // plane index
                if (objects3d.size() > static_cast<size_t>(idx) && objects3d[idx]) {
                    Object3d* object = objects3d[idx].get();
                    ImGui::PushID(2000 + idx);
                    if (ImGui::CollapsingHeader("Plane")) {
                        Vector3 scale = object->GetScale();
                        Vector3 rotate = object->GetRotate();
                        Vector3 translate = object->GetTranslate();

                        if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) {
                            object->SetScale(scale);
                        }
                        if (ImGui::DragFloat3("Rotate", &rotate.x, 0.01f)) {
                            object->SetRotate(rotate);
                        }
                        if (ImGui::DragFloat3("Translate", &translate.x, 0.1f)) {
                            object->SetTranslate(translate);
                        }

                        static char texPathBufP[256] = "resources/uvChecker.png";
                        ImGui::InputText("Texture Path", texPathBufP, sizeof(texPathBufP));
                        if (ImGui::Button("Apply Texture")) {
                            object->SetTexture(std::string(texPathBufP));
                        }

                        bool enableLighting = object->GetEnableLighting();
                        if (ImGui::Checkbox("Enable Lighting", &enableLighting)) {
                            object->SetEnableLighting(enableLighting);
                        }

                        int lm = object->GetLightingMode();
                        const char* lmNames[] = { "None", "Lambert", "Half-Lambert" };
                        if (ImGui::Combo("Lighting Mode", &lm, lmNames, IM_ARRAYSIZE(lmNames))) {
                            object->SetLightingMode(lm);
                        }
                    }
                    ImGui::PopID();
                }
            }

            // fenceオブジェクト
            if (selectedDrawType == DRAW_FENCE) {
                const int idx = 3; // fence index
                if (objects3d.size() > idx && objects3d[idx]) {
                    Object3d* object = objects3d[idx].get();
                    ImGui::PushID(1000 + idx);
                    if (ImGui::CollapsingHeader("Fence")) {
                        Vector3 scale = object->GetScale();
                        Vector3 rotate = object->GetRotate();
                        Vector3 translate = object->GetTranslate();

                        if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) {
                            object->SetScale(scale);
                        }
                        if (ImGui::DragFloat3("Rotate", &rotate.x, 0.01f)) {
                            object->SetRotate(rotate);
                        }
                        if (ImGui::DragFloat3("Translate", &translate.x, 0.1f)) {
                            object->SetTranslate(translate);
                        }

                        static char texPathBufF[256] = "resources/uvChecker.png";
                        ImGui::InputText("Texture Path", texPathBufF, sizeof(texPathBufF));
                        if (ImGui::Button("Apply Texture")) {
                            object->SetTexture(std::string(texPathBufF));
                        }

                        bool enableLighting = object->GetEnableLighting();
                        if (ImGui::Checkbox("Enable Lighting", &enableLighting)) {
                            object->SetEnableLighting(enableLighting);
                        }

                        int lm = object->GetLightingMode();
                        const char* lmNames[] = { "None", "Lambert", "Half-Lambert" };
                        if (ImGui::Combo("Lighting Mode", &lm, lmNames, IM_ARRAYSIZE(lmNames))) {
                            object->SetLightingMode(lm);
                        }
                    }
                    ImGui::PopID();
                }
            }

            // bunnyオブジェクト
            if (selectedDrawType == DRAW_BUNNY) {
                const int idx = 1; // bunny index
                if (objects3d.size() > idx && objects3d[idx]) {
                    Object3d* object = objects3d[idx].get();
                    ImGui::PushID(1000 + idx);
                    if (ImGui::CollapsingHeader("Bunny")) {
                        Vector3 scale = object->GetScale();
                        Vector3 rotate = object->GetRotate();
                        Vector3 translate = object->GetTranslate();

                        if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) {
                            object->SetScale(scale);
                        }
                        if (ImGui::DragFloat3("Rotate", &rotate.x, 0.01f)) {
                            object->SetRotate(rotate);
                        }
                        if (ImGui::DragFloat3("Translate", &translate.x, 0.1f)) {
                            object->SetTranslate(translate);
                        }

                        static char texPathBufB[256] = "resources/uvChecker.png";
                        ImGui::InputText("Texture Path", texPathBufB, sizeof(texPathBufB));
                        if (ImGui::Button("Apply Texture")) {
                            object->SetTexture(std::string(texPathBufB));
                        }

                        bool enableLighting = object->GetEnableLighting();
                        if (ImGui::Checkbox("Enable Lighting", &enableLighting)) {
                            object->SetEnableLighting(enableLighting);
                        }

                        int lm = object->GetLightingMode();
                        const char* lmNames[] = { "None", "Lambert", "Half-Lambert" };
                        if (ImGui::Combo("Lighting Mode", &lm, lmNames, IM_ARRAYSIZE(lmNames))) {
                            object->SetLightingMode(lm);
                        }
                    }
                    ImGui::PopID();
                }
            }

            // チェッカー/ティーポットオブジェクト
            if (selectedDrawType == DRAW_CHECKER) {
                const int idx = 2; // teapot/checker index
                if (objects3d.size() > idx && objects3d[idx]) {
                    Object3d* object = objects3d[idx].get();
                    ImGui::PushID(1000 + idx);
                    if (ImGui::CollapsingHeader("Checker/Teapot")) {
                        Vector3 scale = object->GetScale();
                        Vector3 rotate = object->GetRotate();
                        Vector3 translate = object->GetTranslate();

                        if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) {
                            object->SetScale(scale);
                        }
                        if (ImGui::DragFloat3("Rotate", &rotate.x, 0.01f)) {
                            object->SetRotate(rotate);
                        }
                        if (ImGui::DragFloat3("Translate", &translate.x, 0.1f)) {
                            object->SetTranslate(translate);
                        }

                        static char texPathBufC[256] = "resources/uvChecker.png";
                        ImGui::InputText("Texture Path", texPathBufC, sizeof(texPathBufC));
                        if (ImGui::Button("Apply Texture")) {
                            object->SetTexture(std::string(texPathBufC));
                        }

                        bool enableLighting = object->GetEnableLighting();
                        if (ImGui::Checkbox("Enable Lighting", &enableLighting)) {
                            object->SetEnableLighting(enableLighting);
                        }

                        int lm = object->GetLightingMode();
                        const char* lmNames[] = { "None", "Lambert", "Half-Lambert" };
                        if (ImGui::Combo("Lighting Mode", &lm, lmNames, IM_ARRAYSIZE(lmNames))) {
                            object->SetLightingMode(lm);
                        }
                    }
                    ImGui::PopID();
                }
            }

            // Sphereオブジェクト 
            if (selectedDrawType == DRAW_SPHERE) {
                const int idx = 4; // sphere index 
                if (objects3d.size() > idx && objects3d[idx]) {
                    Object3d* object = objects3d[idx].get();
                    ImGui::PushID(1000 + idx);
                    if (ImGui::CollapsingHeader("Sphere")) {
                        Vector3 scale = object->GetScale();
                        Vector3 rotate = object->GetRotate();
                        Vector3 translate = object->GetTranslate();

                        if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) {
                            object->SetScale(scale);
                        }
                        if (ImGui::DragFloat3("Rotate", &rotate.x, 0.01f)) {
                            object->SetRotate(rotate);
                        }
                        if (ImGui::DragFloat3("Translate", &translate.x, 0.1f)) {
                            object->SetTranslate(translate);
                        }

                        static char texPathBufS[256] = "resources/uvChecker.png";
                        ImGui::InputText("Texture Path", texPathBufS, sizeof(texPathBufS));
                        if (ImGui::Button("Apply Texture")) {
                            object->SetTexture(std::string(texPathBufS));
                        }

                        bool enableLighting = object->GetEnableLighting();
                        if (ImGui::Checkbox("Enable Lighting", &enableLighting)) {
                            object->SetEnableLighting(enableLighting);
                        }

                        int lm = object->GetLightingMode();
                        const char* lmNames[] = { "None", "Lambert", "Half-Lambert" };
                        if (ImGui::Combo("Lighting Mode", &lm, lmNames, IM_ARRAYSIZE(lmNames))) {
                            object->SetLightingMode(lm);
                        }
                    }
                    ImGui::PopID();
                }
            }

            // スプライトオブジェクト(2D描画)
            if (selectedDrawType == DRAW_SPRITE || selectedDrawType == DRAW_ALL) {

                // ImGuiのスコープ内で名前を一意にするためのヘルパー
                char nameBuffer[64];

                for (uint32_t i = 0; i < kSpriteCount; i++) {
                    // 現在操作するスプライトのインスタンス
                    Sprite* currentSprite = sprites[i].get();

                    // ヘッダー名にインデックスを付与し、一意にする 
                    sprintf_s(nameBuffer, "Sprite %d", i);

                    // ImGui::PushID(i) を使用して、ループ内のコントロールを個別化 
                    ImGui::PushID(i);

                    if (ImGui::CollapsingHeader(nameBuffer)) {

                        // --- Size ---
                        Vector2 currentSize = currentSprite->GetSize();
                        // ImGui::DragFloat2 の名前からインデックスを削除
                        if (ImGui::DragFloat2("Size", &(currentSize.x), 0.1f)) {
                            currentSprite->SetSize(currentSize);
                        }

                        // --- Rotate ---
                        float currentRotation = currentSprite->GetRotation();
                        // ImGui::DragFloat の名前からインデックスを削除
                        if (ImGui::DragFloat("Rotate.Z", &currentRotation, 0.01f, -6.28f, 6.28f, "%.2f rad")) {
                            currentSprite->SetRotation(currentRotation);
                        }

                        // --- Translate ---
                        Vector2 currentPos = currentSprite->GetPosition();
                        // ImGui::DragFloat2 の名前からインデックスを削除
                        if (ImGui::DragFloat2("Translate", &(currentPos.x), 0.1f)) {
                            currentSprite->SetPosition(currentPos);
                        }

                        // --- Color ---
                        Vector4 currentColor = currentSprite->GetColor();
                        // ImGui::ColorEdit4 の名前からインデックスを削除
                        if (ImGui::ColorEdit4("Color", &(currentColor.x))) {
                            currentSprite->SetColor(currentColor);
                        }

                        // --- Anchor Point ---
                        Vector2 currentAnchor = currentSprite->GetAnchorPoint();
                        if (ImGui::DragFloat2("AnchorPoint", &(currentAnchor.x), 0.01f, 0.0f, 1.0f)) {
                            currentSprite->SetAnchorPoint(currentAnchor);
                        }

                        // --- Flip X / Y ---
                        bool flipX = currentSprite->GetIsFlipX();
                        bool flipY = currentSprite->GetIsFlipY();
                        if (ImGui::Checkbox("FlipX", &flipX)) {
                            currentSprite->SetIsFlipX(flipX);
                        }
                        ImGui::SameLine();
                        if (ImGui::Checkbox("FlipY", &flipY)) {
                            currentSprite->SetIsFlipY(flipY);
                        }

                        // --- Texture LeftTop / Size ---
                        Vector2 texLeftTop = currentSprite->GetTextureLeftTop();
                        Vector2 texSize = currentSprite->GetTextureSize();
                        if (ImGui::DragFloat2("TextureLeftTop", &(texLeftTop.x), 1.0f, 0.0f, 8192.0f)) {
                            currentSprite->SetTextureLeftTop(texLeftTop);
                        }
                        if (ImGui::DragFloat2("TextureSize", &(texSize.x), 1.0f, 1.0f, 8192.0f)) {
                            currentSprite->SetTextureSize(texSize);
                        }
                    }

                    //  PushID と対になる PopID を呼び出す
                    ImGui::PopID();
                }
            }

            // 平行光源
            if (ImGui::CollapsingHeader("Light")) {
                ImGui::ColorEdit4("Color", &directionalLightData->color.x);
                // 方向ベクトルの調整。変な値を入れないよう正規化
                if (ImGui::SliderFloat3("Direction", &directionalLightData->direction.x, -1.0f, 1.0f)) {
                    // コピーしてからじゃないと値が吹き飛ぶので箱を作る
                    auto dir = directionalLightData->direction;
                    // コピーしたものを正規化
                    dir = math.Normalize(dir);
                    // 正規化されたものを代入
                    directionalLightData->direction = dir;
                }
                // 明るさ（制限付き）
                ImGui::SliderFloat("Intensity", &directionalLightData->intensity, 0.0f, 10.0f, "%.2f");

                // ライティング方式はオブジェクト毎に設定してください (Object の項目で編集可)
            }

            // デバッグカメラ
            if (ImGui::CollapsingHeader("DebugCamera")) {
                // カメラ位置をドラッグで調整
                Vector3 camPos = debugCamera.GetTranslation();
                if (ImGui::DragFloat3("Position", &camPos.x, 0.1f)) {
                    debugCamera.SetTranslation(camPos);
                }

                // カメラ回転を角度でスライダー操作
                Vector3 camRot = debugCamera.GetRotation();
                float rotX = camRot.x * 180.0f / 3.14159265f; // ラジアン→度変換
                float rotY = camRot.y * 180.0f / 3.14159265f;
                float rotZ = camRot.z * 180.0f / 3.14159265f;

                bool changed = false;
                changed |= ImGui::SliderAngle("Rotation X", &rotX);
                changed |= ImGui::SliderAngle("Rotation Y", &rotY);
                changed |= ImGui::SliderAngle("Rotation Z", &rotZ);

                if (changed) {
                    // 度→ラジアンに戻してセット
                    camRot.x = rotX * 3.14159265f / 180.0f;
                    camRot.y = rotY * 3.14159265f / 180.0f;
                    camRot.z = rotZ * 3.14159265f / 180.0f;
                    debugCamera.SetRotation(camRot);
                }
            }

            // カメラ操作の有効/無効を切り替えるチェックボックス
            ImGui::Checkbox("Debug Camera Control", &isDebugCameraControl);

            ImGui::End();

            //--------------------
            // 画面のクリア処理(Draw)
            //--------------------

            // 描画前処理
            DirectXCommon::GetInstance()->PreDraw();

            // 描画準備は描画対象ごとに行う（PSO/RootSignatureを切り替えるため）

            // ImGuiの内部コマンドを生成する
            ImGui::Render();

            // 描画対象に応じた処理
            switch (selectedDrawType) {

            case DRAW_PARTICLE:

                // 3D描画の共通設定
                if (object3dCommon) {
                    object3dCommon->SetInstancingDrawSetting();
                    // Draw instanced plane (index 0) 
                    const int idx = 0;
                    if (objects3d.size() > static_cast<size_t>(idx) && objects3d[idx]) {
                        // If model is set on object, prefer Model::DrawInstanced
                        Model* m = objects3d[idx]->GetModel();
                        if (m) {
                            m->DrawInstanced(objects3d[idx].get(), static_cast<uint32_t>(particleCount));
                        } else {
                            // fallback: draw owner mesh instanced
                            objects3d[idx]->Draw();
                        }
                    }
                }

                break;

            case DRAW_MODEL:

                // 3D描画の共通設定
                object3dCommon->SetCommonDrawSetting();

                // Draw only the plane model (index 0)
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

                // Draw only the bunny model (index 1)
                if (objects3d.size() > 1 && objects3d[1]) {
                    objects3d[1]->Draw();
                }

                break;

            case DRAW_FENCE:
                
                // 3D描画の共通設定
                object3dCommon->SetCommonDrawSetting();

                // Draw only the fence model (index 3)
                if (objects3d.size() > 3 && objects3d[3]) {
                    objects3d[3]->Draw();
                }

                break;

            case DRAW_CHECKER:
                
                // 3D描画の共通設定
                object3dCommon->SetCommonDrawSetting();
                
                // Draw only the teapot model (index 2)
                if (objects3d.size() > 2 && objects3d[2]) {
                    objects3d[2]->Draw();
                }

                break;

            case DRAW_SPHERE: 

                // 3D描画の共通設定
                object3dCommon->SetCommonDrawSetting();

                // Draw only the sphere model (index 4)
                if (objects3d.size() > 4 && objects3d[4]) {
                    objects3d[4]->Draw();
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

                // スプライトの描画
                spriteCommon->SetCommonDrawSetting();
                for (uint32_t i = 0; i < kSpriteCount; i++) {
                    sprites[i]->Draw();
                }

                break;
            }

            // 実際のcommandListのImGuiの描画コマンドを積む
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), DirectXCommon::GetInstance()->GetCommandList());

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