#include "Object3dCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#include "mathUtility.h"

using namespace MyEngine;

void Object3dCommon::Initialize(DirectXCommon* dxCommon)
{
    // 引数で受け取ってメンバ変数に記録する
    dxCommon_ = dxCommon;
    // 共有の平行光源用定数バッファを作成
    // これにより main/ImGui からすべての Object3d インスタンスで使用する単一のライトを編集できる
    directionalLightResource_ = dxCommon_->GetDevice() ? dxCommon_->CreateBufferResource(sizeof(Object3d::DirectionalLight)) : nullptr;
    if (directionalLightResource_) {
        directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
        // default values
        directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
        directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
        directionalLightData_->intensity = 1.0f;
    }
    // Camera constant buffer
    cameraResource_ = dxCommon_->CreateBufferResource(sizeof(CameraForGPU));
    if (cameraResource_) {
        cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
        cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
        cameraData_->pad = 0.0f;
    }
    // グラフィックスパイプラインの生成
    CreateGraphicsPipeline();

    // Create instancing resources (structured buffer + SRV)
    const uint32_t kNumInstance = 10; // default maximum instances for particle demo
    kNumInstance_ = kNumInstance;

    // Create a GPU-visible SRV for a StructuredBuffer containing TransformationMatrix[kNumInstance]
    D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
    instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN; // structured buffer
    instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    instancingSrvDesc.Buffer.FirstElement = 0;
    instancingSrvDesc.Buffer.NumElements = kNumInstance;
    instancingSrvDesc.Buffer.StructureByteStride = sizeof(Object3d::TransformationMatrix);
    instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    // Create a default-size buffer resource in UPLOAD heap to store instance data
    size_t instancingBufferSize = sizeof(Object3d::TransformationMatrix) * kNumInstance;
    instancingResource_ = dxCommon_->CreateBufferResource(instancingBufferSize);
    // Map for CPU write
    if (instancingResource_) {
        instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));
        // initialize to identity
        MathUtility math;
        for (uint32_t i = 0; i < kNumInstance; ++i) {
            instancingData_[i].WVP = math.MakeIdentity4x4();
            instancingData_[i].World = math.MakeIdentity4x4();
        }
    }

    // Create SRV descriptor in the global SRV heap
    // Choose a descriptor slot unlikely to be used by TextureManager: use the last slot in the heap
    uint32_t srvIndex = DirectXCommon::kMaxSRVCount - 1;
    instancingSrvHandleCPU_ = dxCommon_->GetSRVCPUDescriptorHandle(srvIndex);
    instancingSrvHandleGPU_ = dxCommon_->GetSRVGPUDescriptorHandle(srvIndex);
    dxCommon_->GetDevice()->CreateShaderResourceView(instancingResource_.Get(), &instancingSrvDesc, instancingSrvHandleCPU_);

    // Debug log
    {
        char buf[256];
        sprintf_s(buf, "Object3dCommon::Initialize: created instancingResource size=%zu srvIndex=%u srvGPU=0x%016llX\n",
            instancingBufferSize, srvIndex, static_cast<unsigned long long>(instancingSrvHandleGPU_.ptr));
        Logger::Log(buf);
    }
}

void Object3dCommon::SetBlendMode(MyEngine::BlendMode mode)
{
    // 選択されたブレンドモードを保存し、PSOを再生成する
    blendMode_ = mode;
    // 新しいブレンドステートを適用するためパイプラインを再生成
    CreateGraphicsPipeline();
}

void Object3dCommon::SetInstancingDrawSetting()
{
    auto cmdList = dxCommon_ ? dxCommon_->GetCommandList() : nullptr;
    if (!cmdList) {
        Logger::Log("Object3dCommon::SetInstancingDrawSetting: command list is null\n");
        return;
    }
    if (!rootSignature_) {
        Logger::Log("Object3dCommon::SetInstancingDrawSetting: rootSignature_ is null\n");
        return;
    }
    if (!instancingPipelineState_) {
        Logger::Log("Object3dCommon::SetInstancingDrawSetting: instancingPipelineState_ is null\n");
        return;
    }

    cmdList->SetGraphicsRootSignature(rootSignature_.Get());
    cmdList->SetPipelineState(instancingPipelineState_.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::SetCommonDrawSetting()
{
    // 実行時のヌル参照を回避するための防御チェック
    auto cmdList = dxCommon_ ? dxCommon_->GetCommandList() : nullptr;
    if (!cmdList) {
        Logger::Log("Object3dCommon::SetCommonDrawSetting: command list is null\n");
        return;
    }
    if (!rootSignature_) {
        Logger::Log("Object3dCommon::SetCommonDrawSetting: rootSignature_ is null\n");
        return;
    }
    if (!graphicsPipelineState_) {
        Logger::Log("Object3dCommon::SetCommonDrawSetting: graphicsPipelineState_ is null\n");
        return;
    }

    // ルートシグネチャをセットするコマンド
    cmdList->SetGraphicsRootSignature(rootSignature_.Get());
    // パイプラインステートをセットするコマンド
    cmdList->SetPipelineState(graphicsPipelineState_.Get()); // PSOを設定                                                                          
    // プリミティブトポロジーをセットするコマンド
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 形状を設定
}

void Object3dCommon::CreateRootSignature() {
    HRESULT hr;
    // ディスクリプタレンジ作成 (pixel texture SRV)
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
    descriptorRange[0].NumDescriptors = 1; // 数は1つ
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算

    // ディスクリプタレンジ作成 (vertex instancing SRV)
    D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
    descriptorRangeForInstancing[0].BaseShaderRegister = 0; // t0 in VS
    descriptorRangeForInstancing[0].NumDescriptors = 1;
    descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // RootSignature作成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature {};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // RootParameter作成。複数設定できるので配列。
    // Note: keep existing indices for compatibility: 0=material CBV(Pixel), 1=WVP CBV(Vertex), 2=Texture SRV Table(Pixel), 3=Light CBV(Pixel)
    // We'll append instancing SRV Table at index 4 (Vertex) so existing code does not need to change indices.
    D3D12_ROOT_PARAMETER rootParameters[6] = {};
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

    // rootParameters[4] : DescriptorTable for instancing StructuredBuffer (Vertex shader, t0)
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[4].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
    rootParameters[4].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

    // rootParameters[5] : Camera CBV (Pixel shader, b2)
    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[5].Descriptor.ShaderRegister = 2;

    // 注: 平行光源CBVは Object3dCommon に保存されたGPUアドレスを使ってオブジェクト毎にバインドされる

    /// ルートシグネチャの説明
    descriptionRootSignature.pParameters = rootParameters; // ルートパラメーター配列へのポインタ
    descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

    // スタティックサンプラーの設定
    // スタティックサンプラーを2つ用意する:
    //  - s0: 線形フィルタ、ラップアドレッシング（ほとんどのモデルの既定）
    //  - s1: ポイントフィルタ、クランプアドレッシング（フェンスのようなアルファカットアウト用テクスチャ向け）
    D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};

    // s0: linear + wrap
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

    // s1: ポイントフィルタ + クランプ（アルファカットアウトテクスチャのブリーディングを防ぐために有用）
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

void Object3dCommon::CreateGraphicsPipeline() {
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
    // 裏面（時計回り）を表示しない
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    // 三角形の中を塗りつぶす
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    // デフォルトのワインディング（時計回りを前面）を使用し、多くのOBJエクスポートと整合させる
    // もしモデルの一部が裏返しに見える場合は、ここでこのフラグを切り替えるよりも
    // カリングを無効にするか OBJ ローダー側でワインディングを修正することを検討してください。
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;

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
    // Depth write mask: disable depth writes for blend modes that use
    // transparency (e.g. Alpha) to prevent incorrect occlusion. For opaque
    // (None) rendering, enable depth writes.
    if (blendMode_ == BlendMode::Alpha || blendMode_ == BlendMode::Add || blendMode_ == BlendMode::Multiply || blendMode_ == BlendMode::Screen) {
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    } else {
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    }
    // 比較関数はLessEqual。つまり、近ければ描画される
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // DepthStencilの設定
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // 実際に生成し、メンバ変数に保持する
    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
    assert(SUCCEEDED(hr));

    // Create separate PSO for instancing/particle rendering using Particle shaders
    Microsoft::WRL::ComPtr<IDxcBlob> instVS = dxCommon_->CompileShader(L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> instPS = dxCommon_->CompileShader(L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");
    if (instVS && instPS) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC instDesc = {};
        instDesc.pRootSignature = rootSignature_.Get();
        instDesc.InputLayout = inputLayoutDesc;
        instDesc.VS = { instVS->GetBufferPointer(), instVS->GetBufferSize() };
        instDesc.PS = { instPS->GetBufferPointer(), instPS->GetBufferSize() };
        instDesc.BlendState = blendDesc;
        instDesc.RasterizerState = rasterizerDesc;
        instDesc.NumRenderTargets = 1;
        instDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        instDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        instDesc.SampleDesc.Count = 1;
        instDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        instDesc.DepthStencilState = depthStencilDesc;
        instDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        HRESULT r = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&instDesc, IID_PPV_ARGS(&instancingPipelineState_));
        if (FAILED(r)) {
            char buf[256]; sprintf_s(buf, "Object3dCommon::CreateGraphicsPipeline: failed to create instancing PSO hr=0x%08X\n", static_cast<unsigned int>(r)); Logger::Log(buf);
            instancingPipelineState_.Reset();
        }
    } else {
        Logger::Log("Object3dCommon::CreateGraphicsPipeline: Particle shaders not found, instancing PSO not created\n");
    }
}