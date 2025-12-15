#include "Object3d.h"
#include "Object3dCommon.h" 
#include "DirectXCommon.h"
#include <fstream>
#include "Logger.h"
#include "StringUtility.h"
#include "mathUtility.h"
#include "TextureManager.h"
#include "Model.h"
#include "ModelCommon.h"
#include "ModelManager.h"

using namespace MyEngine;
using Microsoft::WRL::ComPtr;

void Object3d::Initialize(Object3dCommon* object3dCommon)
{
    // 引数で受け取ってメンバ変数に記録する
    this->object3dCommon_ = object3dCommon;

    // Transformの初期化
    transform_ = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    cameraTransform_ = { { 1.0f, 1.0f, 1.0f }, { 0.3f, 0.0f, 0.0f }, { 0.0f, 4.0f, -10.0f } };
    // マテリアル用リソース作成
    CreateMaterialResource();
    // テクスチャ割り当て
    AssignTexture();

    // 座標変換行列用リソース作成
    CreateTransformationMatrixResource();
    // 平行光源用リソース作成
    CreateDirectionalLightResource();

    // ModelCommon を生成して初期化
    modelCommon_ = new ModelCommon();
    modelCommon_->Initialize(object3dCommon->GetDxCommon());

    // Model を読み込んでセット
    ModelManager* mgr = ModelManager::GetInstance();
    // デフォルトでは plane.obj を読み込むが、後で SetModel(file) で差し替え可能
    model_ = mgr->LoadModel("resources", "plane.obj", modelCommon_);
}

// ファイル名を指定してモデルを読み込み、設定する
void Object3d::SetModel(const std::string& filePath)
{
    // resources ディレクトリを前提として ModelManager に読み込みを依頼
    ModelManager* mgr = ModelManager::GetInstance();
    Model* m = mgr->LoadModel("resources", filePath, modelCommon_);
    model_ = m; // 成功すればポインタが入る。失敗時は nullptr になる
}

Object3d::~Object3d()
{
    // ModelCommon を解放
    delete modelCommon_;
    modelCommon_ = nullptr;
}

Object3d::MaterialData Object3d::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
    // 1.中で必要となる変数の宣言
    MaterialData materialData; // 構築するMaterialData
    std::string line; // ファイルから読んだ1行を格納するもの

    // 2.ファイルを開く
    std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
    if (!file.is_open()) {
        char buf[256];
        sprintf_s(buf, "Warning: LoadMaterialTemplateFile failed to open %s/%s\n", directoryPath.c_str(), filename.c_str());
        Logger::Log(buf);
        return materialData;
    }

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
    // ライティング無効
    materialData_->enableLighting = 0;
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
    auto texMgr = TextureManager::GetInstance();
    if (!modelData_.material.textureFilePath.empty()) {
        uint32_t idx = texMgr->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
        // 成功したら格納して終了
        if (idx != UINT32_MAX) {
            modelData_.material.textureIndex = idx;
            char buf[256];
            sprintf_s(buf, "Object3d::AssignTexture: file=%s -> index=%u\n", modelData_.material.textureFilePath.c_str(), idx);
            Logger::Log(buf);
            return;
        }
    }

    // テクスチャ指定がない、もしくは見つからなかった場合の処理
    uint32_t loadedCount = texMgr->GetLoadedTextureCount();
    // デフォルトテクスチャを割り当てる
    if (loadedCount > 0) {
        modelData_.material.textureIndex = 0;
        Logger::Log("Object3d::AssignTexture: no material texture specified, defaulting to index=0\n");
    } else {
        modelData_.material.textureIndex = UINT32_MAX;
        Logger::Log("Object3d::AssignTexture: no textures loaded, leaving textureIndex invalid\n");
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
    // 各種ポインタのチェック
    if (!object3dCommon_) {
        Logger::Log("Object3d::Draw skipped: object3dCommon_ is null\n");
        return;
    }
    if (!object3dCommon_->GetDxCommon()) {
        Logger::Log("Object3d::Draw skipped: DxCommon is null\n");
        return;
    }
    // 描画に必要なコマンドを積む
    auto cmdList = object3dCommon_->GetDxCommon()->GetCommandList();
    if (!cmdList) {
        Logger::Log("Object3d::Draw skipped: command list is null\n");
        return;
    }
    // ログ：描画開始、状態確認
    {
        char buf[256];
        sprintf_s(buf, "Object3d::Draw start: model=%p materialRes=%p transformRes=%p lightRes=%p\n",
            reinterpret_cast<void*>(model_),
            reinterpret_cast<void*>(materialResource_.Get()),
            reinterpret_cast<void*>(transformationMatrixResource_.Get()),
            reinterpret_cast<void*>(directionalLightResource_.Get()));
        Logger::Log(buf);
    }

    // モデルがセットされていればモデル描画に任せる
    if (model_) {
        model_->Draw(this);
        return;
    }

    // モデルがセットされていない場合は頂点バッファから直接描画する
    if (vertexBufferView_.SizeInBytes == 0) {
        Logger::Log("Object3d::Draw skipped: no vertex buffer for non-model draw\n");
        return;
    }
    // VBVを設定
    cmdList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // マテリアルCBV (必須)
    if (!materialResource_) {
        Logger::Log("Object3d::Draw skipped: materialResource_ is null\n");
        return;
    }
    auto matAddr = materialResource_->GetGPUVirtualAddress();
    {
        char buf[128];
        sprintf_s(buf, "Object3d::Draw: material GPUAddr=0x%016llX\n", matAddr);
        Logger::Log(buf);
    }
    cmdList->SetGraphicsRootConstantBufferView(0, matAddr);

    // 座標変換行列CBV (必須)
    if (!transformationMatrixResource_) {
        Logger::Log("Object3d::Draw skipped: transformationMatrixResource_ is null\n");
        return;
    }
    auto wvpAddr = transformationMatrixResource_->GetGPUVirtualAddress();
    {
        char buf[128];
        sprintf_s(buf, "Object3d::Draw: WVP GPUAddr=0x%016llX\n", wvpAddr);
        Logger::Log(buf);
    }
    cmdList->SetGraphicsRootConstantBufferView(1, wvpAddr);

    // 光源CBV (必須)
    if (!directionalLightResource_) {
        Logger::Log("Object3d::Draw skipped: directionalLightResource_ is null\n");
        return;
    }
    auto lightAddr = directionalLightResource_->GetGPUVirtualAddress();
    {
        char buf[128];
        sprintf_s(buf, "Object3d::Draw: Light GPUAddr=0x%016llX\n", lightAddr);
        Logger::Log(buf);
    }
    cmdList->SetGraphicsRootConstantBufferView(3, lightAddr);

    // SRVはTextureManagerからハンドルを取り出してRootDescriptorTableにセット
    uint32_t texIndex = modelData_.material.textureIndex;
    auto texMgr = TextureManager::GetInstance();
    if (texIndex != UINT32_MAX && texIndex < texMgr->GetLoadedTextureCount()) {
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = texMgr->GetSrvHandleGPU(texIndex);
        // ログ出力
        {
            char buf[128];
            sprintf_s(buf, "Object3d::Draw: textureIndex=%u srv.ptr=0x%016llX\n", texIndex, srvHandle.ptr);
            Logger::Log(buf);
        }
        // SRV DescriptorTable
        cmdList->SetGraphicsRootDescriptorTable(2, srvHandle);
    } else {
        {
            char buf[128];
            sprintf_s(buf, "Object3d::Draw: no valid texture assigned (index=%u) - skipping SRV\n", texIndex);
            Logger::Log(buf);
        }
    }
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
    if (!file.is_open()) {
        char buf[256];
        sprintf_s(buf, "Warning: LoadObjFile failed to open %s/%s\n", directoryPath.c_str(), filename.c_str());
        Logger::Log(buf);
        return modelData;
    }

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
