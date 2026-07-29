#include "PostProcessPipeline.h"

#include "DirectXCommon.h"
#include "Logger.h"

#include <cassert>

using namespace MyEngine;

namespace {
constexpr UINT kPostProcessDescriptorRangeTotalCount = 2; // カラーと深度のSRV範囲数
constexpr UINT kPostProcessRootParameterCount = 3; // ポストエフェクト用ルートパラメータ数
constexpr UINT kPostProcessStaticSamplerCount = 2; // カラー用と深度用サンプラー数
constexpr UINT kColorSrvRegister = 0; // カラーSRVのレジスタ番号
constexpr UINT kDepthSrvRegister = 1; // 深度SRVのレジスタ番号
constexpr UINT kPostProcessSrvDescriptorCount = 1; // SRVテーブルごとのディスクリプタ数
constexpr UINT kPostProcessDescriptorRangeCount = 1; // ルートパラメータごとのディスクリプタレンジ数
constexpr UINT kPostProcessConstantRegister = 0; // ルート定数のレジスタ番号
constexpr UINT kPostProcessConstantRegisterSpace = 0; // ルート定数のレジスタ空間
constexpr UINT kPostProcessConstant32BitValueCount = 20; // ルート定数で渡す32bit値の数
constexpr UINT kLinearClampSamplerRegister = 0; // 線形クランプサンプラーのレジスタ番号
constexpr UINT kPointClampSamplerRegister = 1; // ポイントクランプサンプラーのレジスタ番号
constexpr UINT kPostProcessRenderTargetCount = 1; // ポストエフェクト描画で使用するRT数
constexpr UINT kPostProcessSampleCount = 1; // ポストエフェクト描画のマルチサンプル数

struct PostProcessPipelineDesc {
    PostEffectType effectType; // 生成対象のポストエフェクト種類
    const wchar_t* pixelShaderPath; // 使用するピクセルシェーダーパス
};

constexpr std::array<PostProcessPipelineDesc, GetPostEffectTypeCount()> kPostProcessPipelineDescs = { {
    { PostEffectType::Copy, L"resources/shaders/CopyImage.PS.hlsl" },
    { PostEffectType::Grayscale, L"resources/shaders/Grayscale.PS.hlsl" },
    { PostEffectType::Vignette, L"resources/shaders/Vignette.PS.hlsl" },
    { PostEffectType::BoxFilter, L"resources/shaders/BoxFilter.PS.hlsl" },
    { PostEffectType::GaussianFilter, L"resources/shaders/GaussianFilter.PS.hlsl" },
    { PostEffectType::LuminanceOutline, L"resources/shaders/LuminanceBasedOutline.PS.hlsl" },
    { PostEffectType::DepthOutline, L"resources/shaders/DepthBasedOutline.PS.hlsl" },
    { PostEffectType::RadialBlur, L"resources/shaders/RadialBlur.PS.hlsl" },
    { PostEffectType::Dissolve, L"resources/shaders/Dissolve.PS.hlsl" },
    { PostEffectType::Random, L"resources/shaders/Random.PS.hlsl" },
    { PostEffectType::Distortion, L"resources/shaders/Distortion.PS.hlsl" },
} }; // Initializeで生成するPSO一覧

/// <summary>
/// ポストエフェクト種類をPSO配列の番号へ変換する。
/// </summary>
size_t GetPipelineIndex(PostEffectType effectType)
{
    return static_cast<size_t>(effectType);
}
} // namespace

/// <summary>
/// ポストエフェクト用RootSignatureとPSOを生成する。
/// </summary>
bool PostProcessPipeline::Initialize(DirectXCommon* dxCommon)
{
    if (!dxCommon) {
        return false;
    }

    Finalize();
    dxCommon_ = dxCommon; // GPUリソース生成に使用するDirectX共通基盤

    CreateRootSignature();
    for (const PostProcessPipelineDesc& pipelineDesc : kPostProcessPipelineDescs) { // 生成対象PSOを順に作成する
        PipelineState* pipelineState = GetPipelineStateStorage(pipelineDesc.effectType); // PSOの格納先
        if (!pipelineState) {
            continue;
        }
        *pipelineState = CreatePipelineState(pipelineDesc.pixelShaderPath);
    }

    return IsReady();
}

/// <summary>
/// 保持しているRootSignatureとPSOを解放する。
/// </summary>
void PostProcessPipeline::Finalize()
{
    for (PipelineState& pipelineState : pipelineStates_) { // 各エフェクト用PSO
        pipelineState.Reset();
    }
    rootSignature_.Reset();
    dxCommon_ = nullptr;
}

/// <summary>
/// 描画に必要なRootSignatureとPSOが揃っているか確認する。
/// </summary>
bool PostProcessPipeline::IsReady() const
{
    if (!dxCommon_ || !rootSignature_) {
        return false;
    }

    for (const PostProcessPipelineDesc& pipelineDesc : kPostProcessPipelineDescs) { // 必須PSOの一覧
        if (!IsEffectReady(pipelineDesc.effectType)) {
            return false;
        }
    }

    return true;
}

/// <summary>
/// 指定したポストエフェクトのPSOが生成済みか確認する。
/// </summary>
bool PostProcessPipeline::IsEffectReady(PostEffectType effectType) const
{
    return GetPipelineState(effectType) != nullptr;
}

/// <summary>
/// 指定したポストエフェクトに対応するPSOを取得する。
/// </summary>
ID3D12PipelineState* PostProcessPipeline::GetPipelineState(PostEffectType effectType) const
{
    if (!IsValidPostEffectType(effectType)) {
        return nullptr;
    }

    return pipelineStates_[GetPipelineIndex(effectType)].Get();
}

/// <summary>
/// 全画面描画用RootSignatureを生成する。
/// </summary>
void PostProcessPipeline::CreateRootSignature()
{
    D3D12_DESCRIPTOR_RANGE descriptorRanges[kPostProcessDescriptorRangeTotalCount] = {}; // カラーと深度のSRV範囲
    descriptorRanges[0].BaseShaderRegister = kColorSrvRegister;
    descriptorRanges[0].NumDescriptors = kPostProcessSrvDescriptorCount;
    descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    descriptorRanges[1].BaseShaderRegister = kDepthSrvRegister;
    descriptorRanges[1].NumDescriptors = kPostProcessSrvDescriptorCount;
    descriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[kPostProcessRootParameterCount] = {}; // PixelShaderへ渡すルートパラメータ
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRanges[0];
    rootParameters[0].DescriptorTable.NumDescriptorRanges = kPostProcessDescriptorRangeCount;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRanges[1];
    rootParameters[1].DescriptorTable.NumDescriptorRanges = kPostProcessDescriptorRangeCount;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[2].Constants.ShaderRegister = kPostProcessConstantRegister;
    rootParameters[2].Constants.RegisterSpace = kPostProcessConstantRegisterSpace;
    rootParameters[2].Constants.Num32BitValues = kPostProcessConstant32BitValueCount;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[kPostProcessStaticSamplerCount] = {}; // カラー用と深度用サンプラー
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].ShaderRegister = kLinearClampSamplerRegister;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].ShaderRegister = kPointClampSamplerRegister;
    staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {}; // ルートシグネチャ設定
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);
    rootSignatureDesc.pStaticSamplers = staticSamplers;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob; // シリアライズ済みRootSignature
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob; // シリアライズ時のエラー情報
    HRESULT result = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);

    if (FAILED(result)) {
        if (errorBlob) {
            Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
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
/// 指定したピクセルシェーダーからPSOを生成する。
/// </summary>
PostProcessPipeline::PipelineState PostProcessPipeline::CreatePipelineState(const wchar_t* pixelShaderPath)
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
    pipelineDesc.NumRenderTargets = kPostProcessRenderTargetCount;
    pipelineDesc.RTVFormats[0] = dxCommon_->GetSwapChainFormat();
    pipelineDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    pipelineDesc.SampleDesc.Count = kPostProcessSampleCount;
    pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    pipelineDesc.RasterizerState = rasterizerDesc;
    pipelineDesc.BlendState = blendDesc;
    pipelineDesc.DepthStencilState = depthStencilDesc;

    PipelineState pipelineState; // 生成したポストエフェクト用PSO
    HRESULT result = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(&pipelineState));
    assert(SUCCEEDED(result));

    return pipelineState;
}

/// <summary>
/// 指定したポストエフェクトに対応するPSO格納先を取得する。
/// </summary>
PostProcessPipeline::PipelineState* PostProcessPipeline::GetPipelineStateStorage(PostEffectType effectType)
{
    if (!IsValidPostEffectType(effectType)) {
        return nullptr;
    }

    return &pipelineStates_[GetPipelineIndex(effectType)];
}