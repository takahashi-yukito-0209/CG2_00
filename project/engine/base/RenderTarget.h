#pragma once

#include <array>
#include <cstdint>
#include <d3d12.h>

namespace MyEngine {
class DirectXCommon;
class SrvManager;

/// <summary>
/// オフスクリーン描画用レンダーターゲットを作成するための設定。
/// </summary>
struct RenderTargetDesc {
    uint32_t width = 0; // レンダーターゲットの幅
    uint32_t height = 0; // レンダーターゲットの高さ
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN; // カラーバッファのフォーマット
    bool useDepth = true; // 深度バッファを作成するか
    bool createColorSrv = true; // カラーSRVを作成するか
    bool createDepthSrv = false; // 深度SRVを作成するか
    bool resizeWithWindow = false; // ウィンドウリサイズに追従するか
    std::array<float, 4> clearColor { 0.0f, 0.0f, 0.0f, 1.0f }; // クリアカラー
};

/// <summary>
/// DirectXCommonが管理するレンダーターゲットを利用側から扱うためのラッパー。
/// </summary>
class RenderTarget {
public:
    /// <summary>
    /// 保持しているレンダーターゲットを解放する。
    /// </summary>
    ~RenderTarget();

    RenderTarget() = default;
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;

    /// <summary>
    /// 指定された設定からレンダーターゲットを作成する。
    /// </summary>
    bool Initialize(DirectXCommon* directXCommon, const RenderTargetDesc& desc);

    /// <summary>
    /// 指定された設定からレンダーターゲットを作成する。
    /// </summary>
    bool Initialize(DirectXCommon* directXCommon, SrvManager* srvManager, const RenderTargetDesc& desc);

    /// <summary>
    /// 保持しているレンダーターゲットを解放する。
    /// </summary>
    void Finalize();

    /// <summary>
    /// このレンダーターゲットへの描画を開始する。
    /// </summary>
    void Begin(bool clear = true) const;

    /// <summary>
    /// このレンダーターゲットへの描画を終了する。
    /// </summary>
    void End() const;

    /// <summary>
    /// このレンダーターゲットをリサイズする。
    /// </summary>
    void Resize(uint32_t width, uint32_t height);

    /// <summary>
    /// 有効なレンダーターゲットを保持しているか確認する。
    /// </summary>
    bool IsValid() const;

    /// <summary>
    /// カラーSRVを保持しているか確認する。
    /// </summary>
    bool HasColorSrv() const { return colorSrvIndex_ != UINT32_MAX; }

    /// <summary>
    /// カラーSRV番号を取得する。
    /// </summary>
    uint32_t GetColorSrvIndex() const { return colorSrvIndex_; }

    /// <summary>
    /// 深度SRVを保持しているか確認する。
    /// </summary>
    bool HasDepthSrv() const { return depthSrvIndex_ != UINT32_MAX; }

    /// <summary>
    /// 深度SRV番号を取得する。
    /// </summary>
    uint32_t GetDepthSrvIndex() const { return depthSrvIndex_; }

    /// <summary>
    /// DirectXCommon側のレンダーターゲットハンドルを取得する。
    /// </summary>
    int GetHandle() const { return handle_; }

private:
    /// <summary>
    /// 別インスタンスから所有権を移動する。
    /// </summary>
    void MoveFrom(RenderTarget& other) noexcept;

    /// <summary>
    /// メンバを未保持状態に戻す。
    /// </summary>
    void ResetMembers();

private:
    DirectXCommon* directXCommon_ = nullptr; // RT実体を管理するDirectX基盤
    RenderTargetDesc desc_ {}; // 作成時の設定
    int handle_ = -1; // DirectXCommon側のRT管理ハンドル
    uint32_t colorSrvIndex_ = UINT32_MAX; // カラーSRV番号
    uint32_t depthSrvIndex_ = UINT32_MAX; // 深度SRV番号
};
} // namespace MyEngine
