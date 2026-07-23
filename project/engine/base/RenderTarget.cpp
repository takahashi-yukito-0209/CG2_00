#include "RenderTarget.h"

#include "DirectXCommon.h"
namespace MyEngine {

/// <summary>
/// レンダーターゲットを破棄し、保持しているハンドルを解放する。
/// </summary>
RenderTarget::~RenderTarget()
{
    Finalize();
}

/// <summary>
/// 他のレンダーターゲットから所有権を移動して生成する。
/// </summary>
RenderTarget::RenderTarget(RenderTarget&& other) noexcept
{
    MoveFrom(other);
}

/// <summary>
/// 他のレンダーターゲットから所有権を移動して代入する。
/// </summary>
RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept
{
    if (this != &other) {
        Finalize();
        MoveFrom(other);
    }
    return *this;
}

/// <summary>
/// 互換用の引数を受け取り、レンダーターゲットを初期化する。
/// </summary>
bool RenderTarget::Initialize(DirectXCommon* directXCommon, SrvManager*, const RenderTargetDesc& desc)
{
    return Initialize(directXCommon, desc);
}

/// <summary>
/// 指定された設定でレンダーターゲットを初期化する。
/// </summary>
bool RenderTarget::Initialize(DirectXCommon* directXCommon, const RenderTargetDesc& desc)
{
    Finalize();

    if (!CanCreateRenderTarget(directXCommon, desc)) {
        return false;
    }
    if (!CreateRenderTargetResource(directXCommon, desc)) {
        return false;
    }
    if (!CreateColorSrvIfNeeded()) {
        Finalize();
        return false;
    }
    if (!CreateDepthSrvIfNeeded()) {
        Finalize();
        return false;
    }

    return true;
}

/// <summary>
/// 指定された設定でレンダーターゲットを作成できるか確認する。
/// </summary>
bool RenderTarget::CanCreateRenderTarget(DirectXCommon* directXCommon, const RenderTargetDesc& desc)
{
    if (!directXCommon || desc.width == 0 || desc.height == 0 || desc.format == DXGI_FORMAT_UNKNOWN) {
        return false;
    }
    if (desc.createDepthSrv && !desc.useDepth) {
        return false;
    }

    return true;
}

/// <summary>
/// DirectXCommonにレンダーターゲット実体の作成を依頼する。
/// </summary>
bool RenderTarget::CreateRenderTargetResource(DirectXCommon* directXCommon, const RenderTargetDesc& desc)
{
    directXCommon_ = directXCommon;
    desc_ = desc;
    handle_ = directXCommon_->CreateRenderTarget(
        desc_.width,
        desc_.height,
        desc_.format,
        desc_.useDepth,
        desc_.clearColor,
        desc_.resizeWithWindow);

    if (handle_ < 0) {
        ResetMembers();
        return false;
    }

    return true;
}

/// <summary>
/// 必要な場合だけカラーSRVを作成する。
/// </summary>
bool RenderTarget::CreateColorSrvIfNeeded()
{
    if (!desc_.createColorSrv) {
        return true;
    }

    colorSrvIndex_ = directXCommon_->CreateRenderTargetSRV(handle_);
    return colorSrvIndex_ != UINT32_MAX;
}

/// <summary>
/// 必要な場合だけ深度SRVを作成する。
/// </summary>
bool RenderTarget::CreateDepthSrvIfNeeded()
{
    if (!desc_.createDepthSrv) {
        return true;
    }

    depthSrvIndex_ = directXCommon_->CreateRenderTargetDepthSRV(handle_);
    return depthSrvIndex_ != UINT32_MAX;
}

/// <summary>
/// レンダーターゲット実体と関連SRVを解放する。
/// </summary>
void RenderTarget::Finalize()
{
    if (directXCommon_ && handle_ >= 0) {
        directXCommon_->DestroyRenderTarget(handle_);
    }
    ResetMembers();
}

/// <summary>
/// このレンダーターゲットへの描画を開始する。
/// </summary>
void RenderTarget::Begin(bool clear) const
{
    if (directXCommon_ && handle_ >= 0) {
        directXCommon_->BeginRenderTo(handle_, clear);
    }
}

/// <summary>
/// このレンダーターゲットへの描画を終了する。
/// </summary>
void RenderTarget::End() const
{
    if (directXCommon_ && handle_ >= 0) {
        directXCommon_->EndRenderTo(handle_);
    }
}

/// <summary>
/// レンダーターゲットを指定サイズへ変更する。
/// </summary>
void RenderTarget::Resize(uint32_t width, uint32_t height)
{
    if (directXCommon_ && handle_ >= 0 && width > 0 && height > 0) {
        directXCommon_->ResizeRenderTarget(handle_, width, height);
        desc_.width = width;
        desc_.height = height;
    }
}

/// <summary>
/// レンダーターゲットとして利用可能か確認する。
/// </summary>
bool RenderTarget::IsValid() const
{
    return directXCommon_ && handle_ >= 0;
}

/// <summary>
/// 他のレンダーターゲットから内部状態を移動する。
/// </summary>
void RenderTarget::MoveFrom(RenderTarget& other) noexcept
{
    directXCommon_ = other.directXCommon_;
    desc_ = other.desc_;
    handle_ = other.handle_;
    colorSrvIndex_ = other.colorSrvIndex_;
    depthSrvIndex_ = other.depthSrvIndex_;

    other.directXCommon_ = nullptr;
    other.handle_ = -1;
    other.colorSrvIndex_ = UINT32_MAX;
    other.depthSrvIndex_ = UINT32_MAX;
}

/// <summary>
/// 内部状態を未初期化状態へ戻す。
/// </summary>
void RenderTarget::ResetMembers()
{
    directXCommon_ = nullptr;
    desc_ = {};
    handle_ = -1;
    colorSrvIndex_ = UINT32_MAX;
    depthSrvIndex_ = UINT32_MAX;
}
} // namespace MyEngine
