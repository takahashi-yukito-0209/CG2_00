#include "Object3d.h"
#include "../utility/ResourceResolver.h"
#include "Camera.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "Model.h"
#include "ModelCommon.h"
#include "ModelManager.h"
#include "Object3dCommon.h"
#include "StringUtility.h"
#include "TextureManager.h"
#include <memory>
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif
#include "mathUtility.h"
#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cstring>
#include <filesystem>
#include <fstream>

using namespace MyEngine;
using Microsoft::WRL::ComPtr;
using namespace Math;

/// <summary>
/// Assimp のノードを再帰的に読み込んで Object3d::ModelData::Node に変換する関数
/// </summary>
static Object3d::ModelData::Node ReadNode(const aiNode* node)
{
    Object3d::ModelData::Node result;
    // Assimp の行列は行優先で格納されているため、転置して列優先に変換する
    aiMatrix4x4 aiLocal = node->mTransformation;
    aiLocal.Transpose();
    // 行列のコピー
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            result.localMatrix.m[r][c] = aiLocal[r][c];
        }
    }

    // ノード名のコピー
    result.name = node->mName.C_Str();
    // 子ノードを再帰的に読み込む
    result.children.resize(node->mNumChildren);
    // 子ノードの数だけループして読み込む
    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
    }

    // 読み込んだノードを返す
    return result;
}

/// <summary>
/// Object3d の初期化
/// </summary>
void Object3d::Initialize(Object3dCommon* object3dCommon, ImGuiManager* imguiManager)
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

    // ModelCommon を生成して初期化
    modelCommon_ = std::make_unique<ModelCommon>();
    modelCommon_->Initialize(object3dCommon->GetDxCommon());

    // Model を読み込んでセット
    ModelManager* mgr = ModelManager::GetInstance();
    // デフォルトでは plane.obj を読み込むが、後で SetModel(file) で差し替え可能
    model_ = mgr->LoadModel("resources", "plane.obj", modelCommon_.get());
    // モデル読み込み後にテクスチャ割り当てを行う（MTLが先に読み込まれるように）
    AssignTexture();

    // 既定のカメラを参照
    camera_ = object3dCommon->GetDefaultCamera();

    (void)imguiManager;
}

/// <summary>
/// ImGui を使用してオブジェクトのパラメータを編集するための関数
/// </summary>
void Object3d::DrawImGui(int index)
{
#ifdef USE_IMGUI
    // オブジェクト識別用のテキスト
    ImGui::Text("Object %d", index);
    ImGui::DragFloat3("Scale", &transform_.scale.x, 0.01f, 0.001f, 1000.0f);
    ImGui::DragFloat3("Rotate", &transform_.rotate.x, 0.01f);
    ImGui::DragFloat3("Translate", &transform_.translate.x, 0.01f);

    // マテリアル編集
    if (materialData_) {
        ImGui::Checkbox("Use Alpha Cutout Sampler", &useAlphaCutoutSampler_);
        materialData_->useAlphaCutoutSampler = useAlphaCutoutSampler_ ? 1 : 0;
        // Color control for material
        float col[4] = { materialData_->color.x, materialData_->color.y, materialData_->color.z, materialData_->color.w };
        if (ImGui::ColorEdit4("Color", col)) {
            materialData_->color.x = col[0];
            materialData_->color.y = col[1];
            materialData_->color.z = col[2];
            materialData_->color.w = col[3];
        }
        ImGui::SliderFloat("Environment Reflection", &materialData_->environmentCoefficient, 0.0f, 1.0f);
    }
#else
    (void)index;
    (void)materialData_;
    (void)useAlphaCutoutSampler_;
#endif
}

/// <summary>
/// ファイルパスを指定してテクスチャを割り当てる関数
/// </summary>
void Object3d::SetTexture(const std::string& filePath)
{
    auto texMgr = TextureManager::GetInstance();
    std::string texToUse = filePath;
    std::string resolved = ResourceResolver::Resolve(filePath, ResourceResolver::Type::Texture);
    if (!resolved.empty())
        texToUse = resolved;
    // 既にロード済みでなければロードを依頼
    uint32_t idx = texMgr->GetTextureIndexByFilePath(filePath);
    if (idx == UINT32_MAX) {
        texMgr->LoadTexture(filePath);
        // 転送を実行して SRV を作成する
        texMgr->ExecuteResourceUpload();
        idx = texMgr->GetTextureIndexByFilePath(filePath);
    }

    // マテリアルデータにファイルパスとインデックスを設定
    modelData_.material.textureFilePath = texToUse;
    modelData_.material.textureIndex = (idx == UINT32_MAX) ? UINT32_MAX : idx;

    // Model が既に持つマテリアル側も更新しておく
    if (model_) {
        // Model::Initialize がセットする textureIndex_ は読み込み時のみ反映されるため
        // ここではモデルが保持する textureIndex_ を直接更新するためのアクセサはない。
        // 代わりに、Model 側の描画時に Object3d の modelData を参照するので十分。
    }
}

/// <summary>
/// ファイルパスを指定してモデルを読み込んでセットする関数
/// </summary>
void Object3d::SetModel(const std::string& filePath)
{
    ModelManager* mgr = ModelManager::GetInstance();
    std::string resolved = ResourceResolver::Resolve(filePath, ResourceResolver::Type::Model);
    Model* m = nullptr;
    if (!resolved.empty()) {
        // 解決されたパスで読み込む
        std::filesystem::path p(resolved);
        m = mgr->LoadModel(p.parent_path().string(), p.filename().string(), modelCommon_.get());
    } else {
        // 直接指定されたパスで読み込む
        m = mgr->LoadModel("resources", filePath, modelCommon_.get());
    }
    model_ = m; // 成功すればポインタが入る。失敗時は nullptr になる
    // モデル読み込み後にテクスチャの割り当てを行う
    AssignTexture();
}

/// <summary>
/// 頂点データを直接セットする関数
/// </summary>
void Object3d::SetMesh(const std::vector<VertexData>& vertices)
{
    model_ = nullptr;
    modelData_.vertices = vertices;

    if (vertices.empty() || !object3dCommon_ || !object3dCommon_->GetDxCommon()) {
        vertexResource_.Reset();
        vertexData_ = nullptr;
        vertexBufferView_ = {};
        return;
    }

    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon(); // DirectX共通処理
    const size_t vertexBufferSize = sizeof(VertexData) * vertices.size(); // 頂点バッファサイズ

    vertexResource_ = dxCommon->CreateBufferResource(vertexBufferSize);
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    std::memcpy(vertexData_, vertices.data(), vertexBufferSize);
    vertexResource_->Unmap(0, nullptr);
    vertexData_ = nullptr;

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(vertexBufferSize);
    vertexBufferView_.StrideInBytes = static_cast<UINT>(sizeof(VertexData));
}

/// <summary>
/// Object3d の終了処理
/// </summary>
Object3d::~Object3d()
{
    // modelCommon_ は std::unique_ptr なので自動解放される
}

/// <summary>
/// .mtlファイルを読み取る関数
/// </summary>
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

/// <summary>
/// マテリアル用リソースを作成する関数
/// </summary>
void Object3d::CreateMaterialResource()
{
    // マテリアル用リソース作成
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();
    // マテリアル用バッファリソースを作成
    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
    // マップしてデータを書き込む
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    // デフォルト値
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    // ライティングを有効化(3Dオブジェクトはライティング対象)
    materialData_->enableLighting = 1;
    // 初期UV変換は単位行列にする
    materialData_->uvTransform = MathUtil::MakeIdentity4x4();
    // ライティングモードは通常のものにする
    materialData_->lightingMode = 2;
    // 既定: アルファカットアウト用サンプラーを強制しない
    // フェンスモデルは初期化後にこれを true に設定する場合がある
    useAlphaCutoutSampler_ = false;
    // GPU可視フラグを0で初期化
    materialData_->useAlphaCutoutSampler = 0;
    // 光沢（shininess）の既定値
    materialData_->shininess = 32.0f;
    materialData_->environmentCoefficient = 0.0f;
}

/// <summary>
/// ライティングの有効/無効を取得する関数
/// </summary>
bool Object3d::GetEnableLighting() const { return materialData_ ? materialData_->enableLighting != 0 : false; }

/// <summary>
/// ライティングの有効/無効を設定する関数
/// </summary>
void Object3d::SetEnableLighting(bool enable)
{
    // マテリアルデータが存在する場合にのみ設定を変更する
    if (materialData_) {
        materialData_->enableLighting = enable ? 1 : 0;
    }
}

/// <summary>
/// ライティングモードを取得する関数
/// </summary>
int Object3d::GetLightingMode() const { return materialData_ ? materialData_->lightingMode : 0; }

/// <summary>
/// ライティングモードを設定する関数
/// </summary>
void Object3d::SetLightingMode(int mode)
{
    // マテリアルデータが存在する場合にのみ設定を変更する
    if (materialData_) {
        materialData_->lightingMode = mode;
    }
}

void Object3d::SetEnvironmentCoefficient(float coefficient)
{
    if (materialData_) {
        materialData_->environmentCoefficient = std::clamp(coefficient, 0.0f, 1.0f);
    }
}

float Object3d::GetEnvironmentCoefficient() const
{
    return materialData_ ? materialData_->environmentCoefficient : 0.0f;
}

/// <summary>
/// UV変換行列を設定する
/// </summary>
void Object3d::SetUVTransform(const Math::Matrix4x4& uvTransform)
{
    if (materialData_) {
        materialData_->uvTransform = uvTransform;
    }
}

/// <summary>
/// 座標変換行列用リソースを作成する関数
/// </summary>
void Object3d::CreateTransformationMatrixResource()
{
    // 座標変換行列用リソース作成
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();
    // 変換行列用バッファリソースを作成
    transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
    // マップしてデータを書き込む
    transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
}

/// <summary>
/// テクスチャマネージャを使用してモデルのテクスチャを割り当てる関数
/// </summary>
void Object3d::AssignTexture()
{
    // テクスチャマネージャからインデックスを取得してモデルデータに格納
    auto texMgr = TextureManager::GetInstance();
    if (!modelData_.material.textureFilePath.empty()) {
        uint32_t idx = texMgr->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
        if (idx == UINT32_MAX) {
            // ロードされていないテクスチャが指定されている場合はロードを依頼してからインデックスを取得する
            texMgr->LoadTexture(modelData_.material.textureFilePath);
            // 転送を実行して SRV を作成する
            texMgr->ExecuteResourceUpload();
            // 再度インデックスを取得する
            idx = texMgr->GetSrvIndex(modelData_.material.textureFilePath);
        }

        // 取得したインデックスをモデルデータに格納する
        modelData_.material.textureIndex = idx;

        // ログ出力: テクスチャ割り当て結果
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
            // 転送を実行して SRV を作成する
            texMgr->ExecuteResourceUpload();
            // 再度インデックスを取得する
            srvIdx = texMgr->GetSrvIndex("resources/uvChecker.png");
        }

        // モデルデータにSRVインデックスを格納する
        modelData_.material.textureIndex = srvIdx;

        // ログ出力: デフォルトテクスチャ割り当て
        char buf[256];
        sprintf_s(buf, "Object3d::AssignTexture: no material texture specified, defaulting to uvChecker srvIndex=%u\n", srvIdx);
        Logger::Log(buf);

    } else {
        // ロードされたテクスチャが1枚もない場合は、インデックスを無効値のままにしておく
        modelData_.material.textureIndex = UINT32_MAX;
        Logger::Log("Object3d::AssignTexture: no textures loaded, leaving textureIndex invalid\n");
    }
}

/// <summary>
/// 座標変換行列を更新して定数バッファに転送する関数
/// </summary>
void Object3d::Update(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix)
{
    // WVP行列計算
    // ワールド行列
    Matrix4x4 world = MathUtil::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    // 逆転置行列（正規行列用にスケール影響除去）
    Matrix4x4 worldInv = MathUtil::Inverse(world);
    Matrix4x4 worldInvTranspose = MathUtil::Transpose(worldInv);

    // ワールドビュー射影行列
    // カメラが設定されていればそれを使用
    Matrix4x4 camView = viewMatrix;
    Matrix4x4 camProj = projectionMatrix;

    // WVP行列の計算
    Matrix4x4 wvp = MathUtil::Multiply(world, MathUtil::Multiply(camView, camProj));

    // 定数バッファに転送
    if (transformationMatrixData_) {
        transformationMatrixData_->World = world;
        transformationMatrixData_->WVP = wvp;
        transformationMatrixData_->WorldInverseTranspose = worldInvTranspose;
    }
}

/// <summary>
/// 描画関数。モデルがセットされていればモデルの Draw に任せる。セットされていなければ頂点バッファから直接描画する。
/// </summary>
void Object3d::Draw()
{
    // Object3dCommon がセットされていない場合は描画できないのでログを出して終了する
    if (!object3dCommon_) {
        Logger::Log("Object3d::Draw skipped: object3dCommon_ is null\n");
        return;
    }

    // DirectXCommon が Object3dCommon から取得できない場合は描画できないのでログを出して終了する
    if (!object3dCommon_->GetDxCommon()) {
        Logger::Log("Object3d::Draw skipped: DxCommon is null\n");
        return;
    }

    // 描画に必要なコマンドを積む
    auto cmdList = object3dCommon_->GetDxCommon()->GetCommandList();

    // コマンドリストが取得できない場合は描画できないのでログを出して終了する
    if (!cmdList) {
        Logger::Log("Object3d::Draw skipped: command list is null\n");
        return;
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
    // GPU仮想アドレスを取得してルートパラメータ0にセット
    auto matAddr = materialResource_->GetGPUVirtualAddress();
    // ルートパラメータ0にマテリアルCBVをセット
    cmdList->SetGraphicsRootConstantBufferView(0, matAddr);

    // 座標変換行列CBV (必須)
    if (!transformationMatrixResource_) {
        Logger::Log("Object3d::Draw skipped: transformationMatrixResource_ is null\n");
        return;
    }

    // GPU仮想アドレスを取得してルートパラメータ1にセット
    auto wvpAddr = transformationMatrixResource_->GetGPUVirtualAddress();
    // ルートパラメータ1に座標変換行列CBVをセット
    cmdList->SetGraphicsRootConstantBufferView(1, wvpAddr);

    // 光源CBV (Object3dCommon で共有されている)
    D3D12_GPU_VIRTUAL_ADDRESS lightAddr = object3dCommon_->GetDirectionalLightGPUAddress();
    // 光源CBVが利用できない場合は描画をスキップしてログを出す
    if (lightAddr == 0) {
        Logger::Log("Object3d::Draw skipped: directional light GPU address is null\n");
        return;
    }
    // ルートパラメータ3に光源CBVをセット
    cmdList->SetGraphicsRootConstantBufferView(3, lightAddr);

    // 点光源CBV (Object3dCommon で共有されている)
    D3D12_GPU_VIRTUAL_ADDRESS plAddr = object3dCommon_->GetPointLightsGPUAddress();
    // 点光源CBVが利用できない場合はログを出すが描画は続行する（点光源なしで描画する）
    if (plAddr != 0) {
        cmdList->SetGraphicsRootConstantBufferView(7, plAddr);
    }

    // カメラCBV (Object3dCommon で共有されている)
    D3D12_GPU_VIRTUAL_ADDRESS camAddr = object3dCommon_->GetCameraGPUAddress();
    // カメラCBVが利用できない場合はログを出すが描画は続行する（カメラ位置なしで描画する）
    if (camAddr != 0) {
        cmdList->SetGraphicsRootConstantBufferView(6, camAddr);
    }

    // SRVは TextureManager からハンドルを取り出して RootDescriptorTable にセット
    uint32_t texIndex = modelData_.material.textureIndex;
    // テクスチャインデックスが有効な場合のみSRVをセットする
    auto texMgr = TextureManager::GetInstance();
    // SRVハンドルを取得してルートパラメータ2にセット
    if (texIndex != UINT32_MAX) {
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = texMgr->GetSrvHandleGPU(texIndex);
        if (srvHandle.ptr != 0) {
            cmdList->SetGraphicsRootDescriptorTable(2, srvHandle);
        } else {
            char buf[128];
            sprintf_s(buf, "Object3d::Draw: SRV handle for index %u is null - skipping SRV bind\n", texIndex);
            Logger::Log(buf);
        }
    } else {
        char buf[128];
        sprintf_s(buf, "Object3d::Draw: no valid texture assigned (index=%u) - skipping SRV\n", texIndex);
        Logger::Log(buf);
    }
    // 描画コマンド
    cmdList->DrawInstanced(static_cast<UINT>(modelData_.vertices.size()), 1, 0, 0);
}

/// <summary>
/// 同じメッシュを指定数だけインスタンシング描画する関数
/// </summary>
void Object3d::DrawInstanced(uint32_t instanceCount)
{
    if (instanceCount == 0 || !object3dCommon_ || !object3dCommon_->GetDxCommon()) {
        return;
    }

    if (model_) {
        model_->DrawInstanced(this, instanceCount);
        return;
    }

    if (vertexBufferView_.SizeInBytes == 0 || modelData_.vertices.empty()) {
        return;
    }

    auto cmdList = object3dCommon_->GetDxCommon()->GetCommandList(); // 描画コマンドリスト
    if (!cmdList) {
        return;
    }

    cmdList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    if (materialResource_) {
        cmdList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    } else {
        return;
    }

    if (transformationMatrixResource_) {
        cmdList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
    }

    D3D12_GPU_VIRTUAL_ADDRESS lightAddress = object3dCommon_->GetDirectionalLightGPUAddress(); // 平行光源のGPUアドレス
    if (lightAddress != 0) {
        cmdList->SetGraphicsRootConstantBufferView(3, lightAddress);
    }

    D3D12_GPU_VIRTUAL_ADDRESS cameraAddress = object3dCommon_->GetCameraGPUAddress(); // カメラ情報のGPUアドレス
    if (cameraAddress != 0) {
        cmdList->SetGraphicsRootConstantBufferView(6, cameraAddress);
    }

    D3D12_GPU_VIRTUAL_ADDRESS pointLightAddress = object3dCommon_->GetPointLightsGPUAddress(); // 点光源のGPUアドレス
    if (pointLightAddress != 0) {
        cmdList->SetGraphicsRootConstantBufferView(7, pointLightAddress);
    }

    auto texMgr = TextureManager::GetInstance(); // テクスチャ管理
    const uint32_t textureIndex = modelData_.material.textureIndex; // 使用するテクスチャ番号
    if (texMgr && textureIndex != UINT32_MAX) {
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = texMgr->GetSrvHandleGPU(textureIndex);
        if (srvHandle.ptr != 0) {
            cmdList->SetGraphicsRootDescriptorTable(2, srvHandle);
        }
    }

    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandle = object3dCommon_->GetInstancingSrvGPUHandle(); // インスタンシング用SRV
    if (instancingSrvHandle.ptr != 0) {
        cmdList->SetGraphicsRootDescriptorTable(4, instancingSrvHandle);
    }

    cmdList->DrawInstanced(static_cast<UINT>(modelData_.vertices.size()), instanceCount, 0, 0);
}

/// <summary>
/// モデルファイルを読みこむ関数。Assimp を使用して obj/glTF 等のモデルファイルを読み取る汎用関数。
/// </summary>
Object3d::ModelData Object3d::LoadModelFile(const std::string& directoryPath, const std::string& filename)
{
    ModelData modelData; // 構築するModelData

    // フルパスとディレクトリを用意
    std::string fullPath = directoryPath + "/" + filename;
    std::filesystem::path objPath(fullPath);
    std::string objDir = objPath.parent_path().string();

    // Assimp を使って読み込む
    Assimp::Importer importer;
    // 読み込み時のオプションを指定してファイルを読み込む
    const aiScene* scene = importer.ReadFile(fullPath.c_str(),
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_FlipWindingOrder);

    // 読み込み失敗のチェック
    if (!scene) {
        char buf[256];
        sprintf_s(buf, "Warning: Assimp failed to load %s\n", fullPath.c_str());
        Logger::Log(buf);
        return modelData;
    }

    // メッシュがない場合は警告を出して空の ModelData を返す
    if (!scene->HasMeshes()) {
        char buf[256];
        sprintf_s(buf, "Warning: Assimp scene has no meshes %s\n", fullPath.c_str());
        Logger::Log(buf);
        return modelData;
    }

    // ノード階層を読み取って modelData.rootNode に保存
    if (scene->mRootNode) {
        modelData.rootNode = ReadNode(scene->mRootNode);
    }

    // マテリアルからテクスチャパスを取得（見つかるものを最後のもので上書きする挙動を維持）
    for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
        aiMaterial* material = scene->mMaterials[materialIndex];
        if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
            aiString textureFilePath;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
            // Assimp の返すパスは相対パスの場合があるので、obj のディレクトリを基準にする
            modelData.material.textureFilePath = objDir + "/" + textureFilePath.C_Str();
        }
    }

    // メッシュデータを展開する（シーンに含まれる全メッシュを結合する）
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh* mesh = scene->mMeshes[meshIndex];

        // 法線は必須とする（ない場合は警告を出してそのメッシュをスキップする）
        if (!mesh->HasNormals()) {
            char buf[256];
            sprintf_s(buf, "Warning: mesh %u has no normals - skipping\n", meshIndex);
            Logger::Log(buf);
            continue;
        }

        // テクスチャ座標はオプション（ない場合は 0 を使う）
        bool hasTex = mesh->HasTextureCoords(0);

        // メッシュの全ての面をループして頂点データを展開する
        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            // 面を取り出す
            const aiFace& face = mesh->mFaces[faceIndex];
            // 三角形のみサポート
            if (face.mNumIndices != 3) {
                continue;
            }

            // 各頂点を取り出し、VertexData を作成して順次 push する
            for (uint32_t element = 0; element < 3; ++element) {
                // 頂点インデックスを取得して頂点データを取り出す
                uint32_t vertexIndex = face.mIndices[element];
                const aiVector3D& p = mesh->mVertices[vertexIndex];
                const aiVector3D& n = mesh->mNormals[vertexIndex];
                aiVector3D t(0, 0, 0);
                // テクスチャ座標がある場合は取得する
                if (hasTex) {
                    t = mesh->mTextureCoords[0][vertexIndex];
                }

                VertexData vtx;
                // 既存実装との互換性のため X を反転（右手系->左手系の調整）
                vtx.position = { -p.x, p.y, p.z, 1.0f };
                // Assimp の aiProcess_FlipUVs を指定しているためここで y を反転しない
                vtx.texcoord = { t.x, t.y };
                // 法線の X も反転
                vtx.normal = { -n.x, n.y, n.z };

                // 作成した頂点をモデルデータに追加する
                modelData.vertices.push_back(vtx);
            }
        }
    }

    // Assimp でマテリアルから見つからなかった場合は従来のフォールバック処理を行う
    if (modelData.material.textureFilePath.empty()) {
        // まず同じベース名を試す（例: fence.obj -> fence.png）
        std::string base = objPath.stem().string();
        // 拡張子の候補を用意してループで試す
        const std::vector<std::string> exts = { ".png", ".jpg", ".jpeg", ".bmp", ".tga" };
        // ベース名 + 拡張子の組み合わせでファイルが存在するかチェック
        for (const auto& ext : exts) {
            // obj のディレクトリにベース名 + 拡張子のファイルが存在するか確認
            std::string tryPath = objDir + "/" + base + ext;
            // ファイルが存在すればそれをテクスチャパスとして使用する
            if (std::filesystem::exists(tryPath)) {
                // 見つかったファイルをテクスチャパスに設定する
                modelData.material.textureFilePath = tryPath;
                // ログ出力: ベース名から見つかったテクスチャファイル
                char buf[256];
                sprintf_s(buf, "Object3d::LoadModelFile: ベース名からテクスチャを検出 %s\n", tryPath.c_str());
                Logger::Log(buf);
                break;
            }
        }

        // それでも空なら、ディレクトリ内の任意の画像ファイルを走査
        if (modelData.material.textureFilePath.empty()) {
            for (const auto& entry : std::filesystem::directory_iterator(objDir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                // 拡張子を小文字にしてチェック
                std::string ext = entry.path().extension().string();
                // 小文字に変換
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                // 対応する拡張子か確認
                if (std::find(exts.begin(), exts.end(), ext) != exts.end()) {
                    // 最初に見つかった画像ファイルをテクスチャとして使用する
                    modelData.material.textureFilePath = entry.path().string();
                    // ログ出力: ディレクトリ内で見つかったテクスチャファイル
                    char buf[256];
                    sprintf_s(buf, "Object3d::LoadModelFile: ディレクトリ内でテクスチャを検出 %s\n", modelData.material.textureFilePath.c_str());
                    Logger::Log(buf);
                    break;
                }
            }
        }
    }

    // フォールバック: resources のチェッカーテクスチャ
    if (modelData.material.textureFilePath.empty()) {
        modelData.material.textureFilePath = "resources/uvChecker.png";
        Logger::Log(std::string("Object3d::LoadModelFile: テクスチャが見つからなかったため、resources/uvChecker.png を既定として使用\n"));
    }

    // 最終的に選択されたテクスチャをログ出力
    {
        char buf[256];
        sprintf_s(buf, "LoadModelFile: 最終的な textureFilePath = %s\n", modelData.material.textureFilePath.c_str());
        Logger::Log(buf);
    }

    // 読み込んだモデルデータを返す
    return modelData;
}
