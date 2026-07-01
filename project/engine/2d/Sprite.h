#pragma once
#include <MathTypes.h>
#include <array>
#include "DirectXCommon.h"
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <wrl.h>


namespace MyEngine {

// 前方宣言
class SpriteCommon;

/// <summary>
/// 2Dスプライトの描画リソースと表示状態を管理するクラス
/// </summary>
class Sprite {

public: // メンバ構造体
    // 頂点データ構造体
    struct VertexData {
        Math::Vector4 position;
        Math::Vector2 texcoord;
        Math::Vector3 normal;
    };

    // マテリアル構造体
    struct Material {
        Math::Vector4 color;
        int32_t enableLighting;
        float padding1[3];
        Math::Matrix4x4 uvTransform;
        int lightingMode;
        int32_t useAlphaCutoutSampler;
        float padding2[2];
        float shininess;
        float pad3[3];
    };

    // 座標変換行列データ
    struct TransformationMatrix {
        Math::Matrix4x4 WVP;
        Math::Matrix4x4 World;
    };

public: // メンバ関数
    /// <summary>
    /// スプライト描画に必要なリソースを生成し、初期テクスチャを設定する
    /// </summary>
    void Initialize(SpriteCommon* spriteCommon, std::string textureFilePath, class ImGuiManager* imguiManager = nullptr);

    /// <summary>
    /// スプライトの頂点、UV、変換行列を更新する
    /// </summary>
    void Update();

    /// <summary>
    /// スプライトを描画する
    /// </summary>
    void Draw();

    /// <summary>
    /// 座標取得
    /// </summary>
    const Math::Vector2& GetPosition() const { return position_; }

    /// <summary>
    /// 回転取得
    /// </summary>
    float GetRotation() const { return rotation_; }

    /// <summary>
    /// 色取得
    /// </summary>
    const Math::Vector4& GetColor() const { return materialData_->color; }

    /// <summary>
    /// 大きさ取得
    /// </summary>
    const Math::Vector2& GetSize() const { return size_; }

    /// <summary>
    /// アンカーポイント取得
    /// </summary>
    const Math::Vector2& GetAnchorPoint() const { return anchorPoint_; }

    /// <summary>
    /// フリップ状態取得(X座標)
    /// </summary>
    const bool& GetIsFlipX() const { return isFlipX_; }

    /// <summary>
    /// フリップ状態取得(Y座標)
    /// </summary>
    const bool& GetIsFlipY() const { return isFlipY_; }

    /// <summary>
    /// テクスチャ左上座標取得
    /// </summary>
    const Math::Vector2& GetTextureLeftTop() const { return textureLeftTop_; }

    /// <summary>
    /// テクスチャサイズ取得
    /// </summary>
    const Math::Vector2& GetTextureSize() const { return textureSize_; }

    /// <summary>
    /// 座標設定
    /// </summary>
    void SetPosition(const Math::Vector2& position) { position_ = position; }

    /// <summary>
    /// 回転設定
    /// </summary>
    void SetRotation(float rotation) { rotation_ = rotation; }

    /// <summary>
    /// 色設定
    /// </summary>
    void SetColor(const Math::Vector4& color) { materialData_->color = color; }

    /// <summary>
    /// 大きさ設定
    /// </summary>
    void SetSize(const Math::Vector2& size) { size_ = size; }

    /// <summary>
    /// アンカーポイント設定
    /// </summary>
    void SetAnchorPoint(const Math::Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }

    /// <summary>
    /// フリップ状態設定(X座標)
    /// </summary>
    void SetIsFlipX(bool isFlipX) { isFlipX_ = isFlipX; }

    /// <summary>
    /// フリップ状態設定(Y座標)
    /// </summary>
    void SetIsFlipY(bool isFlipY) { isFlipY_ = isFlipY; }

    /// <summary>
    /// テクスチャ左上座標設定
    /// </summary>
    void SetTextureLeftTop(const Math::Vector2& textureLeftTop) { textureLeftTop_ = textureLeftTop; }

    /// <summary>
    /// テクスチャサイズ設定
    /// </summary>
    void SetTextureSize(const Math::Vector2& textureSize) { textureSize_ = textureSize; }

    /// <summary>
    /// ロード済みテクスチャをスプライトへ設定する
    /// </summary>
    void SetTexture(const std::string& filePath);

    /// <summary>
    /// ImGui表示用のテクスチャ名を取得する
    /// </summary>
    const std::string& GetTextureFilePath() const { return textureFilePath_; }

    /// <summary>
    /// ImGuiでスプライトの状態を表示・編集する
    /// </summary>
    void DrawImGui();

    ~Sprite();

private: // メンバ関数
    /// <summary>
    /// テクスチャのサイズに合わせてスプライトの表示サイズを調整する
    /// </summary>
    void AdjustTextureSize();

    /// <summary>
    /// 現在フレーム用GPUバッファへCPU側の状態を転送する
    /// </summary>
    void UpdateFrameResources();

private: // メンバ変数
    SpriteCommon* spriteCommon_ = nullptr; // スプライト描画の共通設定を管理するクラスへの参照

    // 頂点バッファリソース
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> vertexResources_;
    std::array<VertexData*, DirectXCommon::kFrameCount> mappedVertexData_ {};
    // インデックスバッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_ = nullptr;
    // マテリアル用の定数バッファ
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> materialResources_;
    std::array<Material*, DirectXCommon::kFrameCount> mappedMaterialData_ {};
    // 行列用の定数バッファ
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> transformationMatrixResources_;
    std::array<TransformationMatrix*, DirectXCommon::kFrameCount> mappedTransformationMatrixData_ {};

    // 定数バッファリソース
    // これらは GPU 上のリソースで、描画に必要な頂点データやインデックスデータ、マテリアル情報、変換行列を格納する。
    std::array<VertexData, 4> vertexState_ {}; // CPU側で保持する頂点状態
    VertexData* vertexData_ = vertexState_.data();
    uint32_t* indexData_ = nullptr;
    Material materialState_ {}; // CPU側で保持するマテリアル状態
    Material* materialData_ = &materialState_;
    TransformationMatrix transformationMatrixState_ {}; // CPU側で保持する変換行列
    TransformationMatrix* transformationMatrixData_ = &transformationMatrixState_;

    // 頂点バッファをコマンドリストにバインドするためのビュー情報
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};
    // インデックスバッファをコマンドリストにバインドするためのビュー情報
    D3D12_INDEX_BUFFER_VIEW indexBufferView_ = {};

    // SRV ディスクリプタヒープ内のテクスチャスロット番号。
    uint32_t textureIndex_ = 0; // 使用するテクスチャのSRVインデックス
    std::string textureFilePath_; // ImGuiで識別するためのテクスチャ名

    // スプライトの変換情報
    Math::Transform transform_ = {};
    // UVテクスチャ変換情報
    Math::Transform uvTransform_ = {};

    // スプライトの基準座標（単位はワールド/スクリーンに依存）。
    Math::Vector2 position_ = { 0.0f, 0.0f };
    // 回転角（ラジアンか度かの規約をここで統一しておくこと。通常ラジアンを想定）。
    float rotation_ = 0.0f;
    // スプライトの幅/高さ（テクスチャ比率に基づくスケール）。
    Math::Vector2 size_ = { 1.0f, 1.0f };
    // アンカー（原点）を [0,0] 左上、[1,1] 右下 とする正規化座標。
    Math::Vector2 anchorPoint_ = { 0.0f, 0.0f };
    //  テクスチャの左右反転フラグ。
    bool isFlipX_ = false;
    // テクスチャの上下反転フラグ。
    bool isFlipY_ = false;
    // テクスチャ上の切り出し左上座標（ピクセル単位）。
    Math::Vector2 textureLeftTop_ = { 0.0f, 0.0f };
    // テクスチャの切り出しサイズ（ピクセル単位）。
    Math::Vector2 textureSize_ = { 100.0f, 100.0f };
};

} // namespace MyEngine
