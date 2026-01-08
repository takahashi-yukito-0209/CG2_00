#pragma once

#include <array>
#include <chrono>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <string>
#include <wrl.h>

#include "externals/DirectXTex/DirectXTex.h"

using Microsoft::WRL::ComPtr;

// 前方宣言
class WinApp;

namespace MyEngine {
/// <summary>
/// DirectX関連の共通処理をまとめたクラス
/// </summary>
class DirectXCommon {

public: // 公開メンバ関数
    /// <summary>
    /// 初期化処理の全体フロー
    /// </summary>
    void Initialize(WinApp* winApp);

    /// <summary>
    /// 描画前処理（リソースバリア、RTV/DSV設定、クリア処理など）
    /// </summary>
    void PreDraw();

    /// <summary>
    /// 描画後処理（リソースバリア、コマンド実行、Present、GPU同期など）
    /// </summary>
    void PostDraw();

    /// <summary>
    /// バッファリソース（頂点、定数など）の生成
    /// </summary>
    ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

    /// <summary>
    /// テクスチャリソースの生成
    /// </summary>
    ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

    /// <summary>
    /// テクスチャデータのアップロード
    /// </summary>
    ComPtr<ID3D12Resource> UploadTextureData(
        ComPtr<ID3D12Resource>& texture,
        const DirectX::ScratchImage& mipImages);

    /// <summary>
    /// シェーダーのコンパイル
    /// </summary>
    ComPtr<IDxcBlob> CompileShader(
        const std::wstring& filePath,
        const wchar_t* profile);

    // --- Getter ---

    /// <summary>
    /// デバイスの取得
    /// </summary>
    ID3D12Device* GetDevice() const { return device_.Get(); }

    /// <summary>
    /// コマンドリストの取得
    /// </summary>
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

    /// <summary>
    /// SRV用CPUディスクリプタハンドルの取得
    /// </summary>
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index) const;

    /// <summary>
    /// SRV用GPUディスクリプタハンドルの取得
    /// </summary>
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index) const;

    /// <summary>
    /// SRV用ディスクリプタヒープの生ポインタを取得（コマンドリストへのバインド用）
    /// </summary>
    ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return srvDescriptorHeap_.Get(); }

    /// <summary>
    /// スワップチェーンが実際に使っているフォーマットを取得
    /// </summary>
    DXGI_FORMAT GetSwapChainFormat() const { return swapChainFormat_; }

    // --- Static メンバ関数（ヘルパー/汎用機能） ---

    // 最大SRV数（最大テクスチャ枚数）
    static const uint32_t kMaxSRVCount;

    // シングルトンインスタンスを取得するための静的メソッドの宣言
    static DirectXCommon* GetInstance();

    //終了
    void Finalize();

    // コマンドリストを実行し、フェンスにシグナルを送る
    void ExecuteCommandList();
   
    // GPUコマンドの完了を待機する
    void WaitForCommandExecution();

    // コマンドアロケータとコマンドリストをリセットする
    void ResetCommandList();


private: // Private メンバ変数
    // WinAppのポインタ
    WinApp* winApp_ = nullptr;

    // シングルトンインスタンスを保持するための静的メンバ変数の宣言
    static DirectXCommon* instance_;

    // デバイス関連
    ComPtr<ID3D12Device> device_;
    ComPtr<IDXGIFactory7> dxgiFactory_;

    // コマンド関連
    ComPtr<ID3D12CommandAllocator> commandAllocator_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    ComPtr<ID3D12CommandQueue> commandQueue_;

    // スワップチェーン
    static const uint32_t kBackBufferCount = 2; // バックバッファの数は固定
    ComPtr<IDXGISwapChain4> swapChain_;
    std::array<ComPtr<ID3D12Resource>, kBackBufferCount> swapChainResources_; // バックバッファリソース
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kBackBufferCount> rtvHandles_; // RTVハンドル

    // スワップチェーンで使われたフォーマットを保持（RTVやImGui初期化で使用）
    DXGI_FORMAT swapChainFormat_ = DXGI_FORMAT_UNKNOWN;

    // ディスクリプタヒープとサイズ
    ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
    ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
    UINT descriptorSizeSRV_ = 0;
    UINT descriptorSizeRTV_ = 0;
    UINT descriptorSizeDSV_ = 0;

    // 深度バッファ
    ComPtr<ID3D12Resource> depthStencilResource_;

    // フェンス
    ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_ = nullptr;
    UINT64 fenceValue_ = 0;

    // ビューポートとシザー矩形
    D3D12_VIEWPORT viewport_ {};
    D3D12_RECT scissorRect_ {};

    // DXC関連
    ComPtr<IDxcUtils> dxcUtils_;
    ComPtr<IDxcCompiler3> dxcCompiler_;
    ComPtr<IDxcIncludeHandler> includeHandler_;

private: // メンバ関数
    // 初期化関数一覧
    void CreateDevice();
    void InitCommandRelated();
    void CreateSwapChain();
    void CreateDepthBuffer();
    void CreateDescriptorHeaps();
    void InitRenderTargetView();
    void InitDepthStencilView();
    void CreateFence();
    void InitViewport();
    void InitScissorRect();
    void CreateDxcCompiler();
    void InitImGui();

    // FPS固定初期化
    void InitializeFixFPS();

    // FPS固定更新
    void UpdateFixFPS();

    // 記録時間(FPS固定用)
    std::chrono::steady_clock::time_point reference_;

private: // Private Static メンバ関数（ディスクリプタヘルパー）
    /// <summary>
    /// 指定番号のCPUディスクリプタハンドルを取得 (内部処理用)
    /// </summary>
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
        const ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize,
        uint32_t index);

    /// <summary>
    /// 指定番号のGPUディスクリプタハンドルを取得 (内部処理用)
    /// </summary>
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
        const ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize,
        uint32_t index);
};
} // namespace MyEngine