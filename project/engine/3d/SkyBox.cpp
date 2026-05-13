#include "SkyBox.h"
#include "engine/3d/Camera.h"
#include "engine/utility/StringUtility.h"
#include "engine/utility/Logger.h"
#include <vector>
#include "engine/base/DirectXCommon.h"

using namespace MyEngine;

// 頂点構造体（位置のみ、テクスチャ座標や法線は不要）
struct Vertex {
    float pos[3];
};

/// <summary>
/// デストラクタ
/// </summary>
SkyBox::~SkyBox() {
    Finalize();
}

/// <summary>
/// 初期化
/// </summary>
void SkyBox::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t srvIndex) {
    // 引数で受け取ってメンバ変数に記録する
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    srvIndex_ = srvIndex;

    // 頂点データ（キューブの8頂点を定義）
    std::vector<Vertex> baseVerts = {
        // -X
        {{-1, -1, -1}}, {{-1, -1, 1}}, {{-1, 1, 1}}, {{-1, 1, -1}},
        // +X
        {{1, -1, 1}}, {{1, -1, -1}}, {{1, 1, -1}}, {{1, 1, 1}},
        // -Y
        {{-1, -1, 1}}, {{1, -1, 1}}, {{1, -1, -1}}, {{-1, -1, -1}},
        // +Y
        {{-1, 1, -1}}, {{1, 1, -1}}, {{1, 1, 1}}, {{-1, 1, 1}},
        // -Z
        {{1, -1, -1}}, {{-1, -1, -1}}, {{-1, 1, -1}}, {{1, 1, -1}},
        // +Z
        {{-1, -1, 1}}, {{1, -1, 1}}, {{1, 1, 1}}, {{-1, 1, 1}},
    };

    // インデックスデータ（12三角形を定義）
    std::vector<uint16_t> indices = {
        0,1,2, 0,2,3,
        4,5,6, 4,6,7,
        8,9,10, 8,10,11,
        12,13,14, 12,14,15,
        16,17,18, 16,18,19,
        20,21,22, 20,22,23
    };

    // インデックスを展開して非インデックスド描画用の頂点バッファを作成する
    std::vector<Vertex> expanded;
    expanded.reserve(indices.size());
    for (auto idx : indices) {
        expanded.push_back(baseVerts[idx]);
    }
    // 展開後の頂点数を記録
    indexCount_ = static_cast<uint32_t>(expanded.size()); 

    // 頂点バッファの作成とデータ転送
    size_t vbSize = expanded.size() * sizeof(Vertex);
    vertexBuffer_ = dxCommon_->CreateBufferResource(vbSize);
    void* vbPtr = nullptr; vertexBuffer_->Map(0, nullptr, &vbPtr); memcpy(vbPtr, expanded.data(), vbSize);

    // 頂点バッファビューの作成
    vbView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = static_cast<UINT>(vbSize);
    vbView_.StrideInBytes = sizeof(Vertex);

    // 定数バッファの作成とマッピング
    constantBuffer_ = dxCommon_->CreateBufferResource(256);
    constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedCB_));
    // 定数バッファの内容は描画前にUpdateViewProjで更新するため、ここでは初期化しない
    {
        HRESULT hr = S_OK;

        // ルートパラメータの定義
        D3D12_ROOT_PARAMETER rootParams[2] = {};
        rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[0].Descriptor.ShaderRegister = 0; // b0
        rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // SRVテーブルの定義
        D3D12_DESCRIPTOR_RANGE range = {};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 1;
        range.BaseShaderRegister = 0; // t0
        range.RegisterSpace = 0;
        range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // ルートパラメータの定義（SRVテーブル）
        rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[1].DescriptorTable.pDescriptorRanges = &range;
        rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // スタティックサンプラーの定義
        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.ShaderRegister = 0;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // ルートシグネチャの説明
        D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
        rsDesc.NumParameters = _countof(rootParams);
        rsDesc.pParameters = rootParams;
        rsDesc.NumStaticSamplers = 1;
        rsDesc.pStaticSamplers = &samplerDesc;
        rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        // ルートシグネチャのシリアライズと生成
        Microsoft::WRL::ComPtr<ID3DBlob> sig;
        Microsoft::WRL::ComPtr<ID3DBlob> err;
        hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
        if (FAILED(hr)) {
            if (err) Logger::Log(reinterpret_cast<const char*>(err->GetBufferPointer()));
        }
        hr = dxCommon_->GetDevice()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
        assert(SUCCEEDED(hr));
    }

    // パイプラインステートの作成
    {
        // シェーダーのコンパイル
        auto vs = dxCommon_->CompileShader(L"resources/shaders/SkyBox.VS.hlsl", L"vs_6_0");
        auto ps = dxCommon_->CompileShader(L"resources/shaders/SkyBox.PS.hlsl", L"ps_6_0");

        // 入力レイアウトの定義（位置のみ）
        D3D12_INPUT_ELEMENT_DESC inputDesc[1] = {};
        inputDesc[0].SemanticName = "POSITION";
        inputDesc[0].SemanticIndex = 0;
        inputDesc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
        inputDesc[0].AlignedByteOffset = 0;
        inputDesc[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

        // パイプラインステートの説明
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
        desc.pRootSignature = rootSignature_.Get();
        desc.InputLayout = { inputDesc, _countof(inputDesc) };
        desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
        desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        // CullModeをNONEにして両面描画にすることで、キューブの内側からも外側からもテクスチャが見えるようにする
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.RasterizerState.DepthClipEnable = TRUE;
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        desc.SampleMask = UINT_MAX;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = dxCommon_->GetSwapChainFormat();
        desc.SampleDesc.Count = 1;
        // スカイボックスは常に最も遠くに描画されるべきなので、深度テストは常にパスさせるが、深度書き込みは行わない設定にする
        desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        desc.DepthStencilState.DepthEnable = FALSE;
        desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

        // PSOの生成とエラーチェック
        HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState_));
        if (FAILED(hr)) {
            char buf[256];
            sprintf_s(buf, "ERROR SkyBox::Initialize: CreateGraphicsPipelineState failed hr=0x%08X\n", static_cast<unsigned int>(hr));
            Logger::Log(buf);
        } else {
            char buf[256];
            sprintf_s(buf, "DEBUG SkyBox::Initialize: PSO created. srvIndex=%u vb.gpu=0x%016llX vertexCount=%u\n",
                static_cast<unsigned int>(srvIndex_),
                static_cast<unsigned long long>(vbView_.BufferLocation),
                static_cast<unsigned int>(indexCount_));
            Logger::Log(buf);
        }
        assert(SUCCEEDED(hr));
    }
}

/// <summary>
/// 終了
/// </summary>
void SkyBox::Finalize() {
    // 定数バッファのアンマップとリセット
    if (constantBuffer_) {
        constantBuffer_->Unmap(0, nullptr);
        mappedCB_ = nullptr;
        constantBuffer_.Reset();
    }
    
    // バッファのリセット
    vertexBuffer_.Reset();
    indexBuffer_.Reset();
}

/// <summary>
/// ビュープロジェクション行列の更新
/// </summary>
void SkyBox::UpdateViewProj(const float vp[16]) {
    if (!mappedCB_) return;
    memcpy(mappedCB_, vp, sizeof(float) * 16);
}

/// <summary>
/// 描画
/// </summary>
void SkyBox::Draw(Camera* camera) {
    if (!dxCommon_) return;
    auto cmd = dxCommon_->GetCommandList();
    if (!cmd) return;
    
    // エラーチェックとログ出力
    if (!pipelineState_) {
        Logger::Log("ERROR SkyBox::Draw: pipelineState_ is null\n");
        return;
    }
    if (!rootSignature_) {
        Logger::Log("ERROR SkyBox::Draw: rootSignature_ is null\n");
        return;
    }
    if (vbView_.BufferLocation == 0) {
        Logger::Log("ERROR SkyBox::Draw: vertex buffer GPU address is null\n");
        return;
    }

    // デバッグ用: 描画前のSRVインデックス、頂点バッファのGPUアドレス、描画する頂点数をログ出力して確認する
    {
        char buf[1024];
        sprintf_s(buf, "DEBUG SkyBox::Draw: attempt srvIndex=%u vb.gpu=0x%016llX vertexCount=%u\n",
            static_cast<unsigned int>(srvIndex_), static_cast<unsigned long long>(vbView_.BufferLocation), static_cast<unsigned int>(indexCount_));
        Logger::Log(buf);
    }

    // カメラのビューとプロジェクション行列をログ出力して確認する
    if (camera) {
        const auto& view = camera->GetViewMatrix();
        const auto& proj = camera->GetProjectionMatrix();
        char buf2[1024];
        sprintf_s(buf2, "DEBUG SkyBox::Draw: Camera translate=(%f,%f,%f) view.m00=%f view.m11=%f proj.m00=%f proj.m11=%f\n",
            camera->GetTranslate().x, camera->GetTranslate().y, camera->GetTranslate().z,
            view.m[0][0], view.m[1][1], proj.m[0][0], proj.m[1][1]);
        Logger::Log(buf2);
    } else {
        Logger::Log("DEBUG SkyBox::Draw: camera is null\n");
    }

    // デバッグ用: SRVマネージャーからSRVのGPUハンドルを取得してログ出力する
    if (srvManager_) {
        auto h = srvManager_->GetGPUDescriptorHandle(srvIndex_);
        char buf3[256];
        sprintf_s(buf3, "DEBUG SkyBox::Draw: srvManager GPU handle.ptr=0x%016llX\n", static_cast<unsigned long long>(h.ptr));
        Logger::Log(buf3);
    } else if (dxCommon_) {
        auto h = dxCommon_->GetSRVGPUDescriptorHandle(srvIndex_);
        char buf4[256];
        sprintf_s(buf4, "DEBUG SkyBox::Draw: dxCommon GPU handle.ptr=0x%016llX\n", static_cast<unsigned long long>(h.ptr));
        Logger::Log(buf4);
    }

    // SRVヒープの設定
    ID3D12DescriptorHeap* heaps[] = { dxCommon_->GetSrvDescriptorHeap() };
    cmd->SetDescriptorHeaps(_countof(heaps), heaps);

    // ルートシグネチャとパイプラインステートの設定
    cmd->SetGraphicsRootSignature(rootSignature_.Get());
    cmd->SetPipelineState(pipelineState_.Get());

    // SRVテーブルをルート1に設定
    if (srvManager_) {
        srvManager_->SetGraphicsRootDescriptorTable(1, srvIndex_);
    } else {
        // SRVマネージャーがない場合は、DirectXCommonから直接GPUハンドルを取得して設定する
        cmd->SetGraphicsRootDescriptorTable(1, dxCommon_->GetSRVGPUDescriptorHandle(srvIndex_));
    }

    // 定数バッファの更新とルート0へのCBV設定
    if (constantBuffer_) {
        // カメラがある場合はビューとプロジェクションを更新して定数バッファにコピーする
        if (camera) {
            Math::Matrix4x4 view = camera->GetViewMatrix();
            Math::Matrix4x4 proj = camera->GetProjectionMatrix();
            // ビュー行列の平行移動成分をゼロにして、カメラの位置に関係なく常に同じ位置にスカイボックスが描画されるようにする
            view.m[3][0] = 0.0f;
            view.m[3][1] = 0.0f;
            view.m[3][2] = 0.0f;
            Math::Matrix4x4 vp = MathUtil::Multiply(view, proj);
            // 定数バッファにコピー
            if (mappedCB_) {
                memcpy(mappedCB_, &vp, sizeof(vp));
            }
        }

        // 定数バッファのGPUアドレスをルート0に設定
        cmd->SetGraphicsRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());
    }

    // 頂点バッファの設定と描画コマンド
    cmd->IASetVertexBuffers(0, 1, &vbView_);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->DrawInstanced(indexCount_, 1, 0, 0);
}
