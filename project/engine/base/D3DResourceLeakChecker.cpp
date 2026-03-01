#include "D3DResourceLeakChecker.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>

/// <summary>
/// デストラクタ：D3Dリソースリークのチェックを行う
/// </summary>
D3DResourceLeakChecker::~D3DResourceLeakChecker()
{
    // リソースリークチェック
    Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
        debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL); // 全てのAPIの全てのオブジェクトをレポート
        debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL); // アプリケーション関連の全てのオブジェクトをレポート
        debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL); // D3D12関連の全てのオブジェクトをレポート
    }
}