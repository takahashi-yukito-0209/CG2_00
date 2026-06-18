#pragma once

#include <array>
#include <chrono>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <string>
#include <wrl.h>

#include "externals/DirectXTex/DirectXTex.h"
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

// 前方宣言: MyEngine 名前空間内の WinApp を宣言
namespace MyEngine {
class WinApp;
}

namespace MyEngine {

class SrvManager;

/// <summary>
/// DirectX関連の共通処理をまとめたクラス
/// </summary>
class DirectXCommon {
public:
    DirectXCommon() = default;

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
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

    /// <summary>
    /// テクスチャリソースの生成
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

    /// <summary>
    /// テクスチャデータのアップロード
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(
        Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
        const DirectX::ScratchImage& mipImages);

    /// <summary>
    /// シェーダーのコンパイル
    /// </summary>
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
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
    /// コマンドキューの取得
    /// </summary>
    ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }

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

    /// <summary>
    /// DSVヒープの先頭CPUディスクリプタハンドルを取得（外部でDSVを使ってOMSetRenderTargetsする場合に使用）
    /// </summary>
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const;

    // --- Static メンバ関数（ヘルパー/汎用機能） ---

    // 最大SRV数（最大テクスチャ枚数）
    static const uint32_t kMaxSRVCount;

    // シングルトンインスタンスを取得するための静的メソッドの宣言
    static DirectXCommon* GetInstance();

    // --- RenderTarget (offscreen) 管理 ---

    /// <summary>
    /// 指定したサイズとフォーマットでオフスクリーンのレンダーターゲットを作成し、管理リストに追加する
    /// </summary>
    int CreateRenderTarget(uint32_t width, uint32_t height, DXGI_FORMAT format, bool useDepth = true,
        const std::array<float, 4>& clearColor = std::array<float, 4> { 0.0f, 0.0f, 0.0f, 1.0f },
        bool resizeWithWindow = false);

    /// <summary>
    /// 指定したハンドルのレンダーターゲットを破棄し、管理リストから削除する
    /// </summary>
    void DestroyRenderTarget(int handle);

    /// <summary>
    /// 管理中のオフスクリーンレンダーターゲットをすべて破棄する
    /// </summary>
    void DestroyAllRenderTargets();

    /// <summary>
    /// 指定したハンドルのレンダーターゲットを新しいサイズにリサイズする
    /// </summary>
    void ResizeRenderTarget(int handle, uint32_t width, uint32_t height);

    /// <summary>
    /// 指定したハンドルのレンダーターゲットのカラーテクスチャに対してSRVを作成し、グローバルSRVヒープの指定されたインデックスに配置する
    /// </summary>
    void CreateRenderTargetSRV(int handle, uint32_t srvIndex);

    /// <summary>
    /// オフスクリーン深度バッファのSRVを生成する
    /// </summary>
    void CreateRenderTargetDepthSRV(int handle, uint32_t srvIndex);

    /// <summary>
    /// 指定したハンドルのレンダーターゲットのRTVのCPUディスクリプタハンドルを取得する
    /// </summary>
    D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetRTV(int handle) const;

    /// <summary>
    /// 指定したハンドルのレンダーターゲットのDSVのCPUディスクリプタハンドルを取得する
    /// </summary>
    D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetDSV(int handle) const;

    /// <summary>
    /// 指定したハンドルのレンダーターゲットを描画対象として設定する
    /// </summary>
    void BeginRenderTo(int handle, bool clear = true);

    /// <summary>
    /// 指定したハンドルのレンダーターゲットへの描画を終了し、必要に応じてリソースバリアで状態を遷移させる
    /// </summary>
    void EndRenderTo(int handle);

    // コピー/ムーブは禁止
    DirectXCommon(const DirectXCommon&) = delete;
    DirectXCommon& operator=(const DirectXCommon&) = delete;
    DirectXCommon(DirectXCommon&&) = delete;
    DirectXCommon& operator=(DirectXCommon&&) = delete;

    // 終了
    void Finalize();

    // コマンドリストを実行し、フェンスにシグナルを送る
    void ExecuteCommandList();

    // GPUコマンドの完了を待機する
    void WaitForCommandExecution();

    // コマンドアロケータとコマンドリストをリセットする
    void ResetCommandList();

    // SrvManager の登録
    void SetSrvManager(class SrvManager* mgr);

    // ウィンドウリサイズ通知 (WinApp から呼ばれる)
    void OnWindowResize(uint32_t width, uint32_t height);

    // リサイズ時のコールバック登録（外部でカメラやシーンへ通知するため）
    void SetOnResizeCallback(const std::function<void(uint32_t, uint32_t)>& cb);

    // 深度バッファを指定サイズで再作成する（リサイズ時に使用）
    void ResizeDepthStencil(uint32_t width, uint32_t height);

private: // Private メンバ変数
    // WinApp のポインタ（外部で管理される）
    WinApp* winApp_ = nullptr;

    // デバイス関連
    // ID3D12Device: GPU デバイスオブジェクト
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    // DXGI ファクトリ: アダプタ列挙等に使用
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
    // 選択されたアダプタのベンダーID (0x8086 = Intel, 0x10DE = NVIDIA, 0x1002 = AMD)
    uint32_t adapterVendorId_ = 0;

    // コマンド関連
    // コマンドアロケータ/コマンドリスト/コマンドキュー
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;

    // スワップチェーン
    static const uint32_t kBackBufferCount = 2; // バックバッファ数
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
    // バックバッファリソース配列
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kBackBufferCount> swapChainResources_;
    // 各バックバッファ用の RTV ハンドル配列
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kBackBufferCount> rtvHandles_;

    // スワップチェーンで使用しているピクセルフォーマット
    DXGI_FORMAT swapChainFormat_ = DXGI_FORMAT_UNKNOWN;

    // ディスクリプタヒープと各種ディスクリプタサイズ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
    UINT descriptorSizeSRV_ = 0;
    UINT descriptorSizeRTV_ = 0;
    UINT descriptorSizeDSV_ = 0;

    // 深度ステンシルバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;

    // SrvManager の参照（存在すればレンダーターゲットの SRV 解放に使用）
    class SrvManager* srvManager_ = nullptr;

    // フェンスと同期イベント
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_ = nullptr;
    UINT64 fenceValue_ = 0;

    // リサイズ処理中フラグ（再入防止）
    std::atomic<bool> resizingInProgress_ { false };

    // リサイズ通知用コールバック
    std::function<void(uint32_t, uint32_t)> onResizeCallback_;

    // ビューポートとシザー矩形
    D3D12_VIEWPORT viewport_ {};
    D3D12_RECT scissorRect_ {};

    // DXC（DirectX Shader Compiler）関連
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;

private: // メンバ関数
    // 初期化関数一覧
    void CreateDevice(); // デバイスの生成と初期化
    void InitCommandRelated(); // コマンドアロケータ、コマンドリスト、コマンドキューの生成と初期化
    void CreateSwapChain(); // スワップチェーンの生成と初期化
    void CreateDepthBuffer(); // 深度ステンシルバッファの生成と初期化
    void CreateDescriptorHeaps(); // ディスクリプタヒープの生成と初期化
    void InitRenderTargetView(); // RTVの生成と初期化
    void InitDepthStencilView(); // DSVの生成と初期化
    void CreateFence(); // フェンスと同期イベントの生成と初期化
    void InitViewport(); // ビューポートの初期化
    void InitScissorRect(); // シザー矩形の初期化
    void CreateDxcCompiler(); // DXCコンパイラの生成と初期化
    void InitImGui(); // ImGuiの初期化

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
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize,
        uint32_t index);

    /// <summary>
    /// 指定番号のGPUディスクリプタハンドルを取得 (内部処理用)
    /// </summary>
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize,
        uint32_t index);
};

} // namespace MyEngine
