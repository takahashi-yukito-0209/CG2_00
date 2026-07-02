#pragma once

#include <array>
#include <cstdint>
#include <d3d12.h>

namespace MyEngine {
class DirectXCommon;
class SrvManager;

/// <summary>
/// Holds settings used when creating an offscreen render target.
/// </summary>
struct RenderTargetDesc {
    uint32_t width = 0; // Target width
    uint32_t height = 0; // Target height
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN; // Color buffer format
    bool useDepth = true; // Creates a depth buffer
    bool createColorSrv = true; // Creates a color SRV
    bool createDepthSrv = false; // Creates a depth SRV
    bool resizeWithWindow = false; // Resizes with the window
    std::array<float, 4> clearColor { 0.0f, 0.0f, 0.0f, 1.0f }; // Clear color
};

/// <summary>
/// Owns a DirectXCommon render target handle and its SRV indices.
/// </summary>
class RenderTarget {
public:
    /// <summary>
    /// Releases the owned render target.
    /// </summary>
    ~RenderTarget();

    RenderTarget() = default;
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;

    /// <summary>
    /// Creates a render target from the specified settings.
    /// </summary>
    bool Initialize(DirectXCommon* directXCommon, const RenderTargetDesc& desc);

    /// <summary>
    /// Creates a render target from the specified settings.
    /// </summary>
    bool Initialize(DirectXCommon* directXCommon, SrvManager* srvManager, const RenderTargetDesc& desc);

    /// <summary>
    /// Releases the owned render target.
    /// </summary>
    void Finalize();

    /// <summary>
    /// Begins drawing to this render target.
    /// </summary>
    void Begin(bool clear = true) const;

    /// <summary>
    /// Ends drawing to this render target.
    /// </summary>
    void End() const;

    /// <summary>
    /// Resizes this render target.
    /// </summary>
    void Resize(uint32_t width, uint32_t height);

    /// <summary>
    /// Returns whether this object has a valid render target.
    /// </summary>
    bool IsValid() const;

    /// <summary>
    /// Returns whether this target has a color SRV.
    /// </summary>
    bool HasColorSrv() const { return colorSrvIndex_ != UINT32_MAX; }

    /// <summary>
    /// Gets the color SRV index.
    /// </summary>
    uint32_t GetColorSrvIndex() const { return colorSrvIndex_; }

    /// <summary>
    /// Returns whether this target has a depth SRV.
    /// </summary>
    bool HasDepthSrv() const { return depthSrvIndex_ != UINT32_MAX; }

    /// <summary>
    /// Gets the depth SRV index.
    /// </summary>
    uint32_t GetDepthSrvIndex() const { return depthSrvIndex_; }

    /// <summary>
    /// Gets the DirectXCommon render target handle.
    /// </summary>
    int GetHandle() const { return handle_; }

private:
    /// <summary>
    /// Moves ownership from another instance.
    /// </summary>
    void MoveFrom(RenderTarget& other) noexcept;

    /// <summary>
    /// Resets members to the empty state.
    /// </summary>
    void ResetMembers();

private:
    DirectXCommon* directXCommon_ = nullptr; // DirectX backend used for RT operations
    RenderTargetDesc desc_ {}; // Creation settings
    int handle_ = -1; // DirectXCommon render target handle
    uint32_t colorSrvIndex_ = UINT32_MAX; // Color SRV index
    uint32_t depthSrvIndex_ = UINT32_MAX; // Depth SRV index
};
} // namespace MyEngine