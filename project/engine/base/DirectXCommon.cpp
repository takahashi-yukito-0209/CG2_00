#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#include "WinApp.h"

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

// 最大SRV数を定義
const uint32_t DirectXCommon::kMaxSRVCount = 512;

// ----------------------------------------------------------------------
// Static メンバ関数の実装
// ----------------------------------------------------------------------

// ディスクリプタハンドルの取得用の静的関数の実装
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(
    const ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (descriptorSize * index);
    return handle;
}

// ディスクリプタハンドルの取得用の静的関数の実装
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(
    const ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += (descriptorSize * index);
    return handle;
}

/// <summary>
/// シングルトンインスタンスの取得
/// - 戻り値: DirectXCommon のシングルトンインスタンスへのポインタ
/// </summary>
DirectXCommon* DirectXCommon::GetInstance()
{
    // ローカル静的変数としてシングルトンインスタンスを定義
    static DirectXCommon instance;
    return &instance;
}

/// <summary>
/// 終了処理: フェンスイベントのクローズとシングルトン解放
/// </summary>
void DirectXCommon::Finalize()
{

    // GPU上のコマンドが完了するのを待ってからリソースを破棄する
    // これによりドライバ側のバックグラウンドスレッドが終了するまで待機し
    // DXGIのReportLiveObjectsで未解放オブジェクトが残る問題を軽減する
    if (commandQueue_ && fence_ && fenceEvent_) {
        // シグナル値をインクリメントしてGPUにシグナル
        fenceValue_++;
        HRESULT hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
        if (SUCCEEDED(hr)) {
            if (fence_->GetCompletedValue() < fenceValue_) {
                hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
                if (SUCCEEDED(hr)) {
                    WaitForSingleObject(fenceEvent_, INFINITE);
                }
            }
        }
    }

    // 明示的にComPtrをリセットして参照カウントを減らす
    // コマンド周り
    if (commandList_) commandList_.Reset();
    if (commandAllocator_) commandAllocator_.Reset();
    if (commandQueue_) commandQueue_.Reset();

    // スワップチェーン関連
    if (swapChain_) swapChain_.Reset();
    for (auto& res : swapChainResources_) {
        if (res) res.Reset();
    }

    // リソース/ヒープ類
    if (rtvDescriptorHeap_) rtvDescriptorHeap_.Reset();
    if (srvDescriptorHeap_) srvDescriptorHeap_.Reset();
    if (dsvDescriptorHeap_) dsvDescriptorHeap_.Reset();
    if (depthStencilResource_) depthStencilResource_.Reset();

    // フェンス/イベント
    if (fence_) fence_.Reset();
    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }

    // コンパイラ/ファクトリ/デバイス
    if (dxcCompiler_) dxcCompiler_.Reset();
    if (dxcUtils_) dxcUtils_.Reset();
    if (includeHandler_) includeHandler_.Reset();
    if (device_) device_.Reset();
    if (dxgiFactory_) dxgiFactory_.Reset();

}

/// <summary>
/// コマンドリストを GPU に送信して実行する
/// </summary>
void DirectXCommon::ExecuteCommandList()
{
    // GPUコマンドの実行
    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists);
}

/// <summary>
/// GPU のコマンド完了を待機する（フェンス同期）
/// </summary>
void DirectXCommon::WaitForCommandExecution()
{

    // Fenceの値を更新し、シグナルを送る
    fenceValue_++;
    HRESULT hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (FAILED(hr)) {
        char buf[256];
        sprintf_s(buf, "DirectXCommon::WaitForCommandExecution: Signal failed. HRESULT=0x%08X\n", static_cast<unsigned int>(hr));
        Logger::Log(std::string(buf));
        if (device_) {
            HRESULT reason = device_->GetDeviceRemovedReason();
            sprintf_s(buf, "DirectXCommon::WaitForCommandExecution: GetDeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(reason));
            Logger::Log(std::string(buf));
        }
        return;
    }

    // コマンド完了待ち (GPU同期)
    if (fence_->GetCompletedValue() < fenceValue_) {
        // GPUの処理完了時にイベントを通知するように設定
        HRESULT hr2 = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        if (FAILED(hr2)) {
            char buf[256];
            sprintf_s(buf, "DirectXCommon::WaitForCommandExecution: SetEventOnCompletion failed. HRESULT=0x%08X\n", static_cast<unsigned int>(hr2));
            Logger::Log(std::string(buf));
            if (device_) {
                HRESULT reason = device_->GetDeviceRemovedReason();
                sprintf_s(buf, "DirectXCommon::WaitForCommandExecution: GetDeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(reason));
                Logger::Log(std::string(buf));
            }
            return;
        }
        // イベントが発生するまで待機
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

/// <summary>
/// コマンドアロケータとコマンドリストをリセットする
/// </summary>
void DirectXCommon::ResetCommandList()
{

    // コマンドアロケータをリセット
    HRESULT hr = commandAllocator_->Reset();
    if (FAILED(hr)) {
        char buf[256];
        sprintf_s(buf, "DirectXCommon::ResetCommandList: commandAllocator_->Reset failed. HRESULT=0x%08X\n", static_cast<unsigned int>(hr));
        Logger::Log(std::string(buf));
        if (device_) {
            HRESULT reason = device_->GetDeviceRemovedReason();
            sprintf_s(buf, "DirectXCommon::ResetCommandList: GetDeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(reason));
            Logger::Log(std::string(buf));
        }
        return;
    }

    // コマンドリストをリセット（アロケータを再設定）
    // 第二引数（PipelineStateObject）はnullでOK
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    if (FAILED(hr)) {
        char buf[256];
        sprintf_s(buf, "DirectXCommon::ResetCommandList: commandList_->Reset failed. HRESULT=0x%08X\n", static_cast<unsigned int>(hr));
        Logger::Log(std::string(buf));
        if (device_) {
            HRESULT reason = device_->GetDeviceRemovedReason();
            sprintf_s(buf, "DirectXCommon::ResetCommandList: GetDeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(reason));
            Logger::Log(std::string(buf));
        }
        return;
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

// SRV特化型Getterの実装
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index) const
{
    return GetGPUDescriptorHandle(srvDescriptorHeap_, descriptorSizeSRV_, index);
}

/// <summary>
/// 全体の初期化フローを実行する
/// </summary>
void DirectXCommon::Initialize(WinApp* winApp)
{
    assert(winApp);
    // 引数のWinAppポインタをメンバ変数に保存
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

/// <summary>
/// 描画前処理を行う（リソースバリア、クリア、RTV/DSV設定等）
/// </summary>
void DirectXCommon::PreDraw()
{

    // スワップチェーンが未作成の場合は前処理をスキップ
    if (!swapChain_) {
        Logger::Log("DirectXCommon::PreDraw: swapChain_ is null\n");
        return;
    }

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
    float clearColor[] = { 0.30f, 0.48f, 0.68f, 1.0f }; // (仮の色)
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

/// <summary>
/// 描画後処理を行う（リソース遷移、Present、GPU同期、リセット等）
/// </summary>
void DirectXCommon::PostDraw()
{

    // スワップチェーンが未作成の場合は後処理をスキップ
    if (!swapChain_) {
        Logger::Log("DirectXCommon::PostDraw: swapChain_ is null\n");
        return;
    }

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
    HRESULT hrPresent = swapChain_->Present(1, 0);
    if (FAILED(hrPresent)) {
        char buf[256];
        sprintf_s(buf, "DirectXCommon::PostDraw: swapChain_->Present failed. HRESULT=0x%08X\n", static_cast<unsigned int>(hrPresent));
        Logger::Log(std::string(buf));
        // ローカル的にデバイス削除理由もログ出力しておく
        if (device_) {
            HRESULT reason = device_->GetDeviceRemovedReason();
            sprintf_s(buf, "DirectXCommon::PostDraw: GetDeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(reason));
            Logger::Log(std::string(buf));
        }
    }
    // GPUコマンドの完了を待機
    WaitForCommandExecution();
    // allocator と commandList をリセット
    ResetCommandList();

    // FPS固定
    UpdateFixFPS();
}

/// <summary>
/// バッファリソース（頂点、定数など）を生成するための関数
/// </summary>
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

/// <summary>
/// テクスチャリソースを生成するための関数
/// </summary>
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

/// <summary>
/// テクスチャデータをGPUにアップロードするための関数
/// </summary>
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

    // アップロード専用コマンドアロケータを作成して記録・実行・同期する
    ComPtr<ID3D12CommandAllocator> uploadAllocator;
    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAllocator));
    assert(SUCCEEDED(hr));

    // アップロード専用コマンドリストを作成して記録・実行・同期する
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

/// <summary>
/// シェーダーをコンパイルするための関数
/// </summary>
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
    std::vector<std::wstring> argStrings;
    argStrings.push_back(filePath);
    argStrings.push_back(L"-E"); argStrings.push_back(L"main");
    argStrings.push_back(L"-T"); argStrings.push_back(profile);
    argStrings.push_back(L"-Zi"); argStrings.push_back(L"-Qembed_debug");
    argStrings.push_back(L"-Od"); // 最適化を外しておく
    argStrings.push_back(L"-Zpr"); // メモリレイアウトは行優先

    // スワップチェーンのフォーマットに応じてSRGB定義を追加
    bool swapchainIsSrgb = (swapChainFormat_ == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    if (swapchainIsSrgb) {
        argStrings.push_back(L"-DSWAPCHAIN_SRGB=1");
    } else {
        argStrings.push_back(L"-DSWAPCHAIN_SRGB=0");
    }

    // DxcCompiler3::Compile は LPCWSTR* 型の引数配列を要求するため、
    // std::vector<std::wstring> から LPCWSTR* への変換が必要
    std::vector<LPCWSTR> arguments;
    arguments.reserve(argStrings.size());
    for (auto& s : argStrings) arguments.push_back(s.c_str());
    UINT32 argCount = static_cast<UINT32>(arguments.size());

    // 4. シェーダーのコンパイル実行 (6引数シグネチャに適合)
    ComPtr<IDxcResult> result = nullptr;
    hr = dxcCompiler_->Compile(
        &buffer, // 1. pSource (DxcBuffer 構造体へのポインタ)
        arguments.data(), // 2. pArguments (コンパイル引数配列)
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

/// <summary>
/// デバイスの生成
/// </summary>
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
                // 選択されたアダプタのベンダーIDを記録しておく
                adapterVendorId_ = static_cast<uint32_t>(adapterDesc.VendorId);
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
        // WARP を使った場合はベンダーIDを0にしておく
        adapterVendorId_ = 0;
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

/// <summary>
/// コマンドキュー、コマンドアロケータ、コマンドリストの生成
/// </summary>
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

/// <summary>
/// スワップチェーンの生成とバックバッファの取得
/// </summary>
void DirectXCommon::CreateSwapChain()
{
    HRESULT hr;

    // スワップチェーン設定
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc {};
    swapChainDesc.Width = WinApp::kWindowWidth;
    swapChainDesc.Height = WinApp::kWindowHeight;
    // フォーマットは後でSRGBとUNORMの両方を試すため、ここでは仮の値を設定しておく
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = kBackBufferCount;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;

    // IDXGISwapChain1 を作成してから、成功したフォーマットで 
    // IDXGISwapChain4 にクエリする方式でスワップチェーンを生成
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;

    // フォーマットの候補を用意
    DXGI_FORMAT formatsToTryDefault[] = { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM };
    DXGI_FORMAT formatsToTryIntelOnly[] = { DXGI_FORMAT_R8G8B8A8_UNORM };
    DXGI_FORMAT* formatsToTry = formatsToTryDefault;
    size_t formatsCount = _countof(formatsToTryDefault);
    
    if (adapterVendorId_ == 0x8086) {
        formatsToTry = formatsToTryIntelOnly;
        formatsCount = _countof(formatsToTryIntelOnly);
        Logger::Log("DirectXCommon::CreateSwapChain: Intel adapter detected — skipping SRGB swapchain attempt.\n");
    }

    // フォーマットの候補を順に試すループ
    bool swapChainCreated = false;
    for (size_t fi = 0; fi < formatsCount; ++fi) {
        DXGI_FORMAT fmt = formatsToTry[fi];
        swapChainDesc.Format = fmt;
        hr = dxgiFactory_->CreateSwapChainForHwnd(
            commandQueue_.Get(),
            winApp_->GetHwnd(),
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1);

        if (SUCCEEDED(hr) && swapChain1) {
            // IDXGISwapChain4 にクエリして保存
            hr = swapChain1.As(&swapChain_);
            if (SUCCEEDED(hr) && swapChain_) {
                swapChainFormat_ = fmt; // 成功したフォーマットを記録
                swapChainCreated = true;

                // ログ出力
                if (fmt == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
                    Logger::Log("SwapChain created with SRGB format.\n");
                } else {
                    Logger::Log("SwapChain created with UNORM format (fallback).\n");
                }
                break;
            }
        }

        // 失敗した場合はリソースをクリーンアップして次のフォーマットを試す
        swapChain1.Reset();
        swapChain_.Reset();
        char buf[256];
        sprintf_s(buf, "CreateSwapChainForHwnd for format %u failed. hr=0x%08X\n", static_cast<unsigned int>(fmt), static_cast<unsigned int>(hr));
        OutputDebugStringA(buf);
        Logger::Log(std::string(buf));
        // デバイスが削除された場合やその他のデバイス関連のエラーが発生した場合、理由をログに出力
        if (device_) {
            HRESULT reason = device_->GetDeviceRemovedReason();
            sprintf_s(buf, "CreateSwapChainForHwnd: GetDeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(reason));
            Logger::Log(std::string(buf));
        }
    }

    // 最終的にスワップチェーンが作成できなかった場合はエラーをログに出力してアサート
    if (!swapChainCreated) {
        Logger::Log("Failed to create swap chain with both SRGB and UNORM formats.\n");
        if (device_) {
            HRESULT reason = device_->GetDeviceRemovedReason();
            char buf[256];
            sprintf_s(buf, "CreateSwapChain: GetDeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(reason));
            Logger::Log(std::string(buf));
        }
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

/// <summary>
/// 深度バッファの生成
/// </summary>
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

/// <summary>
/// ディスクリプタヒープの生成
/// </summary>
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
        // デバッグログ
        if (srvDescriptorHeap_) {
            D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
            D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart();
            char buf[256];
            sprintf_s(buf, "DEBUG CreateDescriptorHeaps: SRV heap created. num=%u descSize=%u CPU=0x%016llX GPU=0x%016llX\n",
                kMaxSRVCount, descriptorSizeSRV_, static_cast<unsigned long long>(cpuStart.ptr), static_cast<unsigned long long>(gpuStart.ptr));
            Logger::Log(buf);
        } else {
            Logger::Log("DEBUG CreateDescriptorHeaps: srvDescriptorHeap_ is null after CreateDescriptorHeap\n");
        }
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

/// <summary>
/// レンダーターゲットビューの初期化
/// </summary>
void DirectXCommon::InitRenderTargetView()
{
    // 各バックバッファの実際のフォーマットに合わせてRTVを作成する
    for (int i = 0; i < kBackBufferCount; ++i) {
        // バッファのフォーマットを取得
        auto res = swapChainResources_[i].Get();
        D3D12_RESOURCE_DESC resDesc = res->GetDesc();

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
        rtvDesc.Format = resDesc.Format; // 取得したリソースのフォーマットを使用
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        rtvHandles_[i] = GetCPUDescriptorHandle(rtvDescriptorHeap_, descriptorSizeRTV_, i);
        device_->CreateRenderTargetView(res, &rtvDesc, rtvHandles_[i]);
    }
}

/// <summary>
/// 深度ステンシルビューの初期化
/// </summary>
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

/// <summary>
/// フェンスの生成とGPU同期のためのイベントオブジェクトの作成
/// </summary>
void DirectXCommon::CreateFence()
{
    // Fenceオブジェクトを生成し、GPU/CPUの同期に使用
    HRESULT hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));
    // GPU待機用のイベントオブジェクトを作成
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent_ != nullptr);
}

/// <summary>
/// ビューポートの初期化
/// </summary>
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

/// <summary>
/// シザー矩形の初期化
/// </summary>
void DirectXCommon::InitScissorRect()
{
    // シザー矩形の設定 (ウィンドウサイズ全体)
    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = WinApp::kWindowWidth;
    scissorRect_.bottom = WinApp::kWindowHeight;
}

/// <summary>
/// DXCコンパイラの生成
/// </summary>
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

/// <summary>
/// ImGuiの初期化
/// </summary>
void DirectXCommon::InitImGui()
{
    // ここでImGuiを初期化しない（意図的）
    // ImGuiのコンテキストとプラットフォームバックエンド(Win32)はImGuiManager側で
    // 一元的に初期化される。これにより二重初期化によるアサート
    // (例: "Already initialized a platform backend!") を回避する。
    // レンダラバックエンド(DX12)は、ImGuiManagerがコンテキストとプラットフォーム
    // バックエンドをセットアップした後に SrvManager::InitImGui() から初期化される。
}

/// <summary>
/// FPS固定の初期化
/// </summary>
void DirectXCommon::InitializeFixFPS()
{
    // 現在時間を記録する
    reference_ = std::chrono::steady_clock::now();
}

/// <summary>
/// FPS固定の更新処理
/// </summary>
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
