#include "WinApp.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"

#include <cassert>
#include <comdef.h> // _com_error 用
#include <d3d12sdklayers.h>
#include <externals/DirectXTex/d3dx12.h>

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <thread>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

using namespace Microsoft::WRL;
using namespace MyEngine;

// 静的インスタンスを定義
DirectXCommon* DirectXCommon::instance_ = nullptr;
// 最大SRV数を定義
const uint32_t DirectXCommon::kMaxSRVCount = 512;

// ----------------------------------------------------------------------
// Static メンバ関数の実装
// ----------------------------------------------------------------------

// ... GetCPUDescriptorHandleの実装 ...
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(
    const ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (descriptorSize * index);
    return handle;
}

// ... GetGPUDescriptorHandleの実装 ...
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(
    const ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += (descriptorSize * index);
    return handle;
}

DirectXCommon* DirectXCommon::GetInstance()
{
    if (instance_ == nullptr) {
        // インスタンスがなければ新しく生成
        instance_ = new DirectXCommon();
    }
    return instance_;
}

void DirectXCommon::Finalize()
{
    // フェンスイベントのクローズ
    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }
    // シングルトンインスタンスの解放
    if (instance_ != nullptr) {
        delete instance_;
        instance_ = nullptr;
    }
}

// コマンドリストを実行し、フェンスにシグナルを送る
void DirectXCommon::ExecuteCommandList()
{
    // GPUコマンドの実行
    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists);

}

// GPUコマンドの完了を待機する
void DirectXCommon::WaitForCommandExecution()
{
    // Fenceの値を更新し、シグナルを送る
    fenceValue_++;
    HRESULT hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
    assert(SUCCEEDED(hr));

    // コマンド完了待ち (GPU同期)
    if (fence_->GetCompletedValue() < fenceValue_) {
        // GPUの処理完了時にイベントを通知するように設定
        HRESULT hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        assert(SUCCEEDED(hr));
        // イベントが発生するまで待機
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

// コマンドアロケータとコマンドリストをリセットする
void DirectXCommon::ResetCommandList()
{
    // コマンドアロケータをリセット
    HRESULT hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));

    // コマンドリストをリセット（アロケータを再設定）
    // 第二引数（PipelineStateObject）はnullでOK
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));
}

// ----------------------------------------------------------------------
// Public メンバ関数の実装
// ----------------------------------------------------------------------

// SRV特化型Getterの実装
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index) const
{
    return GetCPUDescriptorHandle(srvDescriptorHeap_, descriptorSizeSRV_, index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index) const
{
    return GetGPUDescriptorHandle(srvDescriptorHeap_, descriptorSizeSRV_, index);
}

// Initialize関数 (すべての初期化ステップを呼び出す)
void DirectXCommon::Initialize(WinApp* winApp)
{
    assert(winApp);
    this->winApp_ = winApp;

    // FPS固定初期化
    InitializeFixFPS();

    // デバイスの生成
    CreateDevice();
    // コマンド関連の生成
    InitCommandRelated();
    // スワップチェーンの生成
    CreateSwapChain();
    // 深度バッファの生成
    CreateDepthBuffer();
    // 各種ディスクリプタヒープの生成
    CreateDescriptorHeaps();
    // レンダーターゲットビューの初期化
    InitRenderTargetView();
    // 深度ステンシルビューの初期化
    InitDepthStencilView();
    // フェンスの生成
    CreateFence();
    // ビューポート矩形の初期化
    InitViewport();
    // シザリング矩形の生成
    InitScissorRect();
    // DXCコンパイラの生成
    CreateDxcCompiler();
    // ImGuiの初期化
    InitImGui();
}

// PreDraw関数 (描画前処理)
void DirectXCommon::PreDraw()
{

    UINT bbIndex = swapChain_->GetCurrentBackBufferIndex();

    // リソースバリア (Present -> Render Target)
    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[bbIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barrier);

    // 描画先RTVとDSVを指定
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    commandList_->OMSetRenderTargets(1, &rtvHandles_[bbIndex], false, &dsvHandle);

    // 画面全体のクリア
    float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f }; // (仮の色)
    commandList_->ClearRenderTargetView(rtvHandles_[bbIndex], clearColor, 0, nullptr);
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // SRV用ディスクリプタヒープを指定
    ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_.Get() };
    // 診断用ログ
    if (!srvDescriptorHeap_) {
        Logger::Log("DirectXCommon::PreDraw: srvDescriptorHeap_ is null\n");
    } else {
        Logger::Log("DirectXCommon::PreDraw: setting SRV descriptor heap for command list\n");
    }
    commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // ビューポート、シザー矩形の設定
    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);
}

// PostDraw関数 (描画後処理)
void DirectXCommon::PostDraw()
{
    UINT bbIndex = swapChain_->GetCurrentBackBufferIndex();

    // リソースバリア (Render Target -> Present)
    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[bbIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &barrier);

    // コマンドリストを閉じる（記録終了）
    HRESULT hr = commandList_->Close();
    assert(SUCCEEDED(hr));

    // コマンドをGPUへ送信しフェンスをシグナル
    ExecuteCommandList();
    // Present（描画結果を画面に送る）
    swapChain_->Present(1, 0);
    // GPUコマンドの完了を待機
    WaitForCommandExecution();
    // allocator と commandList をリセット
    ResetCommandList();

    // FPS固定
    UpdateFixFPS();
}

ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes)
{
    HRESULT hr;
    ComPtr<ID3D12Resource> resource = nullptr;

    // UPLOADヒープ設定: CPUからGPUへのデータ転送用
    D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // リソース設定: バッファ（1次元、リニアレイアウト）
    D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);

    // リソース生成
    hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, // UPLOADヒープは常にGENERIC_READ
        nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(const DirectX::TexMetadata& metadata)
{
    HRESULT hr;
    ComPtr<ID3D12Resource> resource = nullptr;

    // DEFAULTヒープ設定: GPU常駐データ用
    D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    // リソース設定: メタデータからテクスチャ（2D）リソースを設定
    D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        metadata.format,
        metadata.width,
        (UINT)metadata.height,
        (UINT16)metadata.arraySize,
        (UINT16)metadata.mipLevels);

    // リソース生成
    hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, // 初期ステートはコピー先
        nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

ComPtr<ID3D12Resource> DirectXCommon::UploadTextureData(ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages)
{
    HRESULT hr;
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

    // アップロードバッファのサイズ計算とリソース生成
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    hr = DirectX::PrepareUpload(device_.Get(), mipImages.GetImages(), mipImages.GetImageCount(), metadata, subresources);
    assert(SUCCEEDED(hr));

    UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, (UINT)subresources.size());

    // アップロード用バッファリソース (UPLOADヒープ) を生成
    ComPtr<ID3D12Resource> uploadBuffer = CreateBufferResource(uploadBufferSize);

    // --- アップロード専用コマンドアロケータ／コマンドリストを作成して記録・実行・同期する ---
    ComPtr<ID3D12CommandAllocator> uploadAllocator;
    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAllocator));
    assert(SUCCEEDED(hr));

    ComPtr<ID3D12GraphicsCommandList> uploadCmdList;
    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAllocator.Get(), nullptr, IID_PPV_ARGS(&uploadCmdList));
    assert(SUCCEEDED(hr));

    // データ転送を記録
    UpdateSubresources(uploadCmdList.Get(), texture.Get(), uploadBuffer.Get(), 0, 0, (UINT)subresources.size(), subresources.data());

    // テクスチャをシェーダー読み取り可能状態へ遷移
    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = texture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    uploadCmdList->ResourceBarrier(1, &barrier);

    // クローズして実行
    hr = uploadCmdList->Close();
    assert(SUCCEEDED(hr));

    ID3D12CommandList* lists[] = { uploadCmdList.Get() };
    commandQueue_->ExecuteCommandLists(_countof(lists), lists);

    // GPU に対してシグナルして待機（同期）
    fenceValue_++;
    hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
    assert(SUCCEEDED(hr));
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    return uploadBuffer;
}

ComPtr<IDxcBlob> DirectXCommon::CompileShader(const std::wstring& filePath, const wchar_t* profile)
{
    HRESULT hr;
    ComPtr<IDxcBlobEncoding> shaderSource = nullptr;

    // 1. シェーダーファイルを読み込み
    // DXCユーティリティを使用して、指定されたファイルパスのシェーダーコードをBlobとして読み込む
    hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);
    if (FAILED(hr)) {
        std::string msg = std::string("Error: Failed to load shader file: ") + StringUtility::ConvertString(filePath) + "\n";
        Logger::Log(msg);
        assert(false);
        return nullptr;
    }

    // 2. DxcBuffer の設定
    // IDxcCompiler3::Compile の pSource 引数に合わせて DxcBuffer 構造体を初期化
    DxcBuffer buffer;
    buffer.Ptr = shaderSource->GetBufferPointer();
    buffer.Size = shaderSource->GetBufferSize();
    buffer.Encoding = DXC_CP_UTF8;

    // 3. コンパイル引数の設定
    LPCWSTR arguments[] = {
        filePath.c_str(), // コンパイル対象のhlslファイル名
        L"-E", L"main", // エントリーポイントの指定。基本的にmain以外にはしない。
        L"-T", profile, // ShaderProfileの設定
        L"-Zi", L"-Qembed_debug", // デバッグ用の情報を埋め込む
        L"-Od", // 最適化を外しておく
        L"-Zpr", // メモリレイアウトは行優先
    };
    UINT32 argCount = _countof(arguments);

    // 4. シェーダーのコンパイル実行 (6引数シグネチャに適合)
    ComPtr<IDxcResult> result = nullptr;
    hr = dxcCompiler_->Compile(
        &buffer, // 1. pSource (DxcBuffer 構造体へのポインタ)
        arguments, // 2. pArguments (コンパイル引数配列)
        argCount, // 3. argCount
        includeHandler_.Get(), // 4. pIncludeHandler (インクルード処理用)
        IID_PPV_ARGS(&result) // 5. riid & 6. ppResult (IID_PPV_ARGSで2つ分の引数を処理)
    );
    assert(SUCCEEDED(hr));

    // 5. エラーチェック
    ComPtr<IDxcBlobUtf8> errorBlob = nullptr;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorBlob), nullptr);
    if (errorBlob && errorBlob->GetStringLength() > 0) {
        // エラーが存在する場合、ログ出力してアサート
        std::string msg = std::string("Shader Compile Error (") + StringUtility::ConvertString(filePath) + "):\n" + errorBlob->GetStringPointer() + "\n";
        Logger::Log(msg);
        assert(false);
        return nullptr;
    }

    // 6. コンパイル結果の取得
    ComPtr<IDxcBlob> shaderBlob = nullptr;
    // コンパイル済みオブジェクト (バイトコード) をBlobとして取得
    hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    return shaderBlob;
}

// ----------------------------------------------------------------------
// Private 初期化関数の実装
// ----------------------------------------------------------------------
void DirectXCommon::CreateDevice()
{
    HRESULT hr;

#ifdef _DEBUG
    ComPtr<ID3D12Debug1> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        // GPU-Based Validation は非常に遅くなるのでデバッグ時でもOFFにすることを推奨します。
        // 必要なら true に戻してください。
        debugController->SetEnableGPUBasedValidation(FALSE);
    }
#endif

    // DXGIファクトリーを生成 
    hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
    assert(SUCCEEDED(hr));

    // 1. アダプターの列挙とデバイス生成
    ComPtr<IDXGIAdapter4> useAdapter = nullptr;

    // 高性能なGPUを優先的に選択する
    for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC3 adapterDesc {};
        useAdapter->GetDesc3(&adapterDesc);

        // ソフトウェアアダプター(WARP)でなければ採用
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            std::wstring descW(adapterDesc.Description);
            std::string desc = StringUtility::ConvertString(descW);
            Logger::Log(std::string("Use Adapter: ") + desc + "\n");
            hr = D3D12CreateDevice(useAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_));
            if (SUCCEEDED(hr)) {
                break; // デバイス生成に成功したらループを抜ける
            }
        }
        useAdapter = nullptr; // デバイス生成に失敗したらnullptrに戻す
    }

    // 2. 適切なアダプターが見つからない場合のフォールバック (WARPアダプター)
    if (!device_) {
        Logger::Log("Warning: No suitable hardware adapter found. Using WARP adapter.\n");
        ComPtr<IDXGIAdapter4> warpAdapter;
        hr = dxgiFactory_->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter));
        assert(SUCCEEDED(hr));

        hr = D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_));
        assert(SUCCEEDED(hr));
    }

#ifdef _DEBUG

    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        // やばいエラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);

        // エラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

        // 警告時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

        // 抑制するメッセージのID
        D3D12_MESSAGE_ID denyIds[] = {
            // Windows11でのDXGIデバックレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
            // https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
        };

        // 抑制するレベル
        D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
        D3D12_INFO_QUEUE_FILTER filter {};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        // 指定したメッセージの表示を抑制する
        infoQueue->PushStorageFilter(&filter);
    }

#endif

    // 3. デスクリプタサイズを取得
    descriptorSizeRTV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    descriptorSizeDSV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void DirectXCommon::InitCommandRelated()
{
    HRESULT hr;

    D3D12_COMMAND_QUEUE_DESC queueDesc {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_));
    assert(SUCCEEDED(hr));

    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
    assert(SUCCEEDED(hr));

    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
    assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateSwapChain()
{
    HRESULT hr;

    // スワップチェーン設定
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc {};
    swapChainDesc.Width = WinApp::kWindowWidth;
    swapChainDesc.Height = WinApp::kWindowHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = kBackBufferCount;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;

    hr = dxgiFactory_->CreateSwapChainForHwnd(
        commandQueue_.Get(),
        winApp_->GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));

    // スワップチェーンのフォーマットを記録
    swapChainFormat_ = swapChainDesc.Format;

    // バックバッファ取得
    for (int i = 0; i < kBackBufferCount; ++i) {
        hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
        if (FAILED(hr)) {
            wchar_t buf[128];
            swprintf_s(buf, L"GetBuffer[%d] failed. HRESULT=0x%08X\n", i, hr);
            OutputDebugString(buf);
            assert(false);
            return;
        }
    }

    OutputDebugString(L"SwapChain created successfully.\n");
}

void DirectXCommon::CreateDepthBuffer()
{
    // 深度ステンシル用Resourceを作成
    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Width = WinApp::kWindowWidth;
    resourceDesc.Height = WinApp::kWindowHeight;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_CLEAR_VALUE clearValue {};
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    HRESULT hr = device_->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&depthStencilResource_));
    assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateDescriptorHeaps()
{
    // RTV
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = kBackBufferCount;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvDescriptorHeap_));
        assert(SUCCEEDED(hr));
    }

    // SRV (CBV_SRV_UAV)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = kMaxSRVCount; // 任意の十分な数
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvDescriptorHeap_));
        assert(SUCCEEDED(hr));
    }

    // DSV
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dsvDescriptorHeap_));
        assert(SUCCEEDED(hr));
    }
}

void DirectXCommon::InitRenderTargetView()
{
    // RTV のフォーマット
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    for (int i = 0; i < kBackBufferCount; ++i) {
        rtvHandles_[i] = GetCPUDescriptorHandle(rtvDescriptorHeap_, descriptorSizeRTV_, i);
        device_->CreateRenderTargetView(swapChainResources_[i].Get(), &rtvDesc, rtvHandles_[i]);
    }
}

void DirectXCommon::InitDepthStencilView()
{
    // DSV (深度ステンシルビュー) の設定
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 深度フォーマット
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    // DSVヒープの先頭ハンドルを取得し、DSVを生成
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvHandle);
}

void DirectXCommon::CreateFence()
{
    // Fenceオブジェクトを生成し、GPU/CPUの同期に使用
    HRESULT hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));
    // GPU待機用のイベントオブジェクトを作成
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent_ != nullptr);
}

void DirectXCommon::InitViewport()
{
    // ビューポートの設定 (ウィンドウサイズ全体)
    viewport_.Width = static_cast<float>(WinApp::kWindowWidth);
    viewport_.Height = static_cast<float>(WinApp::kWindowHeight);
    viewport_.TopLeftX = 0;
    viewport_.TopLeftY = 0;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
}

void DirectXCommon::InitScissorRect()
{
    // シザー矩形の設定 (ウィンドウサイズ全体)
    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = WinApp::kWindowWidth;
    scissorRect_.bottom = WinApp::kWindowHeight;
}

void DirectXCommon::CreateDxcCompiler()
{
    // DXCユーティリティとコンパイラインスタンスを生成
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));

    // デフォルトのインクルードハンドラを作成 (シェーダー内の #include の処理用)
    hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
    assert(SUCCEEDED(hr));
}

void DirectXCommon::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(winApp_->GetHwnd());
}

void DirectXCommon::InitializeFixFPS()
{
    // 現在時間を記録する
    reference_ = std::chrono::steady_clock::now();
}

void DirectXCommon::UpdateFixFPS()
{
    // 目標フレーム時間 (1/60秒)
    const std::chrono::microseconds kFrameTime(1000000 / 60);

    // 現在時刻を取得し、前回からの経過時間を求める
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

    // まだフレーム時間に満たない場合は残り時間だけsleepする
    if (elapsed < kFrameTime) {
        auto remaining = kFrameTime - elapsed;
        std::this_thread::sleep_for(remaining);
    }

    // 次フレームの基準時間を更新
    reference_ = std::chrono::steady_clock::now();
}
