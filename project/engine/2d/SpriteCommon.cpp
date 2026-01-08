#include "SpriteCommon.h"
#include "Logger.h"
#include "StringUtility.h"

using namespace MyEngine;

void SpriteCommon::Initialize(DirectXCommon* dxCommon)
{
	//引数で受け取ってメンバ変数に記録する
    dxCommon_ = dxCommon;

	//グラフィクスパイプラインの生成
    CreateGraphicsPipeline();

}


void SpriteCommon::SetCommonDrawSetting()
{
    // 実行時のヌル参照を回避するための防御チェック
    if (!dxCommon_) {
        Logger::Log("SpriteCommon::SetCommonDrawSetting: dxCommon_ is null\n");
        return;
    }
    auto cmdList = dxCommon_->GetCommandList();
    if (!cmdList) {
        Logger::Log("SpriteCommon::SetCommonDrawSetting: command list is null\n");
        return;
    }
    if (!rootSignature_) {
        Logger::Log("SpriteCommon::SetCommonDrawSetting: rootSignature_ is null\n");
        return;
    }
    if (!graphicsPipelineState_) {
        Logger::Log("SpriteCommon::SetCommonDrawSetting: graphicsPipelineState_ is null\n");
        return;
    }

    cmdList->SetGraphicsRootSignature(rootSignature_.Get());
    cmdList->SetPipelineState(graphicsPipelineState_.Get()); // PSOを設定
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 形状を設定
}

bool SpriteCommon::IsReady() const
{
    return dxCommon_ && rootSignature_ && graphicsPipelineState_;
}

void SpriteCommon::CreateRootSignature()
{
    HRESULT hr;

    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
    descriptorRange[0].NumDescriptors = 1; // 数は1つ
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算

    // RootSignature作成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature {};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // RootParameter作成。複数設定できるので配列。
    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う (PixelShader, レジスタ0: マテリアルCBV)
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う (VertexShader, レジスタ0: WVP行列CBV)
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 0;
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う (PixelShader, レジスタ0: テクスチャSRV)
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う (PixelShader, レジスタ1: 光源CBV)
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    descriptionRootSignature.pParameters = rootParameters; // ルートパラメーター配列へのポインタ
    descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

    // Object3d シェーダーの期待に合わせたスタティックサンプラーを2つ用意する:
    // s0: 線形フィルタ + ラップ（既定）
    // s1: ポイントフィルタ + クランプ（アルファカットアウト用）
    D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};

    // s0: 線形フィルタ + ラップ
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].MipLODBias = 0.0f;
    staticSamplers[0].MinLOD = 0.0f;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // s1: ポイントフィルタ + クランプ
    staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[1].MipLODBias = 0.0f;
    staticSamplers[1].MinLOD = 0.0f;
    staticSamplers[1].ShaderRegister = 1;
    staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    // シリアライズしてバイナリにする
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        // エラー処理
        Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }

    // バイナリをもとに生成し、メンバ変数に保持する
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void SpriteCommon::CreateGraphicsPipeline()
{
    HRESULT hr;

    // ルートシグネチャの作成を先に実行する
    CreateRootSignature();

    // InputLayout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc {};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // BlendState の設定を blendMode_ に応じて切り替える
    D3D12_BLEND_DESC blendDesc {};
    D3D12_RENDER_TARGET_BLEND_DESC& rtBlend = blendDesc.RenderTarget[0];
    rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    rtBlend.LogicOpEnable = FALSE;
    rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
    rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;

    switch (blendMode_) {
    case BlendMode::None:
        rtBlend.BlendEnable = FALSE;
        rtBlend.SrcBlend = D3D12_BLEND_ONE;
        rtBlend.DestBlend = D3D12_BLEND_ZERO;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        break;
    case BlendMode::Alpha:
        rtBlend.BlendEnable = TRUE;
        rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        break;
    case BlendMode::Add:
        rtBlend.BlendEnable = TRUE;
        rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rtBlend.DestBlend = D3D12_BLEND_ONE;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ONE;
        break;
    case BlendMode::Multiply:
        rtBlend.BlendEnable = TRUE;
        rtBlend.SrcBlend = D3D12_BLEND_DEST_COLOR; // src * dest
        rtBlend.DestBlend = D3D12_BLEND_ZERO;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        break;
    case BlendMode::Screen:
        rtBlend.BlendEnable = TRUE;
        rtBlend.SrcBlend = D3D12_BLEND_ONE;
        rtBlend.DestBlend = D3D12_BLEND_INV_SRC_COLOR;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        break;
    default:
        rtBlend.BlendEnable = TRUE;
        rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        break;
    }

    // RasiterzerStateの設定
    D3D12_RASTERIZER_DESC rasterizerDesc {};
    // カリングしない（裏面も表示させる）
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    // 三角形の中を塗りつぶす
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // Shaderをコンパイルする
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Object3D.VS.hlsl", L"vs_6_0");
    assert(vertexShaderBlob != nullptr);

    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Object3D.PS.hlsl", L"ps_6_0");
    assert(pixelShaderBlob != nullptr);

    // PSOを生成する
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc {};
    graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get(); // メンバ変数のルートシグネチャを使用
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() }; // VertexShader
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() }; // PixelShader
    graphicsPipelineStateDesc.BlendState = blendDesc; // BlendState
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState
    // 書き込むRTVの情報
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    // 利用するトロポジ（形状）のタイプ。三角形
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // どのように画面に色を打ち込むかの設定（気にしなくて良い）
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // DepthStencilStateの設定
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    // Depthの機能を有効化する
    depthStencilDesc.DepthEnable = true;
    // 書き込みします
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    // 比較関数はLessEqual。つまり、近ければ描画される
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // DepthStencilの設定
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // PSO生成前のデバッグ情報
    {
        char buf[512];
        sprintf_s(buf, "SpriteCommon::CreateGraphicsPipeline: creating PSO. device=%p rootSig=%p VS_sz=%zu PS_sz=%zu RTVFormat=%d DSVFormat=%d\n",
            dxCommon_->GetDevice(),
            rootSignature_.Get(),
            vertexShaderBlob ? vertexShaderBlob->GetBufferSize() : 0,
            pixelShaderBlob ? pixelShaderBlob->GetBufferSize() : 0,
            graphicsPipelineStateDesc.RTVFormats[0],
            graphicsPipelineStateDesc.DSVFormat);
        Logger::Log(buf);
    }

    // 実際に生成し、メンバ変数に保持する
    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
    if (FAILED(hr) || !graphicsPipelineState_) {
        char buf[512];
        sprintf_s(buf, "SpriteCommon::CreateGraphicsPipeline: CreateGraphicsPipelineState failed. hr=0x%08X\n", static_cast<unsigned int>(hr));
        Logger::Log(buf);
        // If device reports removed reason, log it too
        HRESULT removedHr = dxCommon_->GetDevice()->GetDeviceRemovedReason();
        if (removedHr != S_OK) {
            char buf2[256]; sprintf_s(buf2, "SpriteCommon::CreateGraphicsPipeline: DeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(removedHr)); Logger::Log(buf2);
        }
        graphicsPipelineState_.Reset();
        return;
    }
}