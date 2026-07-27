#pragma once

#include "DirectXCommon.h"
#include "MathTypes.h"
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <vector>
#include <wrl.h>

namespace MyEngine {

class Object3d;
class Sprite;

/// <summary>
/// フレーム単位でデバッグ用の線分を蓄積し、3D/2D のデバッグ図形として描画するクラス。
/// </summary>
class DebugRenderer {
public:
    /// <summary>
    /// デストラクタ。
    /// </summary>
    ~DebugRenderer();

    /// <summary>
    /// デバッグライン描画に必要な GPU リソースを初期化する。
    /// </summary>
    void Initialize(DirectXCommon* dxCommon);

    /// <summary>
    /// フレーム内に蓄積したデバッグラインをクリアする。
    /// </summary>
    void BeginFrame();

    /// <summary>
    /// 3D 空間上の線分を追加する。
    /// </summary>
    void DrawLine3D(const Math::Vector3& start, const Math::Vector3& end, const Math::Vector4& color);

    /// <summary>
    /// 2D 画面座標上の線分を追加する。
    /// </summary>
    void DrawLine2D(const Math::Vector2& start, const Math::Vector2& end, const Math::Vector4& color);

    /// <summary>
    /// XZ 平面上にデバッググリッドを追加する。
    /// </summary>
    void DrawGrid(const Math::Vector3& center, int halfLineCount, float spacing, const Math::Vector4& color);

    /// <summary>
    /// AABB のワイヤーフレームを追加する。
    /// </summary>
    void DrawAABB(const Math::Vector3& min, const Math::Vector3& max, const Math::Vector4& color);

    /// <summary>
    /// 2D 矩形のワイヤーフレームを追加する。
    /// </summary>
    void DrawRect2D(const Math::Vector2& leftTop, const Math::Vector2& size, const Math::Vector4& color);

    /// <summary>
    /// Object3d の位置と回転に合わせたローカル軸を追加する。
    /// </summary>
    void DrawObjectAxis(const Object3d& object, float length);

    /// <summary>
    /// Sprite の表示矩形に合わせたワイヤーフレームを追加する。
    /// </summary>
    void DrawSpriteRect(const Sprite& sprite, const Math::Vector4& color);

    /// <summary>
    /// 蓄積した 3D ラインを描画する。
    /// </summary>
    void Render3D(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// 蓄積した 2D ラインを描画する。
    /// </summary>
    void Render2D();

private:
    struct VertexData {
        Math::Vector3 position; // 頂点の座標
        Math::Vector4 color; // 頂点の色
    };

    struct TransformationMatrix {
        Math::Matrix4x4 wvp; // ワールドビュー射影行列
    };

    /// <summary>
    /// デバッグライン描画用の RootSignature を作成する。
    /// </summary>
    void CreateRootSignature();

    /// <summary>
    /// デバッグライン描画用の PSO を作成する。
    /// </summary>
    void CreateGraphicsPipeline();
    void CreateGraphicsPipeline(bool enableDepth, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipelineState);

    /// <summary>
    /// GPUリソースを解放する。
    /// </summary>
    void Finalize();

    /// <summary>
    /// GPU参照が終わるまでD3D12リソースの解放を遅延する。
    /// </summary>
    void DeferReleaseResource(Microsoft::WRL::ComPtr<ID3D12Resource>& resource);

    /// <summary>
    /// 必要な頂点数に合わせて頂点バッファを拡張する。
    /// </summary>
    void EnsureVertexCapacity(size_t vertexCount);

    /// <summary>
    /// ライン頂点を GPU に転送して描画する。
    /// </summary>
    void RenderLines(const std::vector<VertexData>& vertices, const Math::Matrix4x4& wvpMatrix, ID3D12PipelineState* pipelineState);

    /// <summary>
    /// 線分 1 本分の頂点ペアをコンテナに追加する。
    /// </summary>
    void AddLine(std::vector<VertexData>& vertices, const Math::Vector3& start, const Math::Vector3& end, const Math::Vector4& color);

private:
    DirectXCommon* dxCommon_ = nullptr; // DirectX 共通管理への参照
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_; // デバッグライン描画用 RootSignature
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_; // 深度ありRTへ描画する3Dデバッグライン用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateNoDepth_; // 深度なしRTへ描画する2Dデバッグライン用PSO
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> vertexResources_; // フレームごとの頂点バッファ
    std::array<VertexData*, DirectXCommon::kFrameCount> mappedVertexData_ {}; // マップ済み頂点バッファの CPU アドレス
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> transformationResources_; // フレームごとの座標変換用定数バッファ
    std::array<TransformationMatrix*, DirectXCommon::kFrameCount> mappedTransformationData_ {}; // マップ済み座標変換バッファの CPU アドレス
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {}; // 描画時に設定する頂点バッファビュー
    size_t vertexCapacity_ = 0; // 確保済み頂点数
    std::vector<VertexData> lineVertices3D_; // 3D 描画用に蓄積したライン頂点
    std::vector<VertexData> lineVertices2D_; // 2D 描画用に蓄積したライン頂点
};

} // namespace MyEngine
