#include "D3DResourceLeakChecker.h"
#include "engine/utility/Logger.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>
#include <cstddef>

namespace {
constexpr size_t kLeakCheckLogBufferSize = 256; // リークチェックログ用バッファサイズ
} // namespace
/// <summary>
/// デストラクタ：D3Dリソースリークのチェックを行う
/// </summary>
D3DResourceLeakChecker::~D3DResourceLeakChecker()
{
    // リソースリークチェックはデバッグビルド限定で行う
#ifdef _DEBUG
    Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
    HRESULT hr = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug));
    if (FAILED(hr) || !debug) {
        char buf[kLeakCheckLogBufferSize];
        sprintf_s(buf, "D3DResourceLeakChecker: DXGIGetDebugInterface1 failed. HRESULT=0x%08X\n", static_cast<unsigned int>(hr));
        Logger::Warn(std::string(buf));
        return;
    }

    // ReportLiveObjects は詳細出力（ALL）だとドライバに負荷をかける場合があるため
    // 簡易サマリーモードで出力する
    HRESULT r = debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
    if (FAILED(r)) {
        char buf[kLeakCheckLogBufferSize];
        sprintf_s(buf, "D3DResourceLeakChecker: ReportLiveObjects(DXGI_DEBUG_ALL) failed. HRESULT=0x%08X\n", static_cast<unsigned int>(r));
        Logger::Warn(std::string(buf));
    }

    r = debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_SUMMARY);
    if (FAILED(r)) {
        char buf[kLeakCheckLogBufferSize];
        sprintf_s(buf, "D3DResourceLeakChecker: ReportLiveObjects(DXGI_DEBUG_APP) failed. HRESULT=0x%08X\n", static_cast<unsigned int>(r));
        Logger::Warn(std::string(buf));
    }

    r = debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_SUMMARY);
    if (FAILED(r)) {
        char buf[kLeakCheckLogBufferSize];
        sprintf_s(buf, "D3DResourceLeakChecker: ReportLiveObjects(DXGI_DEBUG_D3D12) failed. HRESULT=0x%08X\n", static_cast<unsigned int>(r));
        Logger::Warn(std::string(buf));
    }
#else
    // Releaseビルドでは実行しない
#endif
}