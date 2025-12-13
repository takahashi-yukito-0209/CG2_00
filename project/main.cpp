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

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

using namespace MyEngine;

struct DirectionalLight {
    Vector4 color; //!< ライトの色
    Vector3 direction; //!< ライトの向き
    float intensity; //!< 輝度
};

struct MaterialData {
    std::string textureFilePath;
};

struct ModelData {
    std::vector<VertexData> vertices;
    MaterialData material;
};

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

MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
    // 1.中で必要となる変数の宣言
    MaterialData materialData; // 構築するMAterialData
    std::string line; // ファイルから読んだ1行を格納するもの

    // 2.ファイルを開く
    std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
    assert(file.is_open()); // とりあえず開けなかったら止める

    // 3.実際にファイルを読み、MaterialDataを構築していく
    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;

        // identifierに応じた処理
        if (identifier == "map_Kd") {
            std::string textureFilename;
            s >> textureFilename;
            // 連結してファイルパスにする
            materialData.textureFilePath = directoryPath + "/" + textureFilename;
        }
    }

    // 4.MaterialDataを返す
    return materialData;
}

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
    // 1.中で必要となる変数の宣言
    ModelData modelData; // 構築するModelData
    std::vector<Vector4> positions; // 位置
    std::vector<Vector3> normals; // 法線
    std::vector<Vector2> texcoords; // テクスチャ座標
    std::string line; // ファイルから読んだ1行を格納するもの

    // 2.ファイルを開く
    std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
    assert(file.is_open()); // とりあえず開けなかったら止める

    // 3.実際にファイルを読み、ModelDataを構築していく
    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier; // 先頭の識別子を読む

        // identifierに応じた処理
        if (identifier == "v") {
            Vector4 position;
            s >> position.x >> position.y >> position.z;
            position.w = 1.0f;
            position.x *= -1.0f;
            positions.push_back(position);

        } else if (identifier == "vt") {
            Vector2 texcoord;
            s >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y;
            texcoords.push_back(texcoord);

        } else if (identifier == "vn") {
            Vector3 normal;
            s >> normal.x >> normal.y >> normal.z;
            normal.x *= -1.0f;
            normals.push_back(normal);

        } else if (identifier == "f") {
            VertexData triangle[3];
            // 面は三角形限定。その他は未対応
            for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
                std::string vertexDefinition;
                s >> vertexDefinition;
                // 頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
                std::stringstream v(vertexDefinition);
                uint32_t elementIndices[3];
                for (int32_t element = 0; element < 3; ++element) {
                    std::string index;
                    std::getline(v, index, '/'); // 区切りでインデックスを読んでいく
                    elementIndices[element] = std::stoi(index);
                }
                // 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
                Vector4 position = positions[elementIndices[0] - 1];
                Vector2 texcoord = texcoords[elementIndices[1] - 1];
                Vector3 normal = normals[elementIndices[2] - 1];
                triangle[faceVertex] = { position, texcoord, normal };
            }

            // 頂点を逆順で登録することで、回り順を逆にする
            modelData.vertices.push_back(triangle[2]);
            modelData.vertices.push_back(triangle[1]);
            modelData.vertices.push_back(triangle[0]);

        } else if (identifier == "mtllib") {
            // materialTemplateLibraryファイルの名前を取得する
            std::string materialFilename;
            s >> materialFilename;
            // 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
            modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
        }
    }

    // 4.ModelDataを返す
    return modelData;
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
Transform cameraTransform = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -10.0f } };
Transform sphereTransform = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
Transform transformChecker = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } };
Transform transformSprite = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
Transform uvTransformSprite = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
Transform transformBunny = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } };

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

    HRESULT hr;

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

#pragma endregion 最初のシーンの終了

    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
    descriptorRange[0].NumDescriptors = 1; // 数は1つ
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算

    // RootSignature作成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature {};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // RootParameter作成。複数設定できるので配列。
    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
    rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VertexShadefで使う
    rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号0を使う
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // Tableで利用する数
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
    rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号1を使う

    descriptionRootSignature.pParameters = rootParameters; // ルートパラメーター配列へのポインタ
    descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 倍リニアフィルタ
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
    staticSamplers[0].ShaderRegister = 0; // レジスタ番号0を使う
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    // シリアライズしてバイナリにする
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }

    // バイナリをもとに生成
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
    hr = DirectXCommon::GetInstance()->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));

    // InputLayout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc {};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // BlendStateの設定
    D3D12_BLEND_DESC blendDesc {};
    // すべての色要素を書き込む
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // RasiterzerStateの設定
    D3D12_RASTERIZER_DESC rasterizerDesc {};
    // 裏面（時計回り）を表示しない
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    // 三角形の中を塗りつぶす
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // Shaderをコンパイルする
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = DirectXCommon::GetInstance()->CompileShader(L"resources/shaders/Object3D.VS.hlsl", L"vs_6_0");
    assert(vertexShaderBlob != nullptr);

    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = DirectXCommon::GetInstance()->CompileShader(L"resources/shaders/Object3D.PS.hlsl", L"ps_6_0");
    assert(pixelShaderBlob != nullptr);

    // PSOを生成する
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc {};
    graphicsPipelineStateDesc.pRootSignature = rootSignature.Get(); // RootSignature
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() }; // VertexShader
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() }; // PixelShader
    graphicsPipelineStateDesc.BlendState = blendDesc; // BlendState
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState
    // 書き込むRTVの情報
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    // 利用するトロポジ（形状）のタイプ。三角形
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // どのように画面に色を打ち込むかの設定（気にしなくて良い）
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // DepthStencilStateの設定
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    // Depthの機能を有効化する
    depthStencilDesc.DepthEnable = true;
    // 書き込みします
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    // 比較関数はLessEqual。つまり、近ければ描画される
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // DepthStencilの設定
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // 実際に生成
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;
    hr = DirectXCommon::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
    assert(SUCCEEDED(hr));

    // 自作した数学関数の使用
    MathUtility math;

    constexpr uint32_t kSubdivision = 16; // 分割数
    constexpr uint32_t kVertexCount = (kSubdivision + 1) * (kSubdivision + 1); // 頂点数
    constexpr uint32_t kIndexCount = kSubdivision * kSubdivision * 6; // インデックス数

    // 頂点リソースの作成
    Microsoft::WRL::ComPtr<ID3D12Resource> sphereVertexResource = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(VertexData) * kVertexCount);

    // 頂点バッファビューの作成
    D3D12_VERTEX_BUFFER_VIEW sphereVertexBufferView {};
    sphereVertexBufferView.BufferLocation = sphereVertexResource->GetGPUVirtualAddress();
    sphereVertexBufferView.SizeInBytes = sizeof(VertexData) * kVertexCount;
    sphereVertexBufferView.StrideInBytes = sizeof(VertexData);

    // 書き込み用のアドレスを取得
    VertexData* sphereVertexData = nullptr;
    sphereVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereVertexData));

    // 経度と緯度の1分割あたりの角度を計算
    const float sphereLonStep = 2.0f * std::numbers::pi_v<float> / kSubdivision;
    const float sphereLatStep = std::numbers::pi_v<float> / kSubdivision;

    // 球体の各頂点を計算
    for (uint32_t lat = 0; lat <= kSubdivision; ++lat) {
        float latAngle = -std::numbers::pi_v<float> / 2.0f + sphereLatStep * lat;
        float y = sinf(latAngle);
        float r = cosf(latAngle);

        for (uint32_t lon = 0; lon <= kSubdivision; ++lon) {
            float lonAngle = lon * sphereLonStep;
            float x = r * cosf(lonAngle);
            float z = r * sinf(lonAngle);

            uint32_t index = lat * (kSubdivision + 1) + lon;

            // 頂点情報を格納
            sphereVertexData[index].position = { x, y, z, 1.0f };
            sphereVertexData[index].normal = { x, y, z };
            sphereVertexData[index].texcoord = {
                lon / static_cast<float>(kSubdivision),
                1.0f - lat / static_cast<float>(kSubdivision)
            };
        }
    }

    // インデックスリソースの作成
    Microsoft::WRL::ComPtr<ID3D12Resource> sphereIndexResource = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(uint32_t) * kIndexCount);

    // インデックスバッファビューの作成
    D3D12_INDEX_BUFFER_VIEW sphereIndexBufferView {};
    sphereIndexBufferView.BufferLocation = sphereIndexResource->GetGPUVirtualAddress();
    sphereIndexBufferView.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    sphereIndexBufferView.Format = DXGI_FORMAT_R32_UINT;

    // 書き込み用のアドレスを取得
    uint32_t* sphereIndexData = nullptr;
    sphereIndexResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereIndexData));

    // 各四角形を2つの三角形に分けてインデックスを指定
    uint32_t i = 0;
    for (uint32_t lat = 0; lat < kSubdivision; ++lat) {
        for (uint32_t lon = 0; lon < kSubdivision; ++lon) {
            uint32_t current = lat * (kSubdivision + 1) + lon;
            uint32_t next = (lat + 1) * (kSubdivision + 1) + lon;

            // 三角形1
            sphereIndexData[i++] = current;
            sphereIndexData[i++] = next;
            sphereIndexData[i++] = next + 1;

            // 三角形2
            sphereIndexData[i++] = current;
            sphereIndexData[i++] = next + 1;
            sphereIndexData[i++] = current + 1;
        }
    }

    // マテリアルリソースの作成
    Microsoft::WRL::ComPtr<ID3D12Resource> sphereMaterialResource = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(Material));

    // マテリアルデータのマッピング
    Material* sphereMaterialData = nullptr;
    sphereMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereMaterialData));

    // マテリアル情報を設定
    sphereMaterialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    sphereMaterialData->enableLighting = true;
    sphereMaterialData->uvTransform = math.MakeIdentity4x4();

    // WVP行列用リソースを作成
    Microsoft::WRL::ComPtr<ID3D12Resource> sphereWvpResource = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(TransformationMatrix));

    // 書き込み用ポインタを取得
    TransformationMatrix* sphereWvpData = nullptr;
    sphereWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereWvpData));

    // モデル読み込み
    ModelData modelData = LoadObjFile("resources", "plane.obj");

    // 頂点リソースの作成
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());

    // 頂点バッファビューの作成
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
    // リソースの先頭アドレスから使う
    vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
    // 使用するリソースのサイズ
    vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
    // 1頂点当たりのサイズ
    vertexBufferView.StrideInBytes = sizeof(VertexData);

    // 頂点データ書き込み
    VertexData* vertexData = nullptr;
    // 書き込むためのアドレスを取得
    vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

    // インデックスリソースの作成
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(uint32_t) * kIndexCount);

    // インデックスバッファビューの作成
    D3D12_INDEX_BUFFER_VIEW indexBufferView {};
    // リソースの先頭アドレスから使う
    indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
    // 使用するリソースのサイズ
    indexBufferView.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    // 1頂点当たりのサイズ
    indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    // インデックスデータ書き込み
    uint32_t* indexData = nullptr;
    // 書き込むためのアドレスを取得
    indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));

    i = 0; // インデックス配列の書き込み位置
    // 緯度方向
    for (uint32_t lat = 0; lat < kSubdivision; ++lat) {
        // 経度方向
        for (uint32_t lon = 0; lon < kSubdivision; ++lon) {
            // 現在の頂点インデックス
            uint32_t current = lat * (kSubdivision + 1) + lon;
            // 次の行の同じ列の頂点インデックス
            uint32_t next = (lat + 1) * (kSubdivision + 1) + lon;

            // --- 1枚目の三角形 ---
            indexData[i++] = current; // 左上
            indexData[i++] = next; // 左下
            indexData[i++] = next + 1; // 右下

            // --- 2枚目の三角形 ---
            indexData[i++] = current; // 左上
            indexData[i++] = next + 1; // 右下
            indexData[i++] = current + 1; // 右上
        }
    }

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

    // Sprite用のマテリアルリソースを作る
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(Material));
    // マテリアルにデータを書き込む
    Material* materialDataSprite = nullptr;
    // 書き込むためのアドレスを取得
    materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
    // 今回は白を書き込んでみる
    materialDataSprite->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    // SpriteはLightingしないのでfalseを設定する
    materialDataSprite->enableLighting = false;
    // UVTransformは単位行列で初期化
    materialDataSprite->uvTransform = math.MakeIdentity4x4();

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

    // モデル読み込み
    ModelData bunnyModel = LoadObjFile("resources", "bunny.obj");

    // 頂点リソースを作成
    auto vertexResourceBunny = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(VertexData) * bunnyModel.vertices.size());
    // 頂点バッファビューを設定
    D3D12_VERTEX_BUFFER_VIEW vertexBufferViewBunny {};
    // リソースの先頭アドレスから使う
    vertexBufferViewBunny.BufferLocation = vertexResourceBunny->GetGPUVirtualAddress();
    // 使用するリソースのサイズ
    vertexBufferViewBunny.SizeInBytes = UINT(sizeof(VertexData) * bunnyModel.vertices.size());
    // 1頂点のサイズ
    vertexBufferViewBunny.StrideInBytes = sizeof(VertexData);

    // バッファに頂点データを書き込む
    VertexData* vertexDataBunny = nullptr;
    vertexResourceBunny->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataBunny));
    std::memcpy(vertexDataBunny, bunnyModel.vertices.data(), sizeof(VertexData) * bunnyModel.vertices.size());

    // マテリアルリソースを作成
    auto materialResourceBunny = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(Material));
    // マテリアルに色やLighting設定を記述
    Material* materialDataBunny = nullptr;
    materialResourceBunny->Map(0, nullptr, reinterpret_cast<void**>(&materialDataBunny));
    materialDataBunny->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialDataBunny->enableLighting = true;
    materialDataBunny->uvTransform = math.MakeIdentity4x4();

    // WVPリソースを作成
    auto wvpResourceBunny = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(TransformationMatrix));
    TransformationMatrix* wvpDataBunny = nullptr;
    wvpResourceBunny->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataBunny));

    // モデルの読み込み
    ModelData checkerModel = LoadObjFile("resources", "teapot.obj");

    // 頂点リソースを作成
    auto vertexResourceChecker = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(VertexData) * checkerModel.vertices.size());
    // 頂点バッファビューを設定
    D3D12_VERTEX_BUFFER_VIEW vertexBufferViewChecker {};
    // リソースの先頭アドレスから使う
    vertexBufferViewChecker.BufferLocation = vertexResourceChecker->GetGPUVirtualAddress();
    // 使用するリソースのサイズ
    vertexBufferViewChecker.SizeInBytes = UINT(sizeof(VertexData) * checkerModel.vertices.size());
    // 1頂点のサイズ
    vertexBufferViewChecker.StrideInBytes = sizeof(VertexData);

    // バッファに頂点データを書き込む
    VertexData* vertexDataChecker = nullptr;
    vertexResourceChecker->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataChecker));
    std::memcpy(vertexDataChecker, checkerModel.vertices.data(), sizeof(VertexData) * checkerModel.vertices.size());

    // マテリアルリソースを作成
    auto materialResourceChecker = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(Material));
    // マテリアルに色やLighting設定を記述
    Material* materialDataChecker = nullptr;
    materialResourceChecker->Map(0, nullptr, reinterpret_cast<void**>(&materialDataChecker));
    materialDataChecker->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialDataChecker->enableLighting = true;
    materialDataChecker->uvTransform = math.MakeIdentity4x4();

    // WVPリソースを作成
    auto wvpResourceChecker = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(TransformationMatrix));
    TransformationMatrix* wvpDataChecker = nullptr;
    wvpResourceChecker->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataChecker));
    wvpDataChecker->WVP = math.MakeIdentity4x4();
    wvpDataChecker->World = math.MakeAffineMatrix(
        transformChecker.scale, transformChecker.rotate, transformChecker.translate);

    // Textureを読んで転送する
    /*TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
    TextureManager::GetInstance()->LoadTexture(bunnyModel.material.textureFilePath);
    TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");*/

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

            // WorldMatrix作成(sphere)
            Matrix4x4 sphereWorld = math.MakeAffineMatrix(
                sphereTransform.scale, sphereTransform.rotate, sphereTransform.translate);
            Matrix4x4 sphereView = debugCamera.GetViewMatrix();
            Matrix4x4 sphereProj = debugCamera.GetProjectionMatrix();
            // WVPMatrixを作る
            Matrix4x4 sphereWvp = math.Multiply(sphereWorld, math.Multiply(sphereView, sphereProj));
            // CBufferの中身更新
            sphereWvpData->World = sphereWorld;
            sphereWvpData->WVP = sphereWvp;

            // WorldMatrix作成(model)
            Matrix4x4 worldMatrix = math.MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
            Matrix4x4 viewMatrix = debugCamera.GetViewMatrix();
            Matrix4x4 projectionMatrix = debugCamera.GetProjectionMatrix();
            // WVPMatrixを作る
            Matrix4x4 worldViewProjectionMatrix = math.Multiply(worldMatrix, math.Multiply(viewMatrix, projectionMatrix));
            // CBufferの中身更新
            wvpData->WVP = worldViewProjectionMatrix;
            wvpData->World = worldMatrix;

            // WorldMatrix作成(bunny)
            Matrix4x4 worldMatrixBunny = math.MakeAffineMatrix(transformBunny.scale, transformBunny.rotate, transformBunny.translate);
            Matrix4x4 viewMatrixBunny = debugCamera.GetViewMatrix();
            Matrix4x4 projectionMatrixBunny = debugCamera.GetProjectionMatrix();
            // WVPMatrixを作る
            Matrix4x4 wvpMatrixBunny = math.Multiply(worldMatrixBunny, math.Multiply(viewMatrixBunny, projectionMatrixBunny));
            // CBufferの中身更新
            wvpDataBunny->World = worldMatrixBunny;
            wvpDataBunny->WVP = wvpMatrixBunny;

            // WorldMatrix作成(Checker)
            Matrix4x4 worldMatrixChecker = math.MakeAffineMatrix(transformChecker.scale, transformChecker.rotate, transformChecker.translate);
            Matrix4x4 viewMatrixChecker = debugCamera.GetViewMatrix();
            Matrix4x4 projectionMatrixChecker = debugCamera.GetProjectionMatrix();
            // WVPMatrixを作る
            Matrix4x4 wvpMatrixChecker = math.Multiply(worldMatrixChecker, math.Multiply(viewMatrixChecker, projectionMatrixChecker));
            // CBufferの中身更新
            wvpDataChecker->World = worldMatrixChecker;
            wvpDataChecker->WVP = wvpMatrixChecker;

            //スプライトの更新
            for (uint32_t i = 0; i <kSpriteCount; i++) {
                sprites[i]->Update();
            }

            // imguiの項目内容
            ImGui::Begin("Settings");

            // ImGuiのUIで描画対象を選択
            ImGui::Combo("Model", (int*)&selectedDrawType, drawOptions, IM_ARRAYSIZE(drawOptions));

            // カメラ
            if (ImGui::CollapsingHeader("Camera")) {
                ImGui::DragFloat3("Translate##Camera", &(cameraTransform.translate.x));
                ImGui::SliderAngle("Rotate.x##Camera", &cameraTransform.rotate.x);
                ImGui::SliderAngle("Rotate.y##Camera", &cameraTransform.rotate.y);
                ImGui::SliderAngle("Rotate.z##Camera", &cameraTransform.rotate.z);
            }

            // 球体
            if (selectedDrawType == DRAW_SPHERE || selectedDrawType == DRAW_ALL) {
                if (ImGui::CollapsingHeader("Object(Sphere)")) {
                    ImGui::DragFloat3("Scale##Sphere", &sphereTransform.scale.x, 0.01f);
                    ImGui::DragFloat3("Rotate##Sphere", &sphereTransform.rotate.x, 0.01f);
                    ImGui::DragFloat3("Translate##Sphere", &sphereTransform.translate.x, 0.01f);
                    ImGui::ColorEdit4("Color##Sphere", &(sphereMaterialData->color.x));
                }
            }

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

            // bunny
            if (selectedDrawType == DRAW_BUNNY || selectedDrawType == DRAW_ALL) {
                if (ImGui::CollapsingHeader("Object(Bunny)")) {
                    ImGui::DragFloat3("Scale##Bunny", &(transformBunny.scale.x), 0.1f);
                    ImGui::DragFloat3("Rotate##Bunny", &(transformBunny.rotate.x), 0.1f);
                    ImGui::DragFloat3("Translate##Bunny", &(transformBunny.translate.x), 0.1f);
                    ImGui::ColorEdit4("Color##Bunny", &(materialDataBunny->color.x));
                }
            }

            // ティーポット
            if (selectedDrawType == DRAW_CHECKER || selectedDrawType == DRAW_ALL) {
                if (ImGui::CollapsingHeader("Object(Checker)")) {
                    ImGui::DragFloat3("Scale##Checker", &(transformChecker.scale.x), 0.1f);
                    ImGui::DragFloat3("Rotate##Checker", &(transformChecker.rotate.x), 0.1f);
                    ImGui::DragFloat3("Translate##Checker", &(transformChecker.translate.x), 0.1f);
                    ImGui::ColorEdit4("Color##Checker", &(materialDataChecker->color.x));
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

            // UVTransform
            if (ImGui::CollapsingHeader("UVTransform")) {
                ImGui::DragFloat2("Translate##UV", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
                ImGui::DragFloat2("Scale##UV", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
                ImGui::SliderAngle("Rotate##UV", &uvTransformSprite.rotate.z);
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

            // ImGuiのLighting設定をmaterialDataに反映する（毎フレーム）
            materialDataBunny->enableLighting = (lightingMode != 0);
            materialDataBunny->lightingMode = lightingMode;

            materialDataChecker->enableLighting = (lightingMode != 0);
            materialDataChecker->lightingMode = lightingMode;

            sphereMaterialData->enableLighting = (lightingMode != 0);
            sphereMaterialData->lightingMode = lightingMode;

            materialData->enableLighting = (lightingMode != 0);
            materialData->lightingMode = lightingMode;

            //--------------------
            // 画面のクリア処理(Draw)
            //--------------------

            // 描画前処理
            DirectXCommon::GetInstance()->PreDraw();

            // Sprite描画準備。Spriteの描画に共通のグラフィクスコマンドを積む
            spriteCommon->SetCommonDrawSetting();

            // ImGuiの内部コマンドを生成する
            ImGui::Render();

            DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());
            DirectXCommon::GetInstance()->GetCommandList()->SetPipelineState(graphicsPipelineState.Get()); // PSOを設定
            DirectXCommon::GetInstance()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 形状を設定

            // 描画対象に応じた処理
            switch (selectedDrawType) {

            case DRAW_NONE:
                // 何も描画しない（スキップ）
                break;

            case DRAW_SPHERE:

                //// ===================== 球体モデルの描画 ===================== //
                //dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &sphereVertexBufferView); // VBVを設定
                //dxCommon->GetCommandList()->IASetIndexBuffer(&sphereIndexBufferView); // IBVを設定
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, sphereMaterialResource->GetGPUVirtualAddress()); // マテリアル
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, sphereWvpResource->GetGPUVirtualAddress()); // WVP行列
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress()); // 光源
                //dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU); // SRVを設定
                //dxCommon->GetCommandList()->DrawIndexedInstanced(kIndexCount, 1, 0, 0, 0); // モデル描画

                break;

            case DRAW_MODEL:

                //// ===================== モデルの描画 ===================== //
                //dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView); // VBVを設定
                //dxCommon->GetCommandList()->IASetIndexBuffer(&indexBufferView); // IBVを設定
                //// マテリアルCBufferの場所を指定
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
                //// wvp用のCBufferの場所を設定
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
                //// 平面光源用のCBufferの場所を設定
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
                //// SRVのDescriptorTableの先頭を指定。2はrootParameter[2]である。
                //dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU2);
                //// 描画！（DrawCall/ドローコール）。3頂点で1つのインスタンス。インスタンスについては今後
                //dxCommon->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);

                break;

            case DRAW_SPRITE:

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

                //// ===================== bunny.obj の描画 ===================== //
                //dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewBunny); // VBVを設定
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResourceBunny->GetGPUVirtualAddress()); // マテリアル
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResourceBunny->GetGPUVirtualAddress()); // WVP行列
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress()); // 光源
                //dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU3); // SRV
                //dxCommon->GetCommandList()->DrawInstanced(static_cast<UINT>(bunnyModel.vertices.size()), 1, 0, 0); // モデル描画

                break;

            case DRAW_CHECKER:

                //// ティーポット描画
                //dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewChecker);
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResourceChecker->GetGPUVirtualAddress());
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResourceChecker->GetGPUVirtualAddress());
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
                //dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU4);
                //dxCommon->GetCommandList()->DrawInstanced(UINT(checkerModel.vertices.size()), 1, 0, 0);

                break;

            case DRAW_ALL:

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

                //// ===================== 球体モデルの描画 ===================== //
                //dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &sphereVertexBufferView); // VBVを設定
                //dxCommon->GetCommandList()->IASetIndexBuffer(&sphereIndexBufferView); // IBVを設定
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, sphereMaterialResource->GetGPUVirtualAddress()); // マテリアル
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, sphereWvpResource->GetGPUVirtualAddress()); // WVP行列
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress()); // 光源
                //dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU); // SRVを設定
                //dxCommon->GetCommandList()->DrawIndexedInstanced(kIndexCount, 1, 0, 0, 0); // モデル描画

                //// ===================== モデルの描画 ===================== //
                //dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView); // VBVを設定
                //dxCommon->GetCommandList()->IASetIndexBuffer(&indexBufferView); // IBVを設定
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress()); // マテリアル
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress()); // WVP行列
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress()); // 光源
                //dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU2); // SRV
                //dxCommon->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0); // モデル描画

                //// ===================== bunny.obj の描画 ===================== //
                //dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewBunny); // VBVを設定
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResourceBunny->GetGPUVirtualAddress()); // マテリアル
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResourceBunny->GetGPUVirtualAddress()); // WVP行列
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress()); // 光源
                //dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU3); // SRV
                //dxCommon->GetCommandList()->DrawInstanced(static_cast<UINT>(bunnyModel.vertices.size()), 1, 0, 0); // モデル描画

                //// ティーポット描画
                //dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewChecker);
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResourceChecker->GetGPUVirtualAddress());
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResourceChecker->GetGPUVirtualAddress());
                //dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
                //dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU4);
                //dxCommon->GetCommandList()->DrawInstanced(UINT(checkerModel.vertices.size()), 1, 0, 0);

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