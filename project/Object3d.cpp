#include "Object3d.h"
#include "Object3dCommon.h" 
#include "DirectXCommon.h"
#include <fstream>
#include "Logger.h"
#include "StringUtility.h"
#include "engine/utility/mathUtility.h"
#include "engine/2d/TextureManager.h"

using namespace MyEngine;
using Microsoft::WRL::ComPtr;

void Object3d::Initialize(Object3dCommon* object3dCommon)
{
    // 引数で受け取ってメンバ変数に記録する
    this->object3dCommon_ = object3dCommon;

    // Transformの初期化
    transform_ = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    cameraTransform_ = { { 1.0f, 1.0f, 1.0f }, { 0.3f, 0.0f, 0.0f }, { 0.0f, 4.0f, -10.0f } };

    // モデル読み込み
    modelData_ = LoadObjFile("resources", "plane.obj");

    // VertexResourceの作成
    DirectXCommon* dxCommon = object3dCommon->GetDxCommon();
    // 頂点バッファを作成し、中身を書き込む
    if (!modelData_.vertices.empty()) {
        vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * modelData_.vertices.size());
        // マップしてデータを書き込む
        VertexData* mapped = nullptr;
        vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
        assert(mapped != nullptr);
        std::memcpy(mapped, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
        // vertexData_ はマップしたポインタを保持しておく
        vertexData_ = mapped;

        // 頂点バッファビューを設定
        vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
        vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * modelData_.vertices.size());
        vertexBufferView_.StrideInBytes = static_cast<UINT>(sizeof(VertexData));
    }

    // マテリアル用リソース作成
    CreateMaterialResource();
    // 座標変換行列用リソース作成
    CreateTransformationMatrixResource();
    // 平行光源用リソース作成
    CreateDirectionalLightResource();
    // テクスチャ割り当て
    AssignTexture();
}

Object3d::MaterialData Object3d::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
    // 1.中で必要となる変数の宣言
    MaterialData materialData; // 構築するMaterialData
    std::string line; // ファイルから読んだ1行を格納するもの

    // 2.ファイルを開く
    std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
    assert(file.is_open()); // とりあえず開けなかったら止める

    // 3.実際にファイルを読み、MaterialDataを構築していく
    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;

        // identifierに応じた処理
        if (identifier == "map_Kd") {
            std::string textureFilename;
            s >> textureFilename;
            // 連結してファイルパスにする
            materialData.textureFilePath = directoryPath + "/" + textureFilename;
        }
    }

    // 4.MaterialDataを返す
    return materialData;
}

void Object3d::CreateMaterialResource()
{
    // マテリアル用リソース作成
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();
    // マテリアル用バッファリソースを作成
    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
    // マップしてデータを書き込む
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    // デフォルト値
    materialData_->color = {1.0f,1.0f,1.0f,1.0f};
    materialData_->enableLighting = 1;
    // 初期UV変換は単位行列にする
    MathUtility math;
    materialData_->uvTransform = math.MakeIdentity4x4();
    // ライティングモードは通常のものにする
    materialData_->lightingMode = 2;
}

void Object3d::CreateTransformationMatrixResource()
{
    // 座標変換行列用リソース作成
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();
    // 変換行列用バッファリソースを作成
    transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
    // マップしてデータを書き込む
    transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
}

void Object3d::CreateDirectionalLightResource()
{
    // 平行光源用リソース作成
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();
    // 平行光源用バッファリソースを作成
    directionalLightResource_ = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
    // マップしてデータを書き込む
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
    // デフォルト値
    directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色光
    directionalLightData_->direction = { 0.0f, -1.0f, 0.0f }; // 真下に向ける
    directionalLightData_->intensity = 1.0f; // 輝度1.0f
}

void Object3d::AssignTexture()
{
    // テクスチャマネージャからインデックスを取得してモデルデータに格納
    if (!modelData_.material.textureFilePath.empty()) {
        uint32_t idx = TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
        modelData_.material.textureIndex = idx;
        Logger::Log(std::format("Object3d::AssignTexture: file={} -> index={}\n", modelData_.material.textureFilePath, idx));
    }
}

void Object3d::Update(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix)
{
    // WVP行列計算
    MathUtility math;
    // ワールド行列
    Matrix4x4 world = math.MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    // ワールドビュー射影行列
    Matrix4x4 wvp = math.Multiply(world, math.Multiply(viewMatrix, projectionMatrix));
    // 定数バッファに転送
    if (transformationMatrixData_) {
        transformationMatrixData_->World = world;
        transformationMatrixData_->WVP = wvp;
    }
}

void Object3d::Draw()
{
    // 描画に必要なコマンドを積む
    auto cmdList = object3dCommon_->GetDxCommon()->GetCommandList();
    // VBVを設定
    cmdList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    // マテリアルCBV
    cmdList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    // WVP CBV
    cmdList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
    // 光源CBV
    cmdList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());
    // SRVはTextureManagerからハンドルを取り出してRootDescriptorTableにセット
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(modelData_.material.textureIndex);
    // ログ出力
    Logger::Log(std::format("Object3d::Draw: textureIndex={} srv.ptr=0x{:016X}\n", modelData_.material.textureIndex, srvHandle.ptr));
    // SRV DescriptorTable
    cmdList->SetGraphicsRootDescriptorTable(2, srvHandle);
    // 描画コマンド
    cmdList->DrawInstanced(static_cast<UINT>(modelData_.vertices.size()), 1, 0, 0);
}
 
Object3d::ModelData Object3d::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
    ModelData modelData; // 構築するModelData
    std::vector<Vector4> positions; // 位置
    std::vector<Vector3> normals; // 法線
    std::vector<Vector2> texcoords; // テクスチャ座標
    std::string line; // ファイルから読んだ1行を格納するもの

    // 2.ファイルを開く
    std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
    assert(file.is_open()); // とりあえず開けなかったら止める

    // 3.実際にファイルを読み、ModelDataを構築していく
    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier; // 先頭の識別子を読む

        if (identifier == "v") {
            Vector4 position;
            s >> position.x >> position.y >> position.z;
            position.w = 1.0f;
            position.x *= -1.0f;
            positions.push_back(position);

        } else if (identifier == "vt") {
            Vector2 texcoord;
            s >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y;
            texcoords.push_back(texcoord);

        } else if (identifier == "vn") {
            Vector3 normal;
            s >> normal.x >> normal.y >> normal.z;
            normal.x *= -1.0f;
            normals.push_back(normal);

        } else if (identifier == "f") {
            VertexData triangle[3];
            // 面は三角形限定。その他は未対応
            for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
                std::string vertexDefinition;
                s >> vertexDefinition;
                // 頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
                std::stringstream v(vertexDefinition);
                uint32_t elementIndices[3];
                for (int32_t element = 0; element < 3; ++element) {
                    std::string index;
                    std::getline(v, index, '/'); // 区切りでインデックスを読んでいく
                    elementIndices[element] = std::stoi(index);
                }
                // 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
                Vector4 position = positions[elementIndices[0] - 1];
                Vector2 texcoord = texcoords[elementIndices[1] - 1];
                Vector3 normal = normals[elementIndices[2] - 1];
                triangle[faceVertex] = { position, texcoord, normal };
            }

            // 頂点を逆順で登録することで、回り順を逆にする
            modelData.vertices.push_back(triangle[2]);
            modelData.vertices.push_back(triangle[1]);
            modelData.vertices.push_back(triangle[0]);

        } else if (identifier == "mtllib") {
            // materialTemplateLibraryファイルの名前を取得する
            std::string materialFilename;
            s >> materialFilename;
            // 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
            modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
        }
    }

    return modelData;
}
