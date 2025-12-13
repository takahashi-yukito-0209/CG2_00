#include <Windows.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
// #include <format>
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "mathUtility.h"
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
#include <numbers>
#include <sstream>
#include <vector>
#include <wrl.h>
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

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

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
    assert(file.is_open());

    // 2..wavデータ読み込み

    // RIFFヘッダーの読み込み
    RiffHeader riff;
    file.read((char*)&riff, sizeof(riff));
    // ファイルがRIFFかチェック
    if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
        assert(0);
    }
    // タイプがWAVEかチェック
    if (strncmp(riff.type, "WAVE", 4) != 0) {
        assert(0);
    }

    // Formatチャンクの読み込み
    FormatChunk format = {};
    // チャンクヘッダーの確認
    file.read((char*)&format, sizeof(ChunkHeader));
    if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
        assert(0);
    }

    // チャンク本体の読み込み
    assert(format.chunk.size <= sizeof(format.fmt));
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
        assert(0);
    }

    // Dataチャンクのデータ部(波型データ)の読み込み
    char* pBuffer = new char[data.size];
    file.read(pBuffer, data.size);

    // 3.ファイルクローズ

    // waveファイルを閉じる
    file.close();

    // 4.読み込んだ音声データをreturn

    // returnするための音声データ
    SoundData soundData = {};

    soundData.wfex = format.fmt;
    soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
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
    // 現在時刻を取得（UTC時刻）
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    // ログファイルの名前にコンマ何秒はいらないので削って秒にする
    std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
        nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    // 日本時間（PCの設定時間）に変更
    std::chrono::zoned_time localTime { std::chrono::current_zone(), nowSeconds };
    // formatを使って年月日_時分秒の文字列に変換
    std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
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

    /*HRESULT hr;*/

    // COMライブラリの初期化
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // XAudioエンジンのインスタンスを生成
    result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    // マスターボイスを生成
    result = xAudio2->CreateMasteringVoice(&masterVoice);

    // 音声読み込み
    SoundData soundData1 = SoundLoadWave("resources/mokugyo.wav");

    // DirectInputの初期化
    IDirectInput8* directInput = nullptr;
    result = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
    assert(SUCCEEDED(result));

    // 初期化時
    InputManager::GetInstance()->Initialize(directInput, hwnd);

    // DirectXの初期化
    DirectXCommon::GetInstance()->Initialize(&winApp);

#pragma region 基盤システムの初期化

    SpriteCommon* spriteCommon = nullptr;
    // スプライト共通部の初期化
    spriteCommon = new SpriteCommon();
    spriteCommon->Initialize(DirectXCommon::GetInstance());

    //テクスチャマネージャーの初期化
    TextureManager::GetInstance()->Initialize();

    Object3dCommon* object3dCommon = nullptr;
    // 3Dオブジェクト共通部の初期化
    object3dCommon = new Object3dCommon();
    object3dCommon->Initialize(DirectXCommon::GetInstance());

#pragma endregion 基盤システムの初期化

    // Textureを読んで転送する
    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");

#pragma region 最初のシーンの初期化
    
    //スプライト複数設定
    std::vector<Sprite*> sprites;
    const uint32_t kSpriteCount = 5;
    std::array<std::string, 2> spriteNames = {
        "resources/uvChecker",
        "resources/monsterBall",
    };

    for (uint32_t i = 0; i < kSpriteCount; ++i) {
        if (i / 2 == 0) {             
            Sprite* sprite = new Sprite();
            sprite->Initialize(spriteCommon, spriteNames[0] + ".png");
            sprites.push_back(sprite);
        } else {
            Sprite* sprite = new Sprite();
            sprite->Initialize(spriteCommon, spriteNames[1] + ".png");
            sprites.push_back(sprite);
        }
    }

    // 3dオブジェクト初期化
    Object3d* object3d = new Object3d();
    object3d->Initialize(object3dCommon);

#pragma endregion 最初のシーンの終了

    // 自作した数学関数の使用
    MathUtility math;

    
    // マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(Material));
    // マテリアルにデータを書き込む
    Material* materialData = nullptr;
    // 書き込むためのアドレスを取得
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    // 今回は赤を書き込んでみる
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    // SpriteはLightingするのでtrueを設定する
    materialData->enableLighting = true;
    // UVTransformは単位行列で初期化
    materialData->uvTransform = math.MakeIdentity4x4();

    // 平行光源用のResourceを作る
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(DirectionalLight));
    // データを書き込む
    DirectionalLight* directionalLightData = nullptr;
    // 書き込むためのアドレスを取得
    directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
    directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
    directionalLightData->intensity = 1.0f;
    directionalLightData->direction = math.Normalize(directionalLightData->direction);

    // WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
    Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(TransformationMatrix));
    // データを書き込む
    TransformationMatrix* wvpData = nullptr;
    // 書き込むためのアドレスを取得
    wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
    // 単位行列を書き込んでおく
    wvpData->WVP = math.MakeIdentity4x4();
    wvpData->World = math.MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

    // 全てのロード完了後、まとめて転送を実行
    TextureManager::GetInstance()->ExecuteResourceUpload();

    // 描画対象をUIで切り替えるための変数と選択肢
    enum DrawType {
        DRAW_NONE,
        DRAW_SPHERE,
        DRAW_MODEL,
        DRAW_SPRITE,
        DRAW_BUNNY,
        DRAW_CHECKER,
        DRAW_ALL
    };

    DrawType selectedDrawType = DRAW_SPRITE; // 初期値

    const char* drawOptions[] = {
        "None", // 何も描画しない
        "Sphere", // 球体の描画
        "Model", // モデルのみ描画
        "Sprite", // スプライトのみ描画
        "Bunny", // bunnyのみ描画
        "Checker", // ティーポットのみを描画
        "All" // 両方描画
    };

    enum LightingMode {
        Lighting_None = 0,
        Lighting_Lambert,
        Lighting_HalfLambert,
    };

    int lightingMode = Lighting_HalfLambert; // 初期値

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
            // CBufferの中身更新
            wvpData->WVP = worldViewProjectionMatrix;
            wvpData->World = worldMatrix;

            // Object3Dの更新
            if (object3d) {
                object3d->Update(viewMatrix, projectionMatrix);
            }

            //スプライトの更新
            for (uint32_t i = 0; i <kSpriteCount; i++) {
                sprites[i]->Update();
            }

            // imguiの項目内容
            ImGui::Begin("Settings");

            // ImGuiのUIで描画対象を選択
            ImGui::Combo("Model", (int*)&selectedDrawType, drawOptions, IM_ARRAYSIZE(drawOptions));

            // メインオブジェクト
            if (selectedDrawType == DRAW_MODEL || selectedDrawType == DRAW_ALL) {
                if (ImGui::CollapsingHeader("Object(Main)")) {
                    ImGui::DragFloat3("Scale##Object", &transform.scale.x, 0.01f);
                    ImGui::DragFloat3("Rotate##Object", &transform.rotate.x, 0.01f);
                    ImGui::DragFloat3("Translate##Object", &transform.translate.x, 0.01f);
                    ImGui::ColorEdit4("Color##Object", &(materialData->color.x));
                }
            }

            // スプライトオブジェクト(2D描画)
            if (selectedDrawType == DRAW_SPRITE || selectedDrawType == DRAW_ALL) {

                // ImGuiのスコープ内で名前を一意にするためのヘルパー
                // (名前を生成する用途はCollapsingHeaderに限定するため、char配列は必須)
                char nameBuffer[64];

                for (uint32_t i = 0; i < kSpriteCount; i++) {
                    // 現在操作するスプライトのインスタンス
                    Sprite* currentSprite = sprites[i];

                    // ヘッダー名にインデックスを付与し、一意にする (例: "Sprite 0", "Sprite 1", ...)
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

                        // --- Texture region (left-top and size in pixels) ---
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

                // ライティング方式
                ImGui::Text("Lighting Mode");
                ImGui::RadioButton("None", &lightingMode, 0);
                ImGui::RadioButton("Lambert", &lightingMode, 1);
                ImGui::RadioButton("Half Lambert", &lightingMode, 2);
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

            // 3Dオブジェクトの描画準備。3Dオブジェクトの描画に共通のグラフィクスコマンドを積む
            object3dCommon->SetCommonDrawSetting();

            // Sprite描画準備。Spriteの描画に共通のグラフィクスコマンドを積む
            spriteCommon->SetCommonDrawSetting();

            // ImGuiの内部コマンドを生成する
            ImGui::Render();

            // 描画対象に応じた処理
            switch (selectedDrawType) {

            case DRAW_NONE:
                // 何も描画しない（スキップ）
                break;

            case DRAW_SPHERE:

                break;

            case DRAW_MODEL:

                // オブジェクトの描画
                if (object3d) {
                    object3d->Draw();
                }

                break;

            case DRAW_SPRITE:

                // スプライトの描画
                for (uint32_t i = 0; i < kSpriteCount; i++) {
                    // 描画に使用するテクスチャのGPUハンドルを取得して渡す
                    if (i / 2 == 0) {
                        // 偶数番目のスプライトはモンスターボールテクスチャ
                        sprites[i]->Draw();
                    } else {
                        // 奇数番目のスプライトはチェッカーテクスチャ
                        sprites[i]->Draw();
                    }
                }

                break;

            case DRAW_BUNNY:


                break;

            case DRAW_CHECKER:


                break;

            case DRAW_ALL:

                // スプライトの描画
                for (uint32_t i = 0; i < kSpriteCount; i++) {
                    // 描画に使用するテクスチャのGPUハンドルを取得して渡す
                    if (i / 2 == 0) {
                        // 偶数番目のスプライトはモンスターボールテクスチャ
                        sprites[i]->Draw();
                    } else {
                        // 奇数番目のスプライトはチェッカーテクスチャ
                        sprites[i]->Draw();
                    }
                }

                // オブジェクトの描画
                if (object3d) {
                    object3d->Draw();
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

    // DirectX のシステム系
    DirectXCommon::GetInstance()->Finalize(); 

    // 自作リソース解放
    delete object3dCommon;
    delete object3d;

    TextureManager::GetInstance()->Finalize();

    delete spriteCommon;
    for (uint32_t i = 0; i < kSpriteCount; i++) {
        delete sprites[i];
    }

    // 音・入力など DirectX 依存していないもの
    xAudio2.Reset();
    SoundUnload(&soundData1);

    InputManager::GetInstance()->Finalize();
    directInput->Release();

    // OS側のウィンドウ破棄
    winApp.Finalize();

    // COMの終了処理
    CoUninitialize();

    return 0;
}