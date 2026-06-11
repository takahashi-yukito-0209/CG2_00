#include "PostProcess.h"
#include "DirectXCommon.h"
#include "Logger.h"

using namespace MyEngine;
using namespace Microsoft::WRL;

/// <summary>
/// デストラクタ
/// </summary>
PostProcess::~PostProcess()
{
    Finalize();
}

/// <summary>
/// 初期化処理
/// </summary>
bool PostProcess::Initialize(DirectXCommon* dxCommon)
{
    if (!dxCommon) {
        return false;
    }
    // DirectXCommon のポインタを保存
    dxCommon_ = dxCommon;
    // ルートシグネチャとパイプラインステートの生成
    CreateRootSignature();
    CreatePipelineState();

    // すべてのリソースが正常に生成されたか確認
    return IsReady();
}

/// <summary>
/// 終了処理
/// </summary>
void PostProcess::Finalize()
{
    pipelineState_.Reset();
    rootSignature_.Reset();
    dxCommon_ = nullptr;
}

/// <summary>
/// ルートシグネチャの生成
/// </summary>
void PostProcess::CreateRootSignature()
{
    // デスクリプタレンジの定義（SRV 1つ）
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // ルートパラメータの定義
    D3D12_ROOT_PARAMETER rootParameters[1] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 静的サンプラーの定義
    D3D12_STATIC_SAMPLER_DESC staticSampler = {};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.ShaderRegister = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ルートシグネチャの定義
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = _countof(rootParameters);
    desc.pParameters = rootParameters;
    desc.pStaticSamplers = &staticSampler;
    desc.NumStaticSamplers = 1;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // ルートシグネチャのシリアライズと生成
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr)) {
        if (error) {
            Logger::Log(reinterpret_cast<char*>(error->GetBufferPointer()));
        }
        assert(false);
    }

    // シグネチャからルートシグネチャを生成
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

/// <summary>
/// パイプラインステートの生成
/// </summary>
void PostProcess::CreatePipelineState()
{
    // シェーダーのコンパイル
    auto vs = dxCommon_->CompileShader(L"resources/shaders/CopyImage.VS.hlsl", L"vs_6_0");
    auto ps = dxCommon_->CompileShader(L"resources/shaders/CopyImage.PS.hlsl", L"ps_6_0");

    // シェーダーのコンパイルに失敗していないか確認
    if (!vs || !ps) {
        return;
    }

    // 入力レイアウトは頂点シェーダーで頂点IDから位置を生成するため不要
    D3D12_INPUT_LAYOUT_DESC inputLayout = {};
    inputLayout.pInputElementDescs = nullptr;
    inputLayout.NumElements = 0;

    // パイプラインステートの定義
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    psoDesc.InputLayout = inputLayout;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = dxCommon_->GetSwapChainFormat();
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // ラスタライザーステートの設定（カリングなし、ソリッドフィル）
    D3D12_RASTERIZER_DESC rast = {};
    rast.FillMode = D3D12_FILL_MODE_SOLID;
    rast.CullMode = D3D12_CULL_MODE_NONE;
    rast.FrontCounterClockwise = FALSE;
    rast.DepthClipEnable = TRUE;
    psoDesc.RasterizerState = rast;

    // ブレンドステートの設定（ブレンドなし、全てのチャンネルを書き込み）
    D3D12_BLEND_DESC blend = {};
    D3D12_RENDER_TARGET_BLEND_DESC& rtBlend = blend.RenderTarget[0];
    rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    rtBlend.BlendEnable = FALSE;
    psoDesc.BlendState = blend;

    // デプスステンシルステートの設定（デプス・ステンシルテストなし）
    D3D12_DEPTH_STENCIL_DESC ds = {};
    ds.DepthEnable = FALSE;
    ds.StencilEnable = FALSE;
    // デプスステンシルステートをPSOにセット
    psoDesc.DepthStencilState = ds;
    // DSVは使用しないため、フォーマットはUNKNOWNにしておく
    psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

    // パイプラインステートの生成
    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

/// <summary>
/// 描画処理
/// </summary>
void PostProcess::DrawTexture(uint32_t srvIndex)
{
    // 描画に必要なリソースがすべて揃っているか確認
    if (!IsReady()) {
        return;
    }
    // コマンドリストを取得
    auto cmd = dxCommon_->GetCommandList();
    // ルートシグネチャとパイプラインステートをセット
    cmd->SetGraphicsRootSignature(rootSignature_.Get());
    cmd->SetPipelineState(pipelineState_.Get());

    // SRVをルートパラメータにセット
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = dxCommon_->GetSRVGPUDescriptorHandle(srvIndex);
    // SRVヒープをコマンドリストにセット
    ID3D12DescriptorHeap* pHeaps[1] = { dxCommon_->GetSrvDescriptorHeap() };

    // ディスクリプタヒープとSRVをセット
    cmd->SetDescriptorHeaps(1, pHeaps);
    cmd->SetGraphicsRootDescriptorTable(0, srvHandle);

    // プリミティブトポロジーを設定して描画コマンドを発行（フルスクリーン三角形）
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // 頂点バッファは使用せず、頂点IDから位置を生成するため、DrawInstancedで3頂点を描画する
    cmd->DrawInstanced(3, 1, 0, 0);
}
