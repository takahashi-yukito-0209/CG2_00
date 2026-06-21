#include "PostProcess.h"

#include "../utility/mathUtility.h"
#include "DirectXCommon.h"
#include "Logger.h"

#include <algorithm>
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
    luminanceOutlinePipelineState_ = CreatePipelineState(
        L"resources/shaders/LuminanceBasedOutline.PS.hlsl");
    depthOutlinePipelineState_ = CreatePipelineState(
        L"resources/shaders/DepthBasedOutline.PS.hlsl");
    radialBlurPipelineState_ = CreatePipelineState(
        L"resources/shaders/RadialBlur.PS.hlsl");
    dissolvePipelineState_ = CreatePipelineState(
        L"resources/shaders/Dissolve.PS.hlsl");
    randomPipelineState_ = CreatePipelineState(
        L"resources/shaders/Random.PS.hlsl");
    distortionPipelineState_ = CreatePipelineState(
        L"resources/shaders/Distortion.PS.hlsl");

    return IsReady();
}

/// <summary>
/// ポストエフェクトが保持するリソースを解放する
/// </summary>
void PostProcess::Finalize()
{
    distortionPipelineState_.Reset();
    randomPipelineState_.Reset();
    dissolvePipelineState_.Reset();
    radialBlurPipelineState_.Reset();
    depthOutlinePipelineState_.Reset();
    luminanceOutlinePipelineState_.Reset();
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
        && boxFilterPipelineState_ && gaussianFilterPipelineState_
        && luminanceOutlinePipelineState_ && depthOutlinePipelineState_
        && radialBlurPipelineState_ && dissolvePipelineState_
        && randomPipelineState_ && distortionPipelineState_;
}

/// <summary>
/// 全画面描画用のルートシグネチャを生成する
/// </summary>
void PostProcess::CreateRootSignature()
{
    D3D12_DESCRIPTOR_RANGE descriptorRanges[2] = {}; // カラーと深度のSRV範囲
    descriptorRanges[0].BaseShaderRegister = 0;
    descriptorRanges[0].NumDescriptors = 1;
    descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    descriptorRanges[1].BaseShaderRegister = 1;
    descriptorRanges[1].NumDescriptors = 1;
    descriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3] = {}; // PixelShaderへ渡すルートパラメータ
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRanges[0];
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRanges[1];
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[2].Constants.ShaderRegister = 0;
    rootParameters[2].Constants.RegisterSpace = 0;
    rootParameters[2].Constants.Num32BitValues = 20;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {}; // カラー用と深度用サンプラー
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].ShaderRegister = 1;
    staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {}; // ルートシグネチャ設定
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);
    rootSignatureDesc.pStaticSamplers = staticSamplers;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

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
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
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
    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
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
    case PostEffectType::Distortion:
        return distortionPipelineState_.Get();
    case PostEffectType::Random:
        return randomPipelineState_.Get();
    case PostEffectType::Dissolve:
        return dissolvePipelineState_.Get();
    case PostEffectType::RadialBlur:
        return radialBlurPipelineState_.Get();
    case PostEffectType::DepthOutline:
        return depthOutlinePipelineState_.Get();
    case PostEffectType::LuminanceOutline:
        return luminanceOutlinePipelineState_.Get();
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

    ID3D12PipelineState* pipelineState = GetPipelineState(effectType); // 描画に使用するPSO
    if (!pipelineState) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList(); // 描画命令を記録するコマンドリスト
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = dxCommon_->GetSRVGPUDescriptorHandle(srvIndex); // 入力テクスチャのSRV
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        dxCommon_->GetSrvDescriptorHeap()
    }; // 描画で使用するSRVヒープ

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState);
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetGraphicsRootDescriptorTable(0, srvHandle);
    uint32_t sigmaBits = 0; // ルート定数へ渡す標準偏差のビット表現
    std::memcpy(&sigmaBits, &gaussianSigma_, sizeof(float));
    uint32_t filterSettings[20] = {}; // 通常の1パスエフェクト用設定
    filterSettings[0] = boxFilterKernelSize_;
    filterSettings[2] = sigmaBits;
    std::memcpy(
        &filterSettings[3],
        &outlineStrength_,
        sizeof(float));
    std::memcpy(
        &filterSettings[4],
        &radialBlurCenter_,
        sizeof(Math::Vector2));
    std::memcpy(
        &filterSettings[6],
        &radialBlurWidth_,
        sizeof(float));
    filterSettings[7] = radialBlurSampleCount_;
    std::memcpy(
        &filterSettings[8],
        &dissolveThreshold_,
        sizeof(float));
    std::memcpy(
        &filterSettings[9],
        &dissolveEdgeWidth_,
        sizeof(float));
    std::memcpy(
        &filterSettings[12],
        &dissolveEdgeColor_,
        sizeof(Math::Vector3));
    std::memcpy(
        &filterSettings[16],
        &randomTime_,
        sizeof(float));
    std::memcpy(
        &filterSettings[17],
        &randomStrength_,
        sizeof(float));
    std::memcpy(
        &filterSettings[18],
        &randomScale_,
        sizeof(float));
    if (effectType == PostEffectType::Distortion) {
        std::memcpy(
            &filterSettings[4],
            &distortionCenter_,
            sizeof(Math::Vector2));
        std::memcpy(
            &filterSettings[6],
            &distortionStrength_,
            sizeof(float));
        std::memcpy(
            &filterSettings[8],
            &distortionRadius_,
            sizeof(float));
        std::memcpy(
            &filterSettings[9],
            &distortionWaveCount_,
            sizeof(float));
        std::memcpy(
            &filterSettings[10],
            &distortionProgress_,
            sizeof(float));
    }
    commandList->SetGraphicsRoot32BitConstants(
        2,
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

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList(); // 描画命令を記録するコマンドリスト
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = dxCommon_->GetSRVGPUDescriptorHandle(srvIndex); // 入力テクスチャのSRV
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        dxCommon_->GetSrvDescriptorHeap()
    }; // 描画で使用するSRVヒープ

    uint32_t sigmaBits = 0; // ルート定数へ渡す標準偏差のビット表現
    std::memcpy(&sigmaBits, &gaussianSigma_, sizeof(float));
    uint32_t filterSettings[20] = {}; // Gaussian Filter用設定
    filterSettings[0] = gaussianKernelSize_;
    filterSettings[1] = direction;
    filterSettings[2] = sigmaBits;

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(gaussianFilterPipelineState_.Get());
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetGraphicsRootDescriptorTable(0, srvHandle);
    commandList->SetGraphicsRoot32BitConstants(
        2,
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
/// Outlineの強度を設定する
/// </summary>
void PostProcess::SetOutlineStrength(float strength)
{
    if (strength >= 0.0f && strength <= 32.0f) {
        outlineStrength_ = strength;
    }
}

/// <summary>
/// Depth Outlineで輪郭と判定する深度差の閾値を設定する
/// </summary>
void PostProcess::SetDepthOutlineThreshold(float threshold)
{
    if (threshold >= 0.0f && threshold <= 1.0f) {
        depthOutlineThreshold_ = threshold;
    }
}

/// <summary>
/// Depth Outlineの輪郭の立ち上がり幅を設定する
/// </summary>
void PostProcess::SetDepthOutlineSoftness(float softness)
{
    if (softness >= 0.0001f && softness <= 1.0f) {
        depthOutlineSoftness_ = softness;
    }
}

/// <summary>
/// Radial Blurの中心座標を設定する
/// </summary>
void PostProcess::SetRadialBlurCenter(const Math::Vector2& center)
{
    radialBlurCenter_.x = (std::max)(0.0f, (std::min)(1.0f, center.x));
    radialBlurCenter_.y = (std::max)(0.0f, (std::min)(1.0f, center.y));
}

/// <summary>
/// Radial Blurの幅を設定する
/// </summary>
void PostProcess::SetRadialBlurWidth(float width)
{
    if (width >= 0.0f && width <= 0.1f) {
        radialBlurWidth_ = width;
    }
}

/// <summary>
/// Radial Blurのサンプル数を設定する
/// </summary>
void PostProcess::SetRadialBlurSampleCount(uint32_t sampleCount)
{
    if (sampleCount >= 1 && sampleCount <= 32) {
        radialBlurSampleCount_ = sampleCount;
    }
}

/// <summary>
/// Dissolveの閾値を設定する
/// </summary>
/// <summary>
/// 画面歪みの中心UV座標を設定する
/// </summary>
void PostProcess::SetDistortionCenter(const Math::Vector2& center)
{
    distortionCenter_.x = (std::clamp)(center.x, 0.0f, 1.0f);
    distortionCenter_.y = (std::clamp)(center.y, 0.0f, 1.0f);
}

/// <summary>
/// 画面歪みの強度を設定する
/// </summary>
void PostProcess::SetDistortionStrength(float strength)
{
    distortionStrength_ = (std::clamp)(strength, -0.1f, 0.1f);
}

/// <summary>
/// 画面歪みの影響半径を設定する
/// </summary>
void PostProcess::SetDistortionRadius(float radius)
{
    distortionRadius_ = (std::clamp)(radius, 0.01f, 1.5f);
}

/// <summary>
/// 画面歪みの波数を設定する
/// </summary>
void PostProcess::SetDistortionWaveCount(float waveCount)
{
    distortionWaveCount_ = (std::clamp)(waveCount, 0.0f, 12.0f);
}

/// <summary>
/// 画面歪みの進行率を設定する
/// </summary>
void PostProcess::SetDistortionProgress(float progress)
{
    distortionProgress_ = (std::clamp)(progress, 0.0f, 1.0f);
}

/// <summary>
/// Dissolveの閾値を設定する
/// </summary>
void PostProcess::SetDissolveThreshold(float threshold)
{
    dissolveThreshold_ = (std::max)(0.0f, (std::min)(1.0f, threshold));
}

/// <summary>
/// Dissolve境界の幅を設定する
/// </summary>
void PostProcess::SetDissolveEdgeWidth(float edgeWidth)
{
    dissolveEdgeWidth_ = (std::max)(0.001f, (std::min)(0.25f, edgeWidth));
}

/// <summary>
/// Dissolve境界の色を設定する
/// </summary>
void PostProcess::SetDissolveEdgeColor(const Math::Vector3& color)
{
    dissolveEdgeColor_.x = (std::max)(0.0f, (std::min)(1.0f, color.x));
    dissolveEdgeColor_.y = (std::max)(0.0f, (std::min)(1.0f, color.y));
    dissolveEdgeColor_.z = (std::max)(0.0f, (std::min)(1.0f, color.z));
}

/// <summary>
/// ノイズマスクを使用してDissolveを描画する
/// </summary>
void PostProcess::DrawDissolveTexture(
    uint32_t sourceSrvIndex,
    uint32_t maskSrvIndex)
{
    if (!enabled_) {
        DrawTexture(sourceSrvIndex, PostEffectType::Copy);
        return;
    }

    if (!IsReady() || !dissolvePipelineState_) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList(); // 描画命令を記録するコマンドリスト
    D3D12_GPU_DESCRIPTOR_HANDLE sourceSrvHandle = dxCommon_->GetSRVGPUDescriptorHandle(sourceSrvIndex); // 元画像のSRV
    D3D12_GPU_DESCRIPTOR_HANDLE maskSrvHandle = dxCommon_->GetSRVGPUDescriptorHandle(maskSrvIndex); // ノイズマスクのSRV
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        dxCommon_->GetSrvDescriptorHeap()
    }; // 描画で使用するSRVヒープ

    uint32_t dissolveSettings[20] = {}; // Dissolve用ルート定数
    std::memcpy(
        &dissolveSettings[8],
        &dissolveThreshold_,
        sizeof(float));
    std::memcpy(
        &dissolveSettings[9],
        &dissolveEdgeWidth_,
        sizeof(float));
    std::memcpy(
        &dissolveSettings[12],
        &dissolveEdgeColor_,
        sizeof(Math::Vector3));

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(dissolvePipelineState_.Get());
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetGraphicsRootDescriptorTable(0, sourceSrvHandle);
    commandList->SetGraphicsRootDescriptorTable(1, maskSrvHandle);
    commandList->SetGraphicsRoot32BitConstants(
        2,
        _countof(dissolveSettings),
        dissolveSettings,
        0);
    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
    lastSrvIndex_ = sourceSrvIndex;
}

/// <summary>
/// 時間経過を使用するポストエフェクトを更新する
/// </summary>
void PostProcess::Update(float deltaTime)
{
    if (deltaTime > 0.0f) {
        randomTime_ += deltaTime * randomSpeed_;
    }
}

/// <summary>
/// Randomのノイズ強度を設定する
/// </summary>
void PostProcess::SetRandomStrength(float strength)
{
    randomStrength_ = (std::max)(0.0f, (std::min)(1.0f, strength));
}

/// <summary>
/// Randomのノイズの細かさを設定する
/// </summary>
void PostProcess::SetRandomScale(float scale)
{
    randomScale_ = (std::max)(1.0f, (std::min)(2000.0f, scale));
}

/// <summary>
/// Randomの時間変化速度を設定する
/// </summary>
void PostProcess::SetRandomSpeed(float speed)
{
    randomSpeed_ = (std::max)(0.0f, (std::min)(20.0f, speed));
}

/// <summary>
/// 深度テクスチャを使用してOutlineを描画する
/// </summary>
void PostProcess::DrawDepthOutline(
    uint32_t colorSrvIndex,
    uint32_t depthSrvIndex,
    const Math::Matrix4x4& projectionMatrix)
{
    if (!enabled_) {
        DrawTexture(colorSrvIndex, PostEffectType::Copy);
        return;
    }

    if (!IsReady() || !depthOutlinePipelineState_) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList(); // 描画命令を記録するコマンドリスト
    D3D12_GPU_DESCRIPTOR_HANDLE colorSrvHandle = dxCommon_->GetSRVGPUDescriptorHandle(colorSrvIndex); // カラーSRV
    D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle = dxCommon_->GetSRVGPUDescriptorHandle(depthSrvIndex); // 深度SRV
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        dxCommon_->GetSrvDescriptorHeap()
    }; // 描画で使用するSRVヒープ

    Math::Matrix4x4 projectionInverse = MathUtil::Inverse(projectionMatrix); // View空間復元用の逆射影行列
    uint32_t outlineSettings[20] = {}; // Outline用ルート定数
    std::memcpy(
        &outlineSettings[3],
        &outlineStrength_,
        sizeof(float));
    std::memcpy(
        &outlineSettings[0],
        &depthOutlineThreshold_,
        sizeof(float));
    std::memcpy(
        &outlineSettings[1],
        &depthOutlineSoftness_,
        sizeof(float));
    std::memcpy(
        &outlineSettings[4],
        &projectionInverse,
        sizeof(Math::Matrix4x4));

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(depthOutlinePipelineState_.Get());
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetGraphicsRootDescriptorTable(0, colorSrvHandle);
    commandList->SetGraphicsRootDescriptorTable(1, depthSrvHandle);
    commandList->SetGraphicsRoot32BitConstants(
        2,
        _countof(outlineSettings),
        outlineSettings,
        0);
    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
    lastSrvIndex_ = colorSrvIndex;
}

/// <summary>
/// 現在選択されているポストエフェクトでテクスチャを描画する
/// </summary>
void PostProcess::DrawTexture(uint32_t srvIndex)
{
    PostEffectType drawEffectType = enabled_ ? effectType_ : PostEffectType::Copy; // 無効時は元画像を表示する
    DrawTexture(srvIndex, drawEffectType);
}
