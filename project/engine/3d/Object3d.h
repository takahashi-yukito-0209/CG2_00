#pragma once
#include <MathTypes.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>
#include <memory>
#include <cmath>
#include <sstream>
#include "Logger.h"

using namespace Math;

namespace MyEngine {

// 前方宣言
class Object3dCommon;
class Model; 
class ModelCommon; 

class Object3d {
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
        float padding[3];
        Matrix4x4 uvTransform;
        int lightingMode;
        float padding2[3];
    };

    // 座標変換行列データ
    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };

    // 平行光源データ構造体
    struct DirectionalLight {
        Vector4 color; //!< ライトの色
        Vector3 direction; //!< ライトの向き
        float intensity; //!< 輝度
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
    };

public: // メンバ関数
    // 初期化
    void Initialize(Object3dCommon* object3dCommon);
    // 終了
    ~Object3d();
    // 更新
    void Update(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix);
    // 描画
    void Draw();

    //.mtlファイルを読み取り
    static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

    //.objファイルを読みこむ
    static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

    // モデル用ポインタのセット・ゲット
    void SetModel(Model* model) { model_ = model; }
    // ファイル名を指定してモデルを設定する（resourcesフォルダを想定）
    void SetModel(const std::string& filePath);
    // テクスチャファイルを指定してオブジェクトに割り当てる
    void SetTexture(const std::string& filePath);
    Model* GetModel() const { return model_; }

    // モデル共通情報用ポインタのセット・ゲット
    D3D12_VERTEX_BUFFER_VIEW const& GetVertexBufferView() const { return vertexBufferView_; }
    Microsoft::WRL::ComPtr<ID3D12Resource> const& GetMaterialResource() const { return materialResource_; }
    Microsoft::WRL::ComPtr<ID3D12Resource> const& GetTransformationMatrixResource() const { return transformationMatrixResource_; }
    const ModelData& GetModelData() const { return modelData_; }

    // Access to owning Object3dCommon
    Object3dCommon* GetObject3dCommon() const { return object3dCommon_; }

private: // メンバ変数
    Object3dCommon* object3dCommon_ = nullptr;
    // Objファイルのデータ
    ModelData modelData_;
    // マテリアル用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
    // 座標変換行列用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    TransformationMatrix* transformationMatrixData_ = nullptr;
    // 平行光源用リソース
    // Note: directional light is now owned by Object3dCommon (shared global light)
    // 頂点バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    // バッファリソース内のデータを指すポインタ
    VertexData* vertexData_ = nullptr;
    // バッファリソースの使い道を補足するバッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};

    Transform transform_;
    Transform cameraTransform_;

    // Model pointer
    Model* model_ = nullptr;
    // ModelCommon owned by this Object3d for the model
    ModelCommon* modelCommon_ = nullptr;

public:
    // setters
    void SetScale(const Vector3& scale) { transform_.scale = scale; }
    void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
    void SetTranslate(const Vector3& translate) {
        // 入力値の妥当性チェック
        auto invalid = [](float v) {
            return !std::isfinite(v) || std::fabs(v) > 1e6f;
        };
        
        if (invalid(translate.x) || invalid(translate.y) || invalid(translate.z)) {
            std::ostringstream oss;
            oss << "Warning: Rejecting invalid translate set = " << translate.x << " " << translate.y << " " << translate.z << "\n";
            Logger::Log(oss.str());
            return; // ignore invalid assignment
        }
        transform_.translate = translate;
    }

    // Getter
    const Vector3 GetScale() const { return transform_.scale; }
    const Vector3 GetRotate() const { return transform_.rotate; }
    const Vector3 GetTranslate() const { return transform_.translate; }

private:
    // 初期化補助
    void CreateMaterialResource();
    void CreateTransformationMatrixResource();
    void AssignTexture();
};

} // namespace MyEngine

// Backwards-compatible type aliases (keep names used by legacy code)
namespace MyEngine {
    using VertexData = Object3d::VertexData;
    using Material = Object3d::Material;
    using TransformationMatrix = Object3d::TransformationMatrix;
    using DirectionalLight = Object3d::DirectionalLight;
    using MaterialData = Object3d::MaterialData;
    using ModelData = Object3d::ModelData;
}

// Also provide unqualified aliases for legacy code that expects these names in global scope
using VertexData = MyEngine::Object3d::VertexData;
using Material = MyEngine::Object3d::Material;
using TransformationMatrix = MyEngine::Object3d::TransformationMatrix;
using DirectionalLight = MyEngine::Object3d::DirectionalLight;
using MaterialData = MyEngine::Object3d::MaterialData;
using ModelData = MyEngine::Object3d::ModelData;
