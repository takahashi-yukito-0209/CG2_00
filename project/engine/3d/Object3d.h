#pragma once
#include "Logger.h"
#include <MathTypes.h>
#include <array>
#include <cmath>
#include <d3d12.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <wrl.h>

#include "DirectXCommon.h"
#include "ModelCommon.h"

namespace MyEngine {

// 前方宣言
class Object3dCommon;
class Model;

/// <summary>
/// 3Dオブジェクトの変換、マテリアル、モデル参照を管理するクラス
/// </summary>
class Object3d {
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
        float padding[3];
        Math::Matrix4x4 uvTransform;
        int lightingMode;
        int32_t useAlphaCutoutSampler; // 0でない場合、アルファカットアウト用に point+clamp サンプラーを使用
        int32_t useAlphaDiscard; // 0でない場合、透明テクセルをdiscardする
        float padding2[1];
        float shininess; // 反射の鋭さ（スペキュラー強度の指数）
        float environmentCoefficient; // 環境マップ反射の強さ
        float pad3[2];
    };

    // 座標変換行列データ
    struct TransformationMatrix {
        Math::Matrix4x4 WVP;
        Math::Matrix4x4 World;
        Math::Vector4 color; // インスタンス毎のカラー（w = アルファ）
        Math::Matrix4x4 WorldInverseTranspose;
    };

    // 平行光源データ構造体
    struct DirectionalLight {
        Math::Vector4 color; //!< ライトの色
        Math::Vector3 direction; //!< ライトの向き
        float intensity; //!< 輝度
    };

    // 点光源データ構造体 (CPU側レイアウト)
    struct PointLight {
        Math::Vector4 position;
        Math::Vector4 color;
        float radius;
        float decay;
        int32_t enabled;
        float padding;
    };

    // スポットライトデータ構造体 (CPU側レイアウト)
    struct SpotLight {
        Math::Vector4 position;
        Math::Vector4 color;
        Math::Vector3 direction;
        float distance;
        float decay;
        float cosAngle;
        float cosFalloffStart;
        int32_t enabled;
        float padding;
    };

    // マテリアルデータ構造体
    struct MaterialData {
        std::string textureFilePath;
        uint32_t textureIndex = UINT32_MAX;
    };

    // モデルデータ構造体
    struct ModelData {
        std::vector<VertexData> vertices;
        MaterialData material;
        // ルートノード情報（Assimp のノードツリーを格納）
        struct Node {
            Math::Matrix4x4 localMatrix;
            std::string name;
            std::vector<Node> children;
        } rootNode;
    };

public: // メンバ関数
    /// <summary>
    /// 3Dオブジェクトの描画に必要な初期リソースを生成する
    /// </summary>
    void Initialize(Object3dCommon* object3dCommon, class ImGuiManager* imguiManager = nullptr);

    /// <summary>
    /// 3Dオブジェクトを終了する
    /// </summary>
    ~Object3d();

    /// <summary>
    /// 変換行列を更新し、描画用の状態を反映する
    /// </summary>
    void Update(const Math::Matrix4x4& viewMatrix, const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// 3Dオブジェクトを描画する
    /// </summary>
    void Draw();

    /// <summary>
    /// 同じメッシュを指定数だけインスタンシング描画する
    /// </summary>
    void DrawInstanced(uint32_t instanceCount);

    /// <summary>
    /// マテリアルテンプレートファイルを読みこむ（マテリアルの基本情報を格納した独自フォーマットのファイルを想定）
    /// </summary>
    static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

    /// <summary>
    /// モデルファイルを読みこむ（頂点データとマテリアル情報を格納した独自フォーマットのファイルを想定）
    /// </summary>
    static ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

    /// <summary>
    /// 既存のModelインスタンスを設定する
    /// </summary>
    void SetModel(Model* model)
    {
        model_ = model;
        debugName_ = model ? "External Model" : "No Model";
    }

    /// <summary>
    /// モデルファイル名を指定してModelManagerからモデルを取得する
    /// </summary>
    void SetModel(const std::string& filePath);

    /// <summary>
    /// このオブジェクトで使用するテクスチャを設定する
    /// </summary>
    void SetTexture(const std::string& filePath);

    /// <summary>
    /// モデルを使わず、直接指定した頂点データを設定する
    /// </summary>
    void SetMesh(const std::vector<VertexData>& vertices);

    /// <summary>
    /// 設定されているモデルを取得する
    /// </summary>
    Model* GetModel() const { return model_; }

    /// <summary>
    /// 頂点バッファビューの取得
    /// </summary>
    D3D12_VERTEX_BUFFER_VIEW const& GetVertexBufferView() const { return vertexBufferView_; }

    /// <summary>
    /// マテリアル用リソースの取得
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> const& GetMaterialResource() const;

    /// <summary>
    /// 座標変換行列用リソースの取得
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> const& GetTransformationMatrixResource() const;

    /// <summary>
    /// このオブジェクトが保持するモデル補助データを取得する
    /// </summary>
    const ModelData& GetModelData() const { return modelData_; }

    /// <summary>
    /// Object3dCommonへの参照を取得する
    /// </summary>
    Object3dCommon* GetObject3dCommon() const { return object3dCommon_; }

    /// <summary>
    /// ImGui表示用の名前を取得する
    /// </summary>
    const std::string& GetDebugName() const { return debugName_; }

private: // メンバ変数
    Object3dCommon* object3dCommon_ = nullptr; // 共通情報へのポインタ

    // Objファイルのデータ
    ModelData modelData_;

    // マテリアル用リソース
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> materialResources_;
    std::array<Material*, DirectXCommon::kFrameCount> mappedMaterialData_ {};
    // マテリアル用定数バッファリソース
    Material materialState_ {}; // CPU側で保持するマテリアル状態
    Material* materialData_ = &materialState_;
    // 座標変換行列用リソース
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> transformationMatrixResources_;
    std::array<TransformationMatrix*, DirectXCommon::kFrameCount> mappedTransformationMatrixData_ {};
    // 行列データ用定数バッファリソース
    TransformationMatrix transformationMatrixState_ {}; // CPU側で保持する変換行列
    TransformationMatrix* transformationMatrixData_ = &transformationMatrixState_;

    // 平行光源用リソース
    // 注: 平行光源は現在 `Object3dCommon` が所有する共有リソースとなっている
    // 頂点バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    // バッファリソース内のデータを指すポインタ
    VertexData* vertexData_ = nullptr;
    // バッファリソースの使い道を補足するバッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};

    Math::Transform transform_; // オブジェクトの座標変換情報（スケール、回転、平行移動）
    Math::Transform cameraTransform_; // カメラの座標変換情報（スケール、回転、平行移動）

    // 設定されているモデルへのポインタ
    Model* model_ = nullptr;
    std::string debugName_ = "No Model"; // ImGuiで識別するための表示名
    // モデル用にこの `Object3d` が所有する `ModelCommon`
    std::unique_ptr<ModelCommon> modelCommon_;

    // 参照するカメラ（既定は Object3dCommon のデフォルトカメラ）
    class Camera* camera_ = nullptr;

    // このオブジェクトのマテリアルがアルファカットアウト用サンプラー(point+clamp)を必要とするか
    bool useAlphaCutoutSampler_ = false;
    bool useAlphaDiscard_ = true;

public: // メンバ関数
    /// <summary>
    ///  大きさ設定
    /// </summary>
    void SetScale(const Math::Vector3& scale) { transform_.scale = scale; }

    /// <summary>
    /// 回転設定
    /// </summary>
    void SetRotate(const Math::Vector3& rotate) { transform_.rotate = rotate; }

    /// <summary>
    /// 平行移動設定
    /// </summary>
    void SetTranslate(const Math::Vector3& translate)
    {
        // 入力値の妥当性チェック
        auto invalid = [](float v) {
            return !std::isfinite(v) || std::fabs(v) > 1e6f;
        };

        // いずれかの成分が無限大、非数、または極端に大きい値の場合は警告を出して無視する
        if (invalid(translate.x) || invalid(translate.y) || invalid(translate.z)) {
            std::ostringstream oss;
            oss << "Warning: Rejecting invalid translate set = " << translate.x << " " << translate.y << " " << translate.z << "\n";
            Logger::Log(oss.str());
            return; // すべての成分が有効な値でない場合は transform_.translate を更新せずに終了
        }

        // すべての成分が有効な値の場合のみ transform_.translate に代入する
        transform_.translate = translate;
    }

    /// <summary>
    /// ImGuiでオブジェクトの状態を表示・編集する
    /// </summary>
    void DrawImGui(int index);

    /// <summary>
    /// スケール取得
    /// </summary>
    const Math::Vector3 GetScale() const { return transform_.scale; }

    /// <summary>
    /// 回転取得
    /// </summary>
    const Math::Vector3 GetRotate() const { return transform_.rotate; }

    /// <summary>
    /// 平行移動取得
    /// </summary>
    const Math::Vector3 GetTranslate() const { return transform_.translate; }

    /// <summary>
    /// ライティングの有効/無効を取得する
    /// </summary>
    bool GetEnableLighting() const;

    /// <summary>
    /// ライティングの有効/無効を設定する
    /// </summary>
    void SetEnableLighting(bool enable);

    /// <summary>
    /// ライティングモードの取得
    /// </summary>
    int GetLightingMode() const;

    /// <summary>
    /// ライティングモードの設定
    /// </summary>
    void SetLightingMode(int mode);

    /// <summary>
    /// 環境マップ反射の強さを設定する
    /// </summary>
    void SetEnvironmentCoefficient(float coefficient);

    /// <summary>
    /// 環境マップ反射の強さを取得する
    /// </summary>
    float GetEnvironmentCoefficient() const;

    /// <summary>
    /// UV変換行列を設定する
    /// </summary>
    void SetUVTransform(const Math::Matrix4x4& uvTransform);

    /// <summary>
    /// アルファカットアウト用サンプラーの使用設定
    /// </summary>
    void SetUseAlphaCutoutSampler(bool use)
    {
        // 内部フラグを更新
        useAlphaCutoutSampler_ = use;
        // マテリアルデータの該当フィールドも更新
        if (materialData_) {
            materialData_->useAlphaCutoutSampler = use ? 1 : 0;
        }
    }

    /// <summary>
    /// アルファカットアウト用サンプラーの使用設定の取得
    /// </summary>
    bool GetUseAlphaCutoutSampler() const { return useAlphaCutoutSampler_; }

    /// <summary>
    /// 透明テクセルをdiscardするか設定する
    /// </summary>
    void SetUseAlphaDiscard(bool use)
    {
        useAlphaDiscard_ = use;
        if (materialData_) {
            materialData_->useAlphaDiscard = use ? 1 : 0;
        }
    }

    /// <summary>
    /// 透明テクセルをdiscardするか取得する
    /// </summary>
    bool GetUseAlphaDiscard() const { return useAlphaDiscard_; }

private: // 内部関数
    // 初期化補助
    /// <summary>
    /// Transform の初期値を設定する。
    /// </summary>
    void InitializeTransformState();

    void CreateMaterialResource(); // マテリアル数バッファリソースの作成と初期化
    /// <summary>
    /// マテリアルの初期値をCPU側状態へ設定する。
    /// </summary>
    void InitializeMaterialState();
    void CreateTransformationMatrixResource(); // 定数バッファリソースの作成と初期化
    /// <summary>
    /// Object3d側で明示指定されたテクスチャがあるか確認する
    /// </summary>
    bool HasExplicitTextureOverride() const;

    /// <summary>
    /// 読み込み済みモデル側のマテリアルテクスチャを使用するか確認する
    /// </summary>
    bool UsesLoadedModelMaterialTexture() const;

    /// <summary>
    /// Object3d側で明示指定されたテクスチャを割り当てる。
    /// </summary>
    bool AssignExplicitTextureOverride();

    /// <summary>
    /// Model側のマテリアルテクスチャを使う状態に設定する。
    /// </summary>
    void AssignLoadedModelMaterialTexture();

    /// <summary>
    /// 既定テクスチャをfallbackとして割り当てる。
    /// </summary>
    bool AssignFallbackTexture();

    /// <summary>
    /// Object3d側の明示テクスチャ、Model側マテリアル、fallbackの順でテクスチャを割り当てる。
    /// </summary>
    void AssignTexture();

    /// <summary>
    /// テクスチャパスを解決し、未ロードならロードしてSRV番号を取得する
    /// </summary>
    uint32_t ResolveTextureIndex(const std::string& filePath, std::string* resolvedPath, bool releaseIntermediateAfterLoad) const;

    /// <summary>
    /// デフォルトテクスチャのSRV番号を取得する
    /// </summary>
    uint32_t ResolveFallbackTextureIndex() const;

    /// <summary>
    /// 非モデル描画で使用する頂点数を取得する
    /// </summary>
    uint32_t GetDrawVertexCount() const;

    /// <summary>
    /// マテリアルCBVを描画用ルートパラメータへ設定する
    /// </summary>
    bool BindMaterialResource(ID3D12GraphicsCommandList* commandList, const char* logContext) const;

    /// <summary>
    /// 座標変換行列CBVを描画用ルートパラメータへ設定する
    /// </summary>
    bool BindTransformationMatrixResource(ID3D12GraphicsCommandList* commandList, const char* logContext) const;

    /// <summary>
    /// 平行光源CBVを描画用ルートパラメータへ設定する
    /// </summary>
    bool BindDirectionalLightResource(ID3D12GraphicsCommandList* commandList, const char* logContext) const;

    /// <summary>
    /// カメラCBVを描画用ルートパラメータへ設定する
    /// </summary>
    void BindCameraResource(ID3D12GraphicsCommandList* commandList) const;

    /// <summary>
    /// 点光源CBVを描画用ルートパラメータへ設定する
    /// </summary>
    void BindPointLightResource(ID3D12GraphicsCommandList* commandList) const;

    /// <summary>
    /// 指定されたテクスチャ番号のSRVを描画用ルートパラメータへ設定する
    /// </summary>
    bool BindTexture(ID3D12GraphicsCommandList* commandList, uint32_t textureIndex, const char* logContext) const;

    /// <summary>
    /// インスタンシング用SRVを描画用ルートパラメータへ設定する
    /// </summary>
    void BindInstancingResource(ID3D12GraphicsCommandList* commandList) const;

    /// <summary>
    /// 非モデル通常描画で使用する共通リソースを設定する
    /// </summary>
    bool BindNonModelDrawResources(ID3D12GraphicsCommandList* commandList) const;

    /// <summary>
    /// 非モデルインスタンシング描画で使用する共通リソースを設定する
    /// </summary>
    bool BindNonModelInstancedDrawResources(ID3D12GraphicsCommandList* commandList) const;

    /// <summary>
    /// 現在のフレーム用GPUバッファへCPU側の状態を転送する
    /// </summary>
    void UpdateFrameResources(); // モデルデータ割り当て

    /// <summary>
    /// 現在のフレームで使用するマテリアル状態をGPUバッファへ転送する。
    /// </summary>
    void UploadMaterialFrameResource(uint32_t frameIndex);

    /// <summary>
    /// 現在のフレームで使用する座標変換行列をGPUバッファへ転送する。
    /// </summary>
    void UploadTransformationMatrixFrameResource(uint32_t frameIndex);
};

} // namespace MyEngine
