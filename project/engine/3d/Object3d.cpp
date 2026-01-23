#include "Object3d.h"
#include "Object3dCommon.h" 
#include "DirectXCommon.h"
#include "externals/imgui/imgui.h"
#include <fstream>
#include "Logger.h"
#include "StringUtility.h"
#include "mathUtility.h"
#include "TextureManager.h"
#include "Model.h"
#include "ModelCommon.h"
#include "ModelManager.h"
#include <filesystem>
#include <algorithm>
#include "Camera.h"

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

    // 座標変換行列用リソース作成
    CreateTransformationMatrixResource();
    // 平行光源は Object3dCommon に統合されたため個別の作成は不要

    // ModelCommon を生成して初期化
    modelCommon_ = new ModelCommon();
    modelCommon_->Initialize(object3dCommon->GetDxCommon());

    // Model を読み込んでセット
    ModelManager* mgr = ModelManager::GetInstance();
    // デフォルトでは plane.obj を読み込むが、後で SetModel(file) で差し替え可能
    model_ = mgr->LoadModel("resources", "plane.obj", modelCommon_);
    // モデル読み込み後にテクスチャ割り当てを行う（MTLが先に読み込まれるように）
    AssignTexture();

    // 既定のカメラを参照
    camera_ = object3dCommon->GetDefaultCamera();
}


void Object3d::DrawImGui(int index)
{
    // Transform editing
    ImGui::Text("Object %d", index);
    ImGui::DragFloat3("Scale", &transform_.scale.x, 0.01f, 0.001f, 1000.0f);
    ImGui::DragFloat3("Rotate", &transform_.rotate.x, 0.01f);
    ImGui::DragFloat3("Translate", &transform_.translate.x, 0.01f);

    // Material controls (if material data exists)
    if (materialData_) {
        ImGui::Checkbox("Use Alpha Cutout Sampler", &useAlphaCutoutSampler_);
        materialData_->useAlphaCutoutSampler = useAlphaCutoutSampler_ ? 1 : 0;
    }
}

// ファイル名を指定してテクスチャを設定する
void Object3d::SetTexture(const std::string& filePath)
{
    // テクスチャをロードしてインデックスを取得
    auto texMgr = TextureManager::GetInstance();
    // 既にロード済みでなければロードを依頼
    uint32_t idx = texMgr->GetTextureIndexByFilePath(filePath);
    if (idx == UINT32_MAX) {
        texMgr->LoadTexture(filePath);
        // 転送を実行して SRV を作成する
        texMgr->ExecuteResourceUpload();
        idx = texMgr->GetTextureIndexByFilePath(filePath);
    }

    // 設定
    modelData_.material.textureFilePath = filePath;
    modelData_.material.textureIndex = (idx == UINT32_MAX) ? UINT32_MAX : idx;

    // Model が既に持つマテリアル側も更新しておく
    if (model_) {
        // Model::Initialize がセットする textureIndex_ は読み込み時のみ反映されるため
        // ここではモデルが保持する textureIndex_ を直接更新するためのアクセサはない。
        // 代わりに、Model 側の描画時に Object3d の modelData を参照するので十分。
    }
}

// ファイル名を指定してモデルを読み込み、設定する
void Object3d::SetModel(const std::string& filePath)
{
    // resources ディレクトリを前提として ModelManager に読み込みを依頼
    ModelManager* mgr = ModelManager::GetInstance();
    Model* m = mgr->LoadModel("resources", filePath, modelCommon_);
    model_ = m; // 成功すればポインタが入る。失敗時は nullptr になる
    // モデル読み込み後にテクスチャの割り当てを行う
    AssignTexture();
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
            // ログ出力: mtlで指定されたテクスチャパス
            {
                char buf[256];
                sprintf_s(buf, "LoadMaterialTemplateFile: map_Kd -> %s\n", materialData.textureFilePath.c_str());
                Logger::Log(buf);
            }
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
    // ライティングを有効化(3Dオブジェクトはライティング対象)
    materialData_->enableLighting = 1;
    // 初期UV変換は単位行列にする
    MathUtility math;
    materialData_->uvTransform = math.MakeIdentity4x4();
    // ライティングモードは通常のものにする
    materialData_->lightingMode = 2;
    // 既定: アルファカットアウト用サンプラーを強制しない
    // フェンスモデルは初期化後にこれを true に設定する場合がある
    useAlphaCutoutSampler_ = false;
    // GPU可視フラグを0で初期化
    materialData_->useAlphaCutoutSampler = 0;
    // 光沢（shininess）の既定値
    materialData_->shininess = 32.0f;
}

bool Object3d::GetEnableLighting() const { return materialData_ ? materialData_->enableLighting != 0 : false; }
void Object3d::SetEnableLighting(bool enable) { if (materialData_) materialData_->enableLighting = enable ? 1 : 0; }
int Object3d::GetLightingMode() const { return materialData_ ? materialData_->lightingMode : 0; }
void Object3d::SetLightingMode(int mode) { if (materialData_) materialData_->lightingMode = mode; }

void Object3d::CreateTransformationMatrixResource()
{
    // 座標変換行列用リソース作成
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();
    // 変換行列用バッファリソースを作成
    transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
    // マップしてデータを書き込む
    transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
}

    // 平行光源は現在 Object3dCommon が扱う（共有リソース）

void Object3d::AssignTexture()
{
    // テクスチャマネージャからインデックスを取得してモデルデータに格納
    auto texMgr = TextureManager::GetInstance();
    if (!modelData_.material.textureFilePath.empty()) {
        uint32_t idx = texMgr->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
        if (idx == UINT32_MAX) {
            // 未ロードならロードしてSRVインデックスを取得
            texMgr->LoadTexture(modelData_.material.textureFilePath);
            texMgr->ExecuteResourceUpload();
            idx = texMgr->GetSrvIndex(modelData_.material.textureFilePath);
        }
        modelData_.material.textureIndex = idx;
        char buf[256];
        sprintf_s(buf, "Object3d::AssignTexture: file=%s -> srvIndex=%u\n", modelData_.material.textureFilePath.c_str(), idx);
        Logger::Log(buf);
        return;
    }

    // パス自体が無い場合のみデフォルトを割り当てる
    uint32_t loadedCount = texMgr->GetLoadedTextureCount();
    if (loadedCount > 0) {
        // 既定のチェッカーテクスチャへフォールバックし、そのSRV絶対インデックスを使用
        uint32_t srvIdx = texMgr->GetSrvIndex("resources/uvChecker.png");
        if (srvIdx == UINT32_MAX) {
            // 未ロードならロードして割り当て
            texMgr->LoadTexture("resources/uvChecker.png");
            texMgr->ExecuteResourceUpload();
            srvIdx = texMgr->GetSrvIndex("resources/uvChecker.png");
        }
        modelData_.material.textureIndex = srvIdx;
        char buf[256];
        sprintf_s(buf, "Object3d::AssignTexture: no material texture specified, defaulting to uvChecker srvIndex=%u\n", srvIdx);
        Logger::Log(buf);
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
    // 逆転置行列（正規行列用にスケール影響除去）
    Matrix4x4 worldInv = math.Inverse(world);
    Matrix4x4 worldInvTranspose = math.Transpose(worldInv);
    // ワールドビュー射影行列
    // カメラが設定されていればそれを使用
    Matrix4x4 camView = viewMatrix;
    Matrix4x4 camProj = projectionMatrix;
    if (camera_) {
        camView = camera_->GetViewMatrix();
        camProj = camera_->GetProjectionMatrix();
    }
    Matrix4x4 wvp = math.Multiply(world, math.Multiply(camView, camProj));
    // 定数バッファに転送
    if (transformationMatrixData_) {
        transformationMatrixData_->World = world;
        transformationMatrixData_->WVP = wvp;
        transformationMatrixData_->WorldInverseTranspose = worldInvTranspose;
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
        sprintf_s(buf, "Object3d::Draw start: model=%p materialRes=%p transformRes=%p\n",
            reinterpret_cast<void*>(model_),
            reinterpret_cast<void*>(materialResource_.Get()),
            reinterpret_cast<void*>(transformationMatrixResource_.Get()));
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

    // 光源CBV (Object3dCommon で共有されている)
    D3D12_GPU_VIRTUAL_ADDRESS lightAddr = object3dCommon_->GetDirectionalLightGPUAddress();
    if (lightAddr == 0) {
        Logger::Log("Object3d::Draw skipped: directional light GPU address is null\n");
        return;
    }
    {
        char buf[128];
        sprintf_s(buf, "Object3d::Draw: Light GPUAddr=0x%016llX\n", lightAddr);
        Logger::Log(buf);
    }
    cmdList->SetGraphicsRootConstantBufferView(3, lightAddr);

    // Point lights CBV (if available)
    D3D12_GPU_VIRTUAL_ADDRESS plAddr = object3dCommon_->GetPointLightsGPUAddress();
    if (plAddr != 0) {
        cmdList->SetGraphicsRootConstantBufferView(7, plAddr);
    }

    // Camera CBV (Pixel b3) at root parameter 6
    D3D12_GPU_VIRTUAL_ADDRESS camAddr = object3dCommon_->GetCameraGPUAddress();
    if (camAddr != 0) {
        cmdList->SetGraphicsRootConstantBufferView(6, camAddr);
    }

    // Non-instanced draw path: do not bind the instancing SRV to root parameter 4.

    // SRVは TextureManager からハンドルを取り出して RootDescriptorTable にセット
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
        // SRV DescriptorTable - validate handle
        if (srvHandle.ptr == 0) {
            char buf[128]; sprintf_s(buf, "Object3d::Draw: SRV handle for index %u is null - skipping draw\n", texIndex);
            Logger::Log(buf);
            return;
        }
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

    // OBJのフルパスを構築し、リソース探索のためにそのディレクトリを取得
    std::string fullPath = directoryPath + "/" + filename;
    std::filesystem::path objPath(fullPath);
    std::string objDir = objPath.parent_path().string();

    // 2.ファイルを開く
    std::ifstream file(fullPath); // ファイルを開く
    if (!file.is_open()) {
        char buf[256];
        sprintf_s(buf, "Warning: LoadObjFile failed to open %s\n", fullPath.c_str());
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
            // mtl は obj と同じフォルダにあるはずなので objDir を渡す
            modelData.material = LoadMaterialTemplateFile(objDir, materialFilename);
        }
    }

    // MTLでテクスチャが指定されていない場合、OBJと同じディレクトリ内で画像ファイルを探索する
    if (modelData.material.textureFilePath.empty()) {
        // まず同じベース名を試す（例: fence.obj -> fence.png）
        std::string base = objPath.stem().string();
        const std::vector<std::string> exts = { ".png", ".jpg", ".jpeg", ".bmp", ".tga" };
        for (const auto& ext : exts) {
            std::string tryPath = objDir + "/" + base + ext;
            if (std::filesystem::exists(tryPath)) {
                modelData.material.textureFilePath = tryPath;
                char buf[256];
                sprintf_s(buf, "Object3d::LoadObjFile: ベース名からテクスチャを検出 %s\n", tryPath.c_str());
                Logger::Log(buf);
                break;
            }
        }

        // それでも空なら、ディレクトリ内の任意の画像ファイルを走査
        if (modelData.material.textureFilePath.empty()) {
            for (const auto& entry : std::filesystem::directory_iterator(objDir)) {
                if (!entry.is_regular_file()) continue;
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (std::find(exts.begin(), exts.end(), ext) != exts.end()) {
                    modelData.material.textureFilePath = entry.path().string();
                    char buf[256];
                    sprintf_s(buf, "Object3d::LoadObjFile: ディレクトリ内でテクスチャを検出 %s\n", modelData.material.textureFilePath.c_str());
                    Logger::Log(buf);
                    break;
                }
            }
        }
    }

    // それでも見つからない場合、resources 内の既定のチェッカーテクスチャにフォールバック
    if (modelData.material.textureFilePath.empty()) {
        modelData.material.textureFilePath = "resources/uvChecker.png";
        Logger::Log(std::string("Object3d::LoadObjFile: テクスチャが見つからなかったため、resources/uvChecker.png を既定として使用\n"));
    }

    // 最終的に選択されたテクスチャをログ出力
    {
        char buf[256];
        sprintf_s(buf, "LoadObjFile: 最終的な textureFilePath = %s\n", modelData.material.textureFilePath.c_str());
        Logger::Log(buf);
    }

    return modelData;
}
