#include "WinApp.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"

#include <cassert>
#include <d3d12sdklayers.h>
#include <externals/DirectXTex/d3dx12.h>
#include <format>
#include <comdef.h>  // _com_error 用


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

// ... LoadTextureの実装 (DirectXTex::LoadFromWICFileなどを利用) ...
DirectX::ScratchImage DirectXCommon::LoadTexture(const std::string& filePath)
{

    // std::string を std::wstring に変換
    std::wstring wfilePath = StringUtility::ConvertString(filePath);
    Logger::Log(std::format("DEBUG wfilePath = {}\n", StringUtility::ConvertString(wfilePath)));

    DirectX::ScratchImage image {};
    HRESULT hr = DirectX::LoadFromWICFile(wfilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    Logger::Log(std::format("DEBUG hr = 0x{:08X}\n", hr));

    if (FAILED(hr)) {
        Logger::Log(std::format("Error: Failed to load texture file: {}\n", filePath));
        assert(false);
        return {};
    }

    // ミップマップ生成
    DirectX::ScratchImage mipImages {};
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);

    if (SUCCEEDED(hr)) {
        return mipImages;
    } else {
        return image; // ミップマップ生成失敗時はオリジナルを返す
    }
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

    //FPS固定初期化
    InitializeFixFPS();

    // デバイスの生成
    CreateDevice();
    //コマンド関連の生成
    InitCommandRelated();
    //スワップチェーンの生成
    CreateSwapChain();
    //深度バッファの生成
    CreateDepthBuffer();
    //各種ディスクリプタヒープの生成
    CreateDescriptorHeaps();
    //レンダーターゲットビューの初期化
    InitRenderTargetView();
    //深度ステンシルビューの初期化
    InitDepthStencilView();
    //フェンスの生成
    CreateFence();
    //ビューポート矩形の初期化
    InitViewport();
    //シザリング矩形の生成
    InitScissorRect();
    //DXCコンパイラの生成
    CreateDxcCompiler();
    //ImGuiの初期化
    InitImGui();
}

// PreDraw関数 (描画前処理)
void DirectXCommon::PreDraw()
{
    // フレームの開始時に必ずリセットしてから記録を始める
    HRESULT hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));

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

    // グラフィックスコマンドをクローズ
    HRESULT hr = commandList_->Close();
    assert(SUCCEEDED(hr));

    // GPUコマンドの実行
    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists);

    // GPU画面の交換を通知 (Present)
    swapChain_->Present(1, 0);

    // Fenceの値を更新し、シグナルを送る
    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);

    // コマンド完了待ち (GPU同期)
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    //FPS固定
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

    // テクスチャデータ転送に必要なトータルサイズを計算
    UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, (UINT)subresources.size());

    // アップロード用バッファリソース (UPLOADヒープ) を生成
    ComPtr<ID3D12Resource> uploadBuffer = CreateBufferResource(uploadBufferSize);

    // アップロードバッファにテクスチャデータを転送
    UpdateSubresources(commandList_.Get(), texture.Get(), uploadBuffer.Get(), 0, 0, (UINT)subresources.size(), subresources.data());

    // テクスチャをシェーダーから読み取り可能な状態に遷移させる
    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = texture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList_->ResourceBarrier(1, &barrier);

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
        Logger::Log(std::format("Error: Failed to load shader file: {}\n", StringUtility::ConvertString(filePath)));
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
        Logger::Log(std::format("Shader Compile Error ({}):\n{}\n", StringUtility::ConvertString(filePath), errorBlob->GetStringPointer()));
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

// デバッグレイヤーを有効にする (デバッグビルド時のみ)
#ifdef _DEBUG
    ComPtr<ID3D12Debug1> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(TRUE);
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
            Logger::Log(std::format("Use Adapter: {}\n", StringUtility::ConvertString(adapterDesc.Description)));
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

    hr = commandList_->Close();
    assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateSwapChain()
{
    HRESULT hr;

    // スワップチェーン設定
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc {};
    swapChainDesc.Width = WinApp::kWindowWidth;
    swapChainDesc.Height = WinApp::kWindowHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // sRGB OFFで互換性向上
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = kBackBufferCount;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;

    // IDXGISwapChain1 で作成
    ComPtr<IDXGISwapChain1> swapChain1;
    hr = dxgiFactory_->CreateSwapChainForHwnd(
        commandQueue_.Get(),
        winApp_->GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1);

    // デバイス削除やその他エラーのチェック
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            HRESULT reason = device_->GetDeviceRemovedReason();
            wchar_t buf[256];
            swprintf_s(buf, L"SwapChain creation failed: device removed. Reason=0x%08X\n", reason);
            OutputDebugString(buf);
        } else {
            OutputDebugString(L"SwapChain creation failed with unknown HRESULT.\n");
        }
        assert(false);
        return;
    }

    // IDXGISwapChain3 に変換
    hr = swapChain1.As(&swapChain_);
    if (FAILED(hr)) {
        OutputDebugString(L"QueryInterface for IDXGISwapChain3 failed.\n");
        assert(false);
        return;
    }

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
        desc.NumDescriptors = 128; // 任意の十分な数
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
    // RTV (レンダーターゲットビュー) の設定
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    // 各バックバッファに対してRTVを生成し、ハンドルを保持
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
    // Win32+DX12 用の ImGui 初期化
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(winApp_->GetHwnd());

    // ImGui_ImplDX12_Init の引数に合わせる
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart();

    ImGui_ImplDX12_Init(
        device_.Get(),
        kBackBufferCount,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        srvDescriptorHeap_.Get(),
        cpuHandle,
        gpuHandle);
}

void DirectXCommon::InitializeFixFPS()
{
    //現在時間を記録する
    reference_ = std::chrono::steady_clock::now();
}

void DirectXCommon::UpdateFixFPS()
{
    //1/60秒ぴったりの時間
    const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));

    //1/60秒よりわずかに短い時間
    const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f / 65.0f));

    //現在時刻を取得する
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    //前回記録からの経過時間を取得する
    std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

    //1/60秒(よりわずかに短い時間)経っていない場合
    if (elapsed < kMinCheckTime) {
        //1/60秒経過するまで微小なスリープを繰り返す
        while (std::chrono::steady_clock::now() - reference_ < kMinTime) {
            //1マイクロ秒スリープ
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }

    //現在の時間を記録する
    reference_ = std::chrono::steady_clock::now();

}
