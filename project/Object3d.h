#pragma once
#include <MathTypes.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>

using namespace Math;

namespace MyEngine {

// 前方宣言
class Object3dCommon;

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

    struct DirectionalLight {
        Vector4 color; //!< ライトの色
        Vector3 direction; //!< ライトの向き
        float intensity; //!< 輝度
    };

    // マテリアルデータ構造体
    struct MaterialData {
        std::string textureFilePath;
        uint32_t textureIndex = 0;
    };

    // モデルデータ構造体
    struct ModelData {
        std::vector<VertexData> vertices;
        MaterialData material;
    };

public: // メンバ関数
    // 初期化
    void Initialize(Object3dCommon* object3dCommon);
    // 更新（WVP等の書き込み）
    void Update(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix);
    // 描画
    void Draw();

    //.mtlファイルを読み取り
    static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

    //.objファイルを読みこむ
    static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

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
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
    DirectionalLight* directionalLightData_ = nullptr;
    // 頂点バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    // バッファリソース内のデータを指すポインタ
    VertexData* vertexData_ = nullptr;
    // バッファリソースの使い道を補足するバッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};

    Transform transform_;
    Transform cameraTransform_;

private:
    // 初期化補助
    void CreateMaterialResource();
    void CreateTransformationMatrixResource();
    void CreateDirectionalLightResource();
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
