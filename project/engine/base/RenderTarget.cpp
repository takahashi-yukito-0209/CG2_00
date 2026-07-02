#include "RenderTarget.h"

#include "DirectXCommon.h"
namespace MyEngine {

RenderTarget::~RenderTarget()
{
    Finalize();
}

RenderTarget::RenderTarget(RenderTarget&& other) noexcept
{
    MoveFrom(other);
}

RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept
{
    if (this != &other) {
        Finalize();
        MoveFrom(other);
    }
    return *this;
}

bool RenderTarget::Initialize(DirectXCommon* directXCommon, SrvManager*, const RenderTargetDesc& desc)
{
    return Initialize(directXCommon, desc);
}

bool RenderTarget::Initialize(DirectXCommon* directXCommon, const RenderTargetDesc& desc)
{
    Finalize();

    if (!directXCommon || desc.width == 0 || desc.height == 0 || desc.format == DXGI_FORMAT_UNKNOWN) {
        return false;
    }
    if (desc.createDepthSrv && !desc.useDepth) {
        return false;
    }

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

    if (desc_.createColorSrv) {
        colorSrvIndex_ = directXCommon_->CreateRenderTargetSRV(handle_);
        if (colorSrvIndex_ == UINT32_MAX) {
            Finalize();
            return false;
        }
    }

    if (desc_.createDepthSrv) {
        depthSrvIndex_ = directXCommon_->CreateRenderTargetDepthSRV(handle_);
        if (depthSrvIndex_ == UINT32_MAX) {
            Finalize();
            return false;
        }
    }

    return true;
}

void RenderTarget::Finalize()
{
    if (directXCommon_ && handle_ >= 0) {
        directXCommon_->DestroyRenderTarget(handle_);
    }
    ResetMembers();
}

void RenderTarget::Begin(bool clear) const
{
    if (directXCommon_ && handle_ >= 0) {
        directXCommon_->BeginRenderTo(handle_, clear);
    }
}

void RenderTarget::End() const
{
    if (directXCommon_ && handle_ >= 0) {
        directXCommon_->EndRenderTo(handle_);
    }
}

void RenderTarget::Resize(uint32_t width, uint32_t height)
{
    if (directXCommon_ && handle_ >= 0 && width > 0 && height > 0) {
        directXCommon_->ResizeRenderTarget(handle_, width, height);
        desc_.width = width;
        desc_.height = height;
    }
}

bool RenderTarget::IsValid() const
{
    return directXCommon_ && handle_ >= 0;
}

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

void RenderTarget::ResetMembers()
{
    directXCommon_ = nullptr;
    desc_ = {};
    handle_ = -1;
    colorSrvIndex_ = UINT32_MAX;
    depthSrvIndex_ = UINT32_MAX;
}
} // namespace MyEngine
