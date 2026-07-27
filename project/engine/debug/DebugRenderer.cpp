#include "DebugRenderer.h"

#include "Logger.h"
#include "Object3d.h"
#include "Sprite.h"
#include "mathUtility.h"
#include <cassert>
#include <cstring>
#include <d3dcompiler.h>

using namespace MyEngine;
using Microsoft::WRL::ComPtr;

namespace {
constexpr size_t kInitialVertexCapacity = 4096; // 初期頂点バッファ容量
constexpr UINT kRenderTargetCount = 1; // 使用するレンダーターゲット数
constexpr UINT kSampleCount = 1; // マルチサンプル数
constexpr float kFallbackRenderSize = 1.0f; // 画面サイズ取得失敗時の代替サイズ
const Math::Vector4 kAxisXColor = { 1.0f, 0.1f, 0.1f, 1.0f }; // X 軸の表示色
const Math::Vector4 kAxisYColor = { 0.1f, 1.0f, 0.1f, 1.0f }; // Y 軸の表示色
const Math::Vector4 kAxisZColor = { 0.1f, 0.3f, 1.0f, 1.0f }; // Z 軸の表示色

/// <summary>
/// 3D 座標値を作成する。
/// </summary>
Math::Vector3 MakeVector3(float x, float y, float z)
{
    Math::Vector3 value = { x, y, z }; // 返却する 3D 座標
    return value;
}

/// <summary>
/// 2D 座標値を作成する。
/// </summary>
Math::Vector2 MakeVector2(float x, float y)
{
    Math::Vector2 value = { x, y }; // 返却する 2D 座標
    return value;
}
} // namespace

/// <summary>
/// デストラクタ。
/// </summary>
DebugRenderer::~DebugRenderer()
{
    Finalize();
}

/// <summary>
/// GPU参照が終わるまでD3D12リソースの解放を遅延する。
/// </summary>
void DebugRenderer::DeferReleaseResource(Microsoft::WRL::ComPtr<ID3D12Resource>& resource)
{
    if (!resource) {
        return;
    }

    if (dxCommon_) {
        dxCommon_->DeferReleaseResource(resource);
        return;
    }

    resource.Reset();
}

/// <summary>
/// GPUリソースを解放する。
/// </summary>
void DebugRenderer::Finalize()
{
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        if (vertexResources_[frameIndex] && mappedVertexData_[frameIndex]) {
            vertexResources_[frameIndex]->Unmap(0, nullptr);
        }
        mappedVertexData_[frameIndex] = nullptr;
        DeferReleaseResource(vertexResources_[frameIndex]);

        if (transformationResources_[frameIndex] && mappedTransformationData_[frameIndex]) {
            transformationResources_[frameIndex]->Unmap(0, nullptr);
        }
        mappedTransformationData_[frameIndex] = nullptr;
        DeferReleaseResource(transformationResources_[frameIndex]);
    }

    rootSignature_.Reset();
    pipelineState_.Reset();
    pipelineStateNoDepth_.Reset();
    vertexBufferView_ = {};
    vertexCapacity_ = 0;
    lineVertices3D_.clear();
    lineVertices2D_.clear();
    dxCommon_ = nullptr;
}

/// <summary>
/// デバッグライン描画に必要な GPU リソースを初期化する。
/// </summary>
void DebugRenderer::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    if (!dxCommon_) {
        Logger::Warn("DebugRenderer::Initialize skipped: dxCommon is null\n");
        return;
    }

    CreateRootSignature();
    CreateGraphicsPipeline();

    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        transformationResources_[frameIndex] = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
        HRESULT hr = transformationResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedTransformationData_[frameIndex])); // 定数バッファのマップ結果
        if (FAILED(hr)) {
            Logger::Warn("DebugRenderer::Initialize transformation map failed\n");
            mappedTransformationData_[frameIndex] = nullptr;
        }
    }

    EnsureVertexCapacity(kInitialVertexCapacity);
}

/// <summary>
/// フレーム内に蓄積したデバッグラインをクリアする。
/// </summary>
void DebugRenderer::BeginFrame()
{
    lineVertices3D_.clear();
    lineVertices2D_.clear();
}

/// <summary>
/// 3D 空間上の線分を追加する。
/// </summary>
void DebugRenderer::DrawLine3D(const Math::Vector3& start, const Math::Vector3& end, const Math::Vector4& color)
{
    AddLine(lineVertices3D_, start, end, color);
}

/// <summary>
/// 2D 画面座標上の線分を追加する。
/// </summary>
void DebugRenderer::DrawLine2D(const Math::Vector2& start, const Math::Vector2& end, const Math::Vector4& color)
{
    AddLine(lineVertices2D_, MakeVector3(start.x, start.y, 0.0f), MakeVector3(end.x, end.y, 0.0f), color);
}

/// <summary>
/// XZ 平面上にデバッググリッドを追加する。
/// </summary>
void DebugRenderer::DrawGrid(const Math::Vector3& center, int halfLineCount, float spacing, const Math::Vector4& color)
{
    if (halfLineCount <= 0 || spacing <= 0.0f) {
        return;
    }

    const float halfSize = static_cast<float>(halfLineCount) * spacing; // グリッド中心から端までの距離
    for (int lineIndex = -halfLineCount; lineIndex <= halfLineCount; ++lineIndex) {
        const float offset = static_cast<float>(lineIndex) * spacing; // 中心から現在ラインまでのオフセット
        DrawLine3D(MakeVector3(center.x - halfSize, center.y, center.z + offset), MakeVector3(center.x + halfSize, center.y, center.z + offset), color);
        DrawLine3D(MakeVector3(center.x + offset, center.y, center.z - halfSize), MakeVector3(center.x + offset, center.y, center.z + halfSize), color);
    }
}

/// <summary>
/// AABB のワイヤーフレームを追加する。
/// </summary>
void DebugRenderer::DrawAABB(const Math::Vector3& min, const Math::Vector3& max, const Math::Vector4& color)
{
    const Math::Vector3 p000 = MakeVector3(min.x, min.y, min.z); // AABB の下前左頂点
    const Math::Vector3 p001 = MakeVector3(min.x, min.y, max.z); // AABB の下奥左頂点
    const Math::Vector3 p010 = MakeVector3(min.x, max.y, min.z); // AABB の上前左頂点
    const Math::Vector3 p011 = MakeVector3(min.x, max.y, max.z); // AABB の上奥左頂点
    const Math::Vector3 p100 = MakeVector3(max.x, min.y, min.z); // AABB の下前右頂点
    const Math::Vector3 p101 = MakeVector3(max.x, min.y, max.z); // AABB の下奥右頂点
    const Math::Vector3 p110 = MakeVector3(max.x, max.y, min.z); // AABB の上前右頂点
    const Math::Vector3 p111 = MakeVector3(max.x, max.y, max.z); // AABB の上奥右頂点

    DrawLine3D(p000, p100, color);
    DrawLine3D(p100, p101, color);
    DrawLine3D(p101, p001, color);
    DrawLine3D(p001, p000, color);
    DrawLine3D(p010, p110, color);
    DrawLine3D(p110, p111, color);
    DrawLine3D(p111, p011, color);
    DrawLine3D(p011, p010, color);
    DrawLine3D(p000, p010, color);
    DrawLine3D(p100, p110, color);
    DrawLine3D(p101, p111, color);
    DrawLine3D(p001, p011, color);
}

/// <summary>
/// 2D 矩形のワイヤーフレームを追加する。
/// </summary>
void DebugRenderer::DrawRect2D(const Math::Vector2& leftTop, const Math::Vector2& size, const Math::Vector4& color)
{
    const Math::Vector2 rightTop = MakeVector2(leftTop.x + size.x, leftTop.y); // 矩形の右上座標
    const Math::Vector2 rightBottom = MakeVector2(leftTop.x + size.x, leftTop.y + size.y); // 矩形の右下座標
    const Math::Vector2 leftBottom = MakeVector2(leftTop.x, leftTop.y + size.y); // 矩形の左下座標

    DrawLine2D(leftTop, rightTop, color);
    DrawLine2D(rightTop, rightBottom, color);
    DrawLine2D(rightBottom, leftBottom, color);
    DrawLine2D(leftBottom, leftTop, color);
}

/// <summary>
/// Object3d の位置と回転に合わせたローカル軸を追加する。
/// </summary>
void DebugRenderer::DrawObjectAxis(const Object3d& object, float length)
{
    if (length <= 0.0f) {
        return;
    }

    const Math::Vector3 origin = object.GetTranslate(); // オブジェクト軸の原点
    const Math::Vector3 rotation = object.GetRotate(); // オブジェクトの回転値
    const Math::Matrix4x4 rotateMatrix = MathUtil::MakeAffineMatrix(MakeVector3(1.0f, 1.0f, 1.0f), rotation, MakeVector3(0.0f, 0.0f, 0.0f)); // 軸方向計算用の回転行列
    const Math::Vector3 xAxis = MathUtil::Transform(MakeVector3(length, 0.0f, 0.0f), rotateMatrix); // 回転を反映した X 軸方向
    const Math::Vector3 yAxis = MathUtil::Transform(MakeVector3(0.0f, length, 0.0f), rotateMatrix); // 回転を反映した Y 軸方向
    const Math::Vector3 zAxis = MathUtil::Transform(MakeVector3(0.0f, 0.0f, length), rotateMatrix); // 回転を反映した Z 軸方向

    DrawLine3D(origin, origin + xAxis, kAxisXColor);
    DrawLine3D(origin, origin + yAxis, kAxisYColor);
    DrawLine3D(origin, origin + zAxis, kAxisZColor);
}

/// <summary>
/// Sprite の表示矩形に合わせたワイヤーフレームを追加する。
/// </summary>
void DebugRenderer::DrawSpriteRect(const Sprite& sprite, const Math::Vector4& color)
{
    const Math::Vector2 position = sprite.GetPosition(); // スプライトの基準座標
    const Math::Vector2 size = sprite.GetSize(); // スプライトの表示サイズ
    const Math::Vector2 anchor = sprite.GetAnchorPoint(); // スプライトのアンカーポイント
    const Math::Vector2 leftTop = MakeVector2(position.x - size.x * anchor.x, position.y - size.y * anchor.y); // アンカーを反映した左上座標
    DrawRect2D(leftTop, size, color);
}

/// <summary>
/// 蓄積した 3D ラインを描画する。
/// </summary>
void DebugRenderer::Render3D(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix)
{
    const Math::Matrix4x4 viewProjection = MathUtil::Multiply(viewMatrix, projectionMatrix); // 3D ライン描画用のビュー射影行列
    RenderLines(lineVertices3D_, viewProjection, pipelineState_.Get());
}

/// <summary>
/// 蓄積した 2D ラインを描画する。
/// </summary>
void DebugRenderer::Render2D()
{
    if (!dxCommon_) {
        return;
    }

    float renderWidth = dxCommon_->GetRenderWidth(); // 2D 描画に使う描画幅
    float renderHeight = dxCommon_->GetRenderHeight(); // 2D 描画に使う描画高さ
    if (renderWidth <= 0.0f) {
        renderWidth = kFallbackRenderSize;
    }
    if (renderHeight <= 0.0f) {
        renderHeight = kFallbackRenderSize;
    }

    const Math::Matrix4x4 projectionMatrix = MathUtil::MakeOrthographicMatrix(0.0f, 0.0f, renderWidth, renderHeight, 0.0f, 1.0f); // 2D ライン描画用の正射影行列
    RenderLines(lineVertices2D_, projectionMatrix, pipelineStateNoDepth_.Get());
}

/// <summary>
/// デバッグライン描画用の RootSignature を作成する。
/// </summary>
void DebugRenderer::CreateRootSignature()
{
    D3D12_ROOT_PARAMETER rootParameters[1] = {}; // RootSignature に登録するルートパラメータ
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {}; // RootSignature の作成設定
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);

    ComPtr<ID3DBlob> signatureBlob; // シリアライズ済み RootSignature
    ComPtr<ID3DBlob> errorBlob; // RootSignature シリアライズ時のエラー情報
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob); // RootSignature シリアライズ結果
    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
        return;
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

/// <summary>
/// デバッグライン描画用の PSO を作成する。
/// </summary>
void DebugRenderer::CreateGraphicsPipeline()
{
    CreateGraphicsPipeline(true, pipelineState_);
    CreateGraphicsPipeline(false, pipelineStateNoDepth_);
}

/// <summary>
/// 深度設定に合わせたデバッグライン描画用の PSO を作成する。
/// </summary>
void DebugRenderer::CreateGraphicsPipeline(bool enableDepth, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipelineState)
{
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {}; // 頂点入力レイアウト要素
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].SemanticName = "COLOR";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {}; // 頂点入力レイアウト設定
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    D3D12_BLEND_DESC blendDesc = {}; // 半透明ライン用ブレンド設定
    D3D12_RENDER_TARGET_BLEND_DESC& renderTargetBlend = blendDesc.RenderTarget[0]; // 先頭レンダーターゲットのブレンド設定
    renderTargetBlend.BlendEnable = TRUE;
    renderTargetBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    renderTargetBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    renderTargetBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    renderTargetBlend.BlendOp = D3D12_BLEND_OP_ADD;
    renderTargetBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
    renderTargetBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
    renderTargetBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;

    D3D12_RASTERIZER_DESC rasterizerDesc = {}; // ライン描画用ラスタライザ設定
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.DepthClipEnable = TRUE;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {}; // 深度テスト設定
    depthStencilDesc.DepthEnable = enableDepth ? TRUE : FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/DebugLine.VS.hlsl", L"vs_6_0"); // コンパイル済み頂点シェーダ
    ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/DebugLine.PS.hlsl", L"ps_6_0"); // コンパイル済みピクセルシェーダ
    assert(vertexShaderBlob != nullptr);
    assert(pixelShaderBlob != nullptr);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {}; // PSO の作成設定
    pipelineDesc.pRootSignature = rootSignature_.Get();
    pipelineDesc.InputLayout = inputLayoutDesc;
    pipelineDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    pipelineDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    pipelineDesc.BlendState = blendDesc;
    pipelineDesc.RasterizerState = rasterizerDesc;
    pipelineDesc.DepthStencilState = depthStencilDesc;
    pipelineDesc.NumRenderTargets = kRenderTargetCount;
    pipelineDesc.RTVFormats[0] = dxCommon_->GetSwapChainFormat();
    pipelineDesc.DSVFormat = enableDepth ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_UNKNOWN;
    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    pipelineDesc.SampleDesc.Count = kSampleCount;
    pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineState)); // PSO 作成結果
    if (FAILED(hr)) {
        Logger::Error("DebugRenderer::CreateGraphicsPipeline failed\n");
        pipelineState.Reset();
        assert(false);
    }
}

/// <summary>
/// 必要な頂点数に合わせて頂点バッファを拡張する。
/// </summary>
void DebugRenderer::EnsureVertexCapacity(size_t vertexCount)
{
    if (!dxCommon_ || vertexCount <= vertexCapacity_) {
        return;
    }

    size_t newCapacity = vertexCapacity_ > kInitialVertexCapacity ? vertexCapacity_ : kInitialVertexCapacity; // 新しく確保する頂点容量
    while (newCapacity < vertexCount) {
        newCapacity *= 2;
    }

    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        if (vertexResources_[frameIndex] && mappedVertexData_[frameIndex]) {
            vertexResources_[frameIndex]->Unmap(0, nullptr);
        }
        mappedVertexData_[frameIndex] = nullptr;
        DeferReleaseResource(vertexResources_[frameIndex]);
        vertexResources_[frameIndex] = dxCommon_->CreateBufferResource(sizeof(VertexData) * newCapacity);
        HRESULT hr = vertexResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertexData_[frameIndex])); // 頂点バッファのマップ結果
        if (FAILED(hr)) {
            Logger::Warn("DebugRenderer::EnsureVertexCapacity vertex map failed\n");
            mappedVertexData_[frameIndex] = nullptr;
        }
    }

    vertexCapacity_ = newCapacity;
}

/// <summary>
/// ライン頂点を GPU に転送して描画する。
/// </summary>
void DebugRenderer::RenderLines(const std::vector<VertexData>& vertices, const Math::Matrix4x4& wvpMatrix, ID3D12PipelineState* pipelineState)
{
    if (vertices.empty() || !dxCommon_ || !pipelineState || !rootSignature_) {
        return;
    }

    const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex(); // 現在描画中のフレーム番号
    EnsureVertexCapacity(vertices.size());
    if (!mappedVertexData_[frameIndex] || !mappedTransformationData_[frameIndex]) {
        return;
    }

    std::memcpy(mappedVertexData_[frameIndex], vertices.data(), sizeof(VertexData) * vertices.size());
    mappedTransformationData_[frameIndex]->wvp = wvpMatrix;

    vertexBufferView_.BufferLocation = vertexResources_[frameIndex]->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList(); // デバッグライン描画を積むコマンドリスト
    if (!commandList) {
        return;
    }

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->SetGraphicsRootConstantBufferView(0, transformationResources_[frameIndex]->GetGPUVirtualAddress());
    commandList->DrawInstanced(static_cast<UINT>(vertices.size()), 1, 0, 0);
}

/// <summary>
/// 線分 1 本分の頂点ペアをコンテナに追加する。
/// </summary>
void DebugRenderer::AddLine(std::vector<VertexData>& vertices, const Math::Vector3& start, const Math::Vector3& end, const Math::Vector4& color)
{
    vertices.push_back({ start, color });
    vertices.push_back({ end, color });
}
