#include "PostProcess.h"

#include "DirectXCommon.h"
#include "Logger.h"

#include <cassert>
#include <cstring>

using namespace Microsoft::WRL;
using namespace MyEngine;

/// <summary>
/// デストラクタ
/// </summary>
PostProcess::~PostProcess()
{
    Finalize();
}

/// <summary>
/// ポストエフェクトに必要なリソースを初期化する
/// </summary>
bool PostProcess::Initialize(DirectXCommon* dxCommon)
{
    if (!dxCommon) {
        return false;
    }

    dxCommon_ = dxCommon; // DirectX共通基盤を保持する

    CreateRootSignature();
    copyPipelineState_ = CreatePipelineState(
        L"resources/shaders/CopyImage.PS.hlsl");
    grayscalePipelineState_ = CreatePipelineState(
        L"resources/shaders/Grayscale.PS.hlsl");
    vignettePipelineState_ = CreatePipelineState(
        L"resources/shaders/Vignette.PS.hlsl");
    boxFilterPipelineState_ = CreatePipelineState(
        L"resources/shaders/BoxFilter.PS.hlsl");
    gaussianFilterPipelineState_ = CreatePipelineState(
        L"resources/shaders/GaussianFilter.PS.hlsl");

    return IsReady();
}

/// <summary>
/// ポストエフェクトが保持するリソースを解放する
/// </summary>
void PostProcess::Finalize()
{
    gaussianFilterPipelineState_.Reset();
    boxFilterPipelineState_.Reset();
    vignettePipelineState_.Reset();
    grayscalePipelineState_.Reset();
    copyPipelineState_.Reset();
    rootSignature_.Reset();
    dxCommon_ = nullptr;
    lastSrvIndex_ = UINT32_MAX;
}

/// <summary>
/// 描画に必要なリソースが揃っているか確認する
/// </summary>
bool PostProcess::IsReady() const
{
    return dxCommon_ && rootSignature_ && copyPipelineState_
        && grayscalePipelineState_ && vignettePipelineState_
        && boxFilterPipelineState_ && gaussianFilterPipelineState_;
}

/// <summary>
/// 全画面描画用のルートシグネチャを生成する
/// </summary>
void PostProcess::CreateRootSignature()
{
    D3D12_DESCRIPTOR_RANGE descriptorRange = {}; // 入力テクスチャ用SRV範囲
    descriptorRange.BaseShaderRegister = 0;
    descriptorRange.NumDescriptors = 1;
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[2] = {}; // PixelShaderへ渡すルートパラメータ
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[1].Constants.ShaderRegister = 0;
    rootParameters[1].Constants.RegisterSpace = 0;
    rootParameters[1].Constants.Num32BitValues = 4;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC staticSampler = {}; // 入力テクスチャ用サンプラー
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.ShaderRegister = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {}; // ルートシグネチャ設定
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.pStaticSamplers = &staticSampler;
    rootSignatureDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signatureBlob; // シリアライズ済みルートシグネチャ
    ComPtr<ID3DBlob> errorBlob; // シリアライズ時のエラー情報
    HRESULT result = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);

    if (FAILED(result)) {
        if (errorBlob) {
            Logger::Log(
                reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
        return;
    }

    result = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(result));
}

/// <summary>
/// 指定したピクセルシェーダーからパイプラインステートを生成する
/// </summary>
ComPtr<ID3D12PipelineState> PostProcess::CreatePipelineState(
    const wchar_t* pixelShaderPath)
{
    auto vertexShader = dxCommon_->CompileShader(
        L"resources/shaders/Fullscreen.VS.hlsl",
        L"vs_6_0"); // 全画面描画用頂点シェーダー
    auto pixelShader = dxCommon_->CompileShader(
        pixelShaderPath,
        L"ps_6_0"); // エフェクト固有のピクセルシェーダー

    if (!vertexShader || !pixelShader) {
        return nullptr;
    }

    D3D12_INPUT_LAYOUT_DESC inputLayout = {}; // 頂点バッファを使用しない入力設定

    D3D12_RASTERIZER_DESC rasterizerDesc = {}; // 全画面三角形のラスタライザー設定
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthClipEnable = TRUE;

    D3D12_BLEND_DESC blendDesc = {}; // 上書き描画用ブレンド設定
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {}; // 深度を使用しない設定
    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {}; // PSO全体の設定
    pipelineDesc.pRootSignature = rootSignature_.Get();
    pipelineDesc.VS = {
        vertexShader->GetBufferPointer(),
        vertexShader->GetBufferSize()
    };
    pipelineDesc.PS = {
        pixelShader->GetBufferPointer(),
        pixelShader->GetBufferSize()
    };
    pipelineDesc.InputLayout = inputLayout;
    pipelineDesc.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = dxCommon_->GetSwapChainFormat();
    pipelineDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    pipelineDesc.SampleDesc.Count = 1;
    pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    pipelineDesc.RasterizerState = rasterizerDesc;
    pipelineDesc.BlendState = blendDesc;
    pipelineDesc.DepthStencilState = depthStencilDesc;

    ComPtr<ID3D12PipelineState> pipelineState; // 生成したポストエフェクト用PSO
    HRESULT result = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(&pipelineState));
    assert(SUCCEEDED(result));

    return pipelineState;
}

/// <summary>
/// 指定したポストエフェクトに対応するパイプラインステートを取得する
/// </summary>
ID3D12PipelineState* PostProcess::GetPipelineState(
    PostEffectType effectType) const
{
    switch (effectType) {
    case PostEffectType::GaussianFilter:
        return gaussianFilterPipelineState_.Get();
    case PostEffectType::BoxFilter:
        return boxFilterPipelineState_.Get();
    case PostEffectType::Vignette:
        return vignettePipelineState_.Get();
    case PostEffectType::Grayscale:
        return grayscalePipelineState_.Get();
    case PostEffectType::Copy:
    default:
        return copyPipelineState_.Get();
    }
}

/// <summary>
/// 指定したポストエフェクトでテクスチャを描画する
/// </summary>
void PostProcess::DrawTexture(
    uint32_t srvIndex,
    PostEffectType effectType)
{
    if (!IsReady()) {
        return;
    }

    ID3D12PipelineState* pipelineState =
        GetPipelineState(effectType); // 描画に使用するPSO
    if (!pipelineState) {
        return;
    }

    ID3D12GraphicsCommandList* commandList =
        dxCommon_->GetCommandList(); // 描画命令を記録するコマンドリスト
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle =
        dxCommon_->GetSRVGPUDescriptorHandle(srvIndex); // 入力テクスチャのSRV
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        dxCommon_->GetSrvDescriptorHeap()
    }; // 描画で使用するSRVヒープ

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState);
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetGraphicsRootDescriptorTable(0, srvHandle);
    uint32_t sigmaBits = 0; // ルート定数へ渡す標準偏差のビット表現
    std::memcpy(&sigmaBits, &gaussianSigma_, sizeof(float));
    uint32_t filterSettings[4] = {
        boxFilterKernelSize_,
        0,
        sigmaBits,
        0
    }; // 通常の1パスエフェクト用設定
    commandList->SetGraphicsRoot32BitConstants(
        1,
        _countof(filterSettings),
        filterSettings,
        0);
    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
    lastSrvIndex_ = srvIndex;
}

/// <summary>
/// Box Filterで使用するカーネルサイズを設定する
/// </summary>
void PostProcess::SetBoxFilterKernelSize(uint32_t kernelSize)
{
    if (kernelSize == 3 || kernelSize == 5) {
        boxFilterKernelSize_ = kernelSize;
    }
}

/// <summary>
/// Gaussian Filterで使用するカーネルサイズを設定する
/// </summary>
void PostProcess::SetGaussianKernelSize(uint32_t kernelSize)
{
    if (kernelSize == 3 || kernelSize == 5 || kernelSize == 7) {
        gaussianKernelSize_ = kernelSize;
    }
}

/// <summary>
/// Gaussian Filterで使用する標準偏差を設定する
/// </summary>
void PostProcess::SetGaussianSigma(float sigma)
{
    if (sigma >= 0.1f && sigma <= 10.0f) {
        gaussianSigma_ = sigma;
    }
}

/// <summary>
/// Gaussian Filterの1方向分を描画する
/// </summary>
void PostProcess::DrawGaussianPass(uint32_t srvIndex, uint32_t direction)
{
    if (!IsReady() || !gaussianFilterPipelineState_) {
        return;
    }

    ID3D12GraphicsCommandList* commandList =
        dxCommon_->GetCommandList(); // 描画命令を記録するコマンドリスト
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle =
        dxCommon_->GetSRVGPUDescriptorHandle(srvIndex); // 入力テクスチャのSRV
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        dxCommon_->GetSrvDescriptorHeap()
    }; // 描画で使用するSRVヒープ

    uint32_t sigmaBits = 0; // ルート定数へ渡す標準偏差のビット表現
    std::memcpy(&sigmaBits, &gaussianSigma_, sizeof(float));
    uint32_t filterSettings[4] = {
        gaussianKernelSize_,
        direction,
        sigmaBits,
        0
    }; // Gaussian Filter用設定

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(gaussianFilterPipelineState_.Get());
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetGraphicsRootDescriptorTable(0, srvHandle);
    commandList->SetGraphicsRoot32BitConstants(
        1,
        _countof(filterSettings),
        filterSettings,
        0);
    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
    lastSrvIndex_ = srvIndex;
}

/// <summary>
/// 中間レンダーターゲットを使用して分離型Gaussian Filterを描画する
/// </summary>
void PostProcess::DrawGaussianTexture(
    uint32_t sourceSrvIndex,
    int intermediateRenderTargetHandle,
    uint32_t intermediateSrvIndex)
{
    if (!enabled_) {
        DrawTexture(sourceSrvIndex, PostEffectType::Copy);
        return;
    }

    dxCommon_->BeginRenderTo(intermediateRenderTargetHandle, true);
    DrawGaussianPass(sourceSrvIndex, 0); // 横方向へぼかす
    dxCommon_->EndRenderTo(intermediateRenderTargetHandle);

    DrawGaussianPass(intermediateSrvIndex, 1); // 縦方向へぼかす
}

/// <summary>
/// 現在選択されているポストエフェクトでテクスチャを描画する
/// </summary>
void PostProcess::DrawTexture(uint32_t srvIndex)
{
    PostEffectType drawEffectType =
        enabled_ ? effectType_ : PostEffectType::Copy; // 無効時は元画像を表示する
    DrawTexture(srvIndex, drawEffectType);
}
