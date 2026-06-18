#pragma once
#include <MathTypes.h>
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <wrl.h>

using namespace Math;

namespace MyEngine {

// 前方宣言
class SpriteCommon;

/// <summary>
/// スプライト描画に必要な共通設定を管理するクラス
/// </summary>
class Sprite {

public: // メンバ構造体
    // 頂点データ構造体
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    // マテリアル構造体
    struct Material {
        Vector4 color;
        int32_t enableLighting;
        float padding1[3];
        Matrix4x4 uvTransform;
        int lightingMode;
        int32_t useAlphaCutoutSampler;
        float padding2[2];
        float shininess;
        float pad3[3];
    };

    // 座標変換行列データ
    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };

public: // メンバ関数
    /// <summary>
    /// 初期化
    /// </summary>
    // Single Initialize: optional ImGuiManager parameter (default nullptr)
    void Initialize(SpriteCommon* spriteCommon, std::string textureFilePath, class ImGuiManager* imguiManager = nullptr);

    /// <summary>
    /// 更新処理
    /// </summary>
    void Update();

    /// <summary>
    /// 頂点バッファビュー、インデックスバッファビュー、マテリアル定数バッファ、変換行列定数バッファ、SRVをコマンドリストにセットして描画
    /// </summary>
    void Draw();

    /// <summary>
    /// 座標取得
    /// </summary>
    const Vector2& GetPosition() const { return position_; }

    /// <summary>
    /// 回転取得
    /// </summary>
    float GetRotation() const { return rotation_; }

    /// <summary>
    /// 色取得
    /// </summary>
    const Vector4& GetColor() const { return materialData_->color; }

    /// <summary>
    /// 大きさ取得
    /// </summary>
    const Vector2& GetSize() const { return size_; }

    /// <summary>
    /// アンカーポイント取得
    /// </summary>
    const Vector2& GetAnchorPoint() const { return anchorPoint_; }

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
    const Vector2& GetTextureLeftTop() const { return textureLeftTop_; }

    /// <summary>
    /// テクスチャサイズ取得
    /// </summary>
    const Vector2& GetTextureSize() const { return textureSize_; }

    /// <summary>
    /// 座標設定
    /// </summary>
    void SetPosition(const Vector2& position) { position_ = position; }

    /// <summary>
    /// 回転設定
    /// </summary>
    void SetRotation(float rotation) { rotation_ = rotation; }

    /// <summary>
    /// 色設定
    /// </summary>
    void SetColor(const Vector4& color) { materialData_->color = color; }

    /// <summary>
    /// 大きさ設定
    /// </summary>
    void SetSize(const Vector2& size) { size_ = size; }

    /// <summary>
    /// アンカーポイント設定
    /// </summary>
    void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }

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
    void SetTextureLeftTop(const Vector2& textureLeftTop) { textureLeftTop_ = textureLeftTop; }

    /// <summary>
    /// テクスチャサイズ設定
    /// </summary>
    void SetTextureSize(const Vector2& textureSize) { textureSize_ = textureSize; }

    /// <summary>
    /// ロード済みテクスチャをスプライトへ設定する
    /// </summary>
    void SetTexture(const std::string& filePath);

    /// <summary>
    /// ImGui を使ってこのスプライトのプロパティを編集する関数
    /// </summary>
    void DrawImGui();

    // ImGui registration moved to central manager; no per-sprite registration fields
    ~Sprite();

private: // メンバ関数
    /// <summary>
    /// テクスチャサイズをイメージに合わせてスプライトのサイズも調整する
    /// </summary>
    void AdjustTextureSize();

private: // メンバ変数
    SpriteCommon* spriteCommon_ = nullptr; // スプライト描画の共通設定を管理するクラスへの参照

    // 頂点バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;
    // インデックスバッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_ = nullptr;
    // マテリアル用の定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_ = nullptr;
    // 行列用の定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_ = nullptr;

    // 定数バッファリソース
    // これらは GPU 上のリソースで、描画に必要な頂点データやインデックスデータ、マテリアル情報、変換行列を格納する。
    VertexData* vertexData_ = nullptr;
    uint32_t* indexData_ = nullptr;
    Material* materialData_ = nullptr;
    TransformationMatrix* transformationMatrixData_ = nullptr;

    // 頂点バッファをコマンドリストにバインドするためのビュー情報
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};
    // インデックスバッファをコマンドリストにバインドするためのビュー情報
    D3D12_INDEX_BUFFER_VIEW indexBufferView_ = {};

    // SRV ディスクリプタヒープ内のテクスチャスロット番号。
    uint32_t textureIndex_ = 0;

    // スプライトの変換情報
    Transform transform_ = {};
    // UVテクスチャ変換情報
    Transform uvTransform_ = {};

    // スプライトの基準座標（単位はワールド/スクリーンに依存）。
    Vector2 position_ = { 0.0f, 0.0f };
    // 回転角（ラジアンか度かの規約をここで統一しておくこと。通常ラジアンを想定）。
    float rotation_ = 0.0f;
    // スプライトの幅/高さ（テクスチャ比率に基づくスケール）。
    Vector2 size_ = { 1.0f, 1.0f };
    // アンカー（原点）を [0,0] 左上、[1,1] 右下 とする正規化座標。
    Vector2 anchorPoint_ = { 0.0f, 0.0f };
    //  テクスチャの左右反転フラグ。
    bool isFlipX_ = false;
    // テクスチャの上下反転フラグ。
    bool isFlipY_ = false;
    // テクスチャ上の切り出し左上座標（ピクセル単位）。
    Vector2 textureLeftTop_ = { 0.0f, 0.0f };
    // テクスチャの切り出しサイズ（ピクセル単位）。
    Vector2 textureSize_ = { 100.0f, 100.0f };
};

} // namespace MyEngine