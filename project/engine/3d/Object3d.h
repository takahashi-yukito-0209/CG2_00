#pragma once
#include "Logger.h"
#include <MathTypes.h>
#include <array>
#include <cmath>
#include <d3d12.h>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
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
        int32_t useAlphaCutoutSampler; // 0以外の場合、アルファカットアウト用にpoint+clampサンプラーを使用
        int32_t useAlphaDiscard; // 0以外の場合、透過ピクセルをdiscardする
        float padding2[1];
        float shininess; // 反射の鋭さ（スペキュラー強度の指数）
        float environmentCoefficient; // 環境マップ反射強度
        float pad3[2];
    };

    // 座標変換行列データ
    struct TransformationMatrix {
        Math::Matrix4x4 WVP;
        Math::Matrix4x4 World;
        Math::Vector4 color; // インスタンスごとの色（wはアルファ）
        Math::Matrix4x4 WorldInverseTranspose;
    };

    // 平行光源データ構造体
    struct DirectionalLight {
        Math::Vector4 color; // ライトの色
        Math::Vector3 direction; // ライトの向き
        float intensity; // 輝度
    };

    // 点光源データ構造体
    struct PointLight {
        Math::Vector4 position;
        Math::Vector4 color;
        float radius;
        float decay;
        int32_t enabled;
        float padding;
    };

    // スポットライトデータ構造体
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

    // マテリアルデータの該当フィールドも更新
    struct MaterialData {
        std::string textureFilePath;
        uint32_t textureIndex = UINT32_MAX;
    };

    template <typename TValue>
    struct Keyframe {
        float time; // キーフレームの時刻
        TValue value; // キーフレームの値
    };

    using KeyframeVector3 = Keyframe<Math::Vector3>;
    using KeyframeQuaternion = Keyframe<Math::Quaternion>;

    template <typename TValue>
    struct AnimationCurve {
        std::vector<Keyframe<TValue>> keyframes; // 時刻順に並んだキーフレーム
    };

    struct NodeAnimation {
        AnimationCurve<Math::Vector3> translate; // 平行移動のアニメーション
        AnimationCurve<Math::Quaternion> rotate; // 回転のアニメーション
        AnimationCurve<Math::Vector3> scale; // スケールのアニメーション
    };

    struct Animation {
        float duration = 0.0f; // アニメーション全体の長さ
        std::unordered_map<std::string, NodeAnimation> nodeAnimations; // ノード名ごとのアニメーション
    };

    // モデルデータ構造体
    struct ModelData {
        std::vector<VertexData> vertices;
        MaterialData material;
        // ルートノード情報（Assimpのノードツリーを格納）
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
    /// マテリアルテンプレートファイルを読み込む
    /// </summary>
    static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

    /// <summary>
    /// モデルファイルを読み込み、頂点データとマテリアル情報を格納する
    /// </summary>
    static ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

    /// <summary>
    /// アニメーションファイルを読み込む
    /// </summary>
    static Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

    /// <summary>
    /// 任意時刻のVector3値を取得する
    /// </summary>
    static Math::Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);

    /// <summary>
    /// 任意時刻のQuaternion値を取得する
    /// </summary>
    static Math::Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

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
    /// 再生するアニメーションを設定する
    /// </summary>
    void SetAnimation(const Animation& animation);

    /// <summary>
    /// 指定ファイルからアニメーションを読み込んで設定する
    /// </summary>
    bool SetAnimation(const std::string& filePath);

    /// <summary>
    /// アニメーション再生の有効状態を設定する
    /// </summary>
    void SetAnimationEnabled(bool enabled) { animationEnabled_ = enabled; }

    /// <summary>
    /// アニメーション再生状態を更新する
    /// </summary>
    void UpdateAnimation(float deltaTime);

    /// <summary>
    /// モデルを使わず、直接指定した頂点データを設定する
    /// </summary>
    void SetMesh(const std::vector<VertexData>& vertices);

    /// <summary>
    /// 設定されているモデルを取得する
    /// </summary>
    Model* GetModel() const { return model_; }

    /// <summary>
    /// 頂点バッファビューを取得する
    /// </summary>
    D3D12_VERTEX_BUFFER_VIEW const& GetVertexBufferView() const { return vertexBufferView_; }

    /// <summary>
    /// マテリアル用リソースを取得する
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> const& GetMaterialResource() const;

    /// <summary>
    /// 座標変換行列用リソースを取得する
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

    // OBJファイルのデータ
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
    // 注: 平行光源は現在 Object3dCommon が所有する共有リソースを使用する
    // 頂点バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    // バッファ内のデータを指すポインタ
    VertexData* vertexData_ = nullptr;
    // 頂点バッファの使い方を表すビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};

    Math::Transform transform_; // オブジェクトの座標変換情報（スケール、回転、平行移動）
    Math::Transform cameraTransform_; // カメラの座標変換情報（スケール、回転、平行移動）

    Animation animation_; // 再生対象のアニメーション
    float animationTime_ = 0.0f; // 現在の再生時刻
    bool hasAnimation_ = false; // アニメーションを保持しているか
    bool animationEnabled_ = false; // アニメーションを再生するか
    Math::Matrix4x4 animationLocalMatrix_ {}; // アニメーションから作成したローカル行列

    // 設定されているモデルへのポインタ
    Model* model_ = nullptr;
    std::string debugName_ = "No Model"; // ImGuiで識別するための表示名
    // モデル用にこのObject3dが所有するModelCommon
    std::unique_ptr<ModelCommon> modelCommon_;

    // 参照するカメラ（未設定時はObject3dCommonのデフォルトカメラ）
    class Camera* camera_ = nullptr;

    // このオブジェクトのマテリアルがアルファカットアウト用サンプラー（point+clamp）を必要とするか
    bool useAlphaCutoutSampler_ = false;
    bool useAlphaDiscard_ = true;

public: // メンバ関数
    /// <summary>
    /// 大きさを設定する
    /// </summary>
    void SetScale(const Math::Vector3& scale) { transform_.scale = scale; }

    /// <summary>
    /// 回転を設定する
    /// </summary>
    void SetRotate(const Math::Vector3& rotate) { transform_.rotate = rotate; }

    /// <summary>
    /// 平行移動を設定する
    /// </summary>
    void SetTranslate(const Math::Vector3& translate)
    {
        // 入力値の妥当性を確認
        auto invalid = [](float v) {
            return !std::isfinite(v) || std::fabs(v) > 1e6f;
        };

        // 無限大、非数、または極端に大きい値は無視する
        if (invalid(translate.x) || invalid(translate.y) || invalid(translate.z)) {
            std::ostringstream oss;
            oss << "Warning: Rejecting invalid translate set = " << translate.x << " " << translate.y << " " << translate.z << "\n";
            Logger::Log(oss.str());
            return; // すべての成分が有効でない場合はtransform_.translateを更新せずに終了
        }

        // すべての成分が有効な場合のみ平行移動に代入する
        transform_.translate = translate;
    }

    /// <summary>
    /// ImGuiでオブジェクトの状態を表示・編集する
    /// </summary>
    void DrawImGui(int index);

    /// <summary>
    /// スケールを取得する
    /// </summary>
    const Math::Vector3 GetScale() const { return transform_.scale; }

    /// <summary>
    /// 回転を取得する
    /// </summary>
    const Math::Vector3 GetRotate() const { return transform_.rotate; }

    /// <summary>
    /// 平行移動を取得する
    /// </summary>
    const Math::Vector3 GetTranslate() const { return transform_.translate; }

    /// <summary>
    /// ライティングの有効・無効を取得する
    /// </summary>
    bool GetEnableLighting() const;

    /// <summary>
    /// ライティングの有効・無効を設定する
    /// </summary>
    void SetEnableLighting(bool enable);

    /// <summary>
    /// ライティングモードを取得する
    /// </summary>
    int GetLightingMode() const;

    /// <summary>
    /// ライティングモードを設定する
    /// </summary>
    void SetLightingMode(int mode);

    /// <summary>
    /// 環境マップ反射強度を設定する
    /// </summary>
    void SetEnvironmentCoefficient(float coefficient);

    /// <summary>
    /// 環境マップ反射強度を取得する
    /// </summary>
    float GetEnvironmentCoefficient() const;

    /// <summary>
    /// UV変換行列を設定する
    /// </summary>
    void SetUVTransform(const Math::Matrix4x4& uvTransform);

    /// <summary>
    /// アルファカットアウト用サンプラーの使用を設定する
    /// </summary>
    void SetUseAlphaCutoutSampler(bool use)
    {
        // 内部フラグを更新
        useAlphaCutoutSampler_ = use;
        if (materialData_) {
            // マテリアルデータの該当フィールドも更新
            materialData_->useAlphaCutoutSampler = use ? 1 : 0;
        }
    }

    /// <summary>
    /// アルファカットアウト用サンプラーの使用状態を取得する
    /// </summary>
    bool GetUseAlphaCutoutSampler() const { return useAlphaCutoutSampler_; }

    /// <summary>
    /// 透過ピクセルをdiscardするか設定する
    /// </summary>
    void SetUseAlphaDiscard(bool use)
    {
        useAlphaDiscard_ = use;
        if (materialData_) {
            materialData_->useAlphaDiscard = use ? 1 : 0;
        }
    }

    /// <summary>
    /// 透過ピクセルをdiscardするか取得する
    /// </summary>
    bool GetUseAlphaDiscard() const { return useAlphaDiscard_; }

private: // 内部関数
    // 蛻晄悄蛹冶｣懷勧
    /// <summary>
    /// Transformの初期値を設定する
    /// </summary>
    void InitializeTransformState();

    void CreateMaterialResource(); // マテリアル用定数バッファリソースの作成と初期化
    /// <summary>
    /// マテリアルの初期値をCPU側状態へ設定する
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
    /// Object3d側で明示指定されたテクスチャを割り当てる
    /// </summary>
    bool AssignExplicitTextureOverride();

    /// <summary>
    /// Model側のマテリアルテクスチャを使う状態に設定する
    /// </summary>
    void AssignLoadedModelMaterialTexture();

    /// <summary>
    /// 既定テクスチャをfallbackとして割り当てる
    /// </summary>
    bool AssignFallbackTexture();

    /// <summary>
    /// Object3d側の明示テクスチャ、Model側マテリアル、fallbackの順でテクスチャを割り当てる
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
    /// 現在のフレームで使用するマテリアル状態をGPUバッファへ転送する
    /// </summary>
    void UploadMaterialFrameResource(uint32_t frameIndex);

    /// <summary>
    /// 現在のフレームで使用する座標変換行列をGPUバッファへ転送する
    /// </summary>
    void UploadTransformationMatrixFrameResource(uint32_t frameIndex);
};

} // namespace MyEngine
