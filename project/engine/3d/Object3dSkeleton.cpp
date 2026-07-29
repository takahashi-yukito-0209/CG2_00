#include "Object3d.h"

#include "DirectXCommon.h"
#include "Model.h"
#include "Object3dCommon.h"
#include "engine/base/SrvManager.h"
#include "mathUtility.h"
#include <cmath>
#include <cstring>

using namespace MyEngine;
using namespace Math;

namespace {
constexpr const char* kSkeletonDebugTexturePath = "resources/textures/white1x1.png"; // Skeletonデバッグ描画用白テクスチャ
constexpr int kDefaultLightingMode = 2; // 初期ライティングモード
constexpr float kDefaultShininess = 32.0f; // 初期スペキュラ指数
/// <summary>
/// Matrix4x4の平行移動成分を取得する
/// </summary>
Vector3 GetMatrixTranslation(const Matrix4x4& matrix)
{
    return { matrix.m[3][0], matrix.m[3][1], matrix.m[3][2] };
}

/// <summary>
/// 2つのVector3を加算する
/// </summary>
Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs)
{
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

/// <summary>
/// 2つのVector3を減算する
/// </summary>
Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs)
{
    return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

/// <summary>
/// Vector3をスカラー倍する
/// </summary>
Vector3 MultiplyVector3(const Vector3& vector, float scalar)
{
    return { vector.x * scalar, vector.y * scalar, vector.z * scalar };
}

/// <summary>
/// Vector3の外積を計算する
/// </summary>
Vector3 CrossVector3(const Vector3& lhs, const Vector3& rhs)
{
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x
    };
}

/// <summary>
/// Vector3の長さを取得する
/// </summary>
float LengthVector3(const Vector3& vector)
{
    return std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
}

/// <summary>
/// デバッグメッシュ用頂点を追加する
/// </summary>
void AddSkeletonDebugVertex(std::vector<Object3d::VertexData>& vertices, const Vector3& position, const Vector3& normal)
{
    vertices.push_back({ { position.x, position.y, position.z, 1.0f }, { 0.0f, 0.0f }, normal });
}

/// <summary>
/// 両面表示用に三角形を表裏で追加する
/// </summary>
void AddDoubleSidedTriangle(std::vector<Object3d::VertexData>& vertices, const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& normal)
{
    AddSkeletonDebugVertex(vertices, a, normal);
    AddSkeletonDebugVertex(vertices, b, normal);
    AddSkeletonDebugVertex(vertices, c, normal);
    AddSkeletonDebugVertex(vertices, c, normal);
    AddSkeletonDebugVertex(vertices, b, normal);
    AddSkeletonDebugVertex(vertices, a, normal);
}

/// <summary>
/// Joint表示用の八面体を追加する
/// </summary>
void AppendJointOctahedron(std::vector<Object3d::VertexData>& vertices, const Vector3& center, float radius)
{
    const Vector3 top = AddVector3(center, { 0.0f, radius, 0.0f }); // 上頂点
    const Vector3 bottom = AddVector3(center, { 0.0f, -radius, 0.0f }); // 下頂点
    const Vector3 right = AddVector3(center, { radius, 0.0f, 0.0f }); // 右頂点
    const Vector3 left = AddVector3(center, { -radius, 0.0f, 0.0f }); // 左頂点
    const Vector3 front = AddVector3(center, { 0.0f, 0.0f, radius }); // 前頂点
    const Vector3 back = AddVector3(center, { 0.0f, 0.0f, -radius }); // 後頂点

    AddDoubleSidedTriangle(vertices, top, front, right, { 0.0f, 1.0f, 0.0f });
    AddDoubleSidedTriangle(vertices, top, right, back, { 0.0f, 1.0f, 0.0f });
    AddDoubleSidedTriangle(vertices, top, back, left, { 0.0f, 1.0f, 0.0f });
    AddDoubleSidedTriangle(vertices, top, left, front, { 0.0f, 1.0f, 0.0f });
    AddDoubleSidedTriangle(vertices, bottom, right, front, { 0.0f, -1.0f, 0.0f });
    AddDoubleSidedTriangle(vertices, bottom, back, right, { 0.0f, -1.0f, 0.0f });
    AddDoubleSidedTriangle(vertices, bottom, left, back, { 0.0f, -1.0f, 0.0f });
    AddDoubleSidedTriangle(vertices, bottom, front, left, { 0.0f, -1.0f, 0.0f });
}

/// <summary>
/// 親子Joint間を結ぶBone表示用の細い角柱を追加する
/// </summary>
void AppendBonePrism(std::vector<Object3d::VertexData>& vertices, const Vector3& start, const Vector3& end, float radius)
{
    const Vector3 axis = SubtractVector3(end, start); // Boneの向き
    if (LengthVector3(axis) <= 0.0001f) {
        return;
    }

    const Vector3 direction = MathUtil::Normalize(axis); // 正規化したBone方向
    const Vector3 reference = std::fabs(direction.y) < 0.95f ? Vector3 { 0.0f, 1.0f, 0.0f } : Vector3 { 1.0f, 0.0f, 0.0f }; // 断面基準軸
    const Vector3 side = MultiplyVector3(MathUtil::Normalize(CrossVector3(direction, reference)), radius); // 断面横方向
    const Vector3 up = MultiplyVector3(MathUtil::Normalize(CrossVector3(side, direction)), radius); // 断面縦方向

    const Vector3 s0 = AddVector3(start, AddVector3(side, up));
    const Vector3 s1 = AddVector3(start, SubtractVector3(up, side));
    const Vector3 s2 = SubtractVector3(start, AddVector3(side, up));
    const Vector3 s3 = AddVector3(start, SubtractVector3(side, up));
    const Vector3 e0 = AddVector3(end, AddVector3(side, up));
    const Vector3 e1 = AddVector3(end, SubtractVector3(up, side));
    const Vector3 e2 = SubtractVector3(end, AddVector3(side, up));
    const Vector3 e3 = AddVector3(end, SubtractVector3(side, up));

    AddDoubleSidedTriangle(vertices, s0, e0, e1, direction);
    AddDoubleSidedTriangle(vertices, s0, e1, s1, direction);
    AddDoubleSidedTriangle(vertices, s1, e1, e2, direction);
    AddDoubleSidedTriangle(vertices, s1, e2, s2, direction);
    AddDoubleSidedTriangle(vertices, s2, e2, e3, direction);
    AddDoubleSidedTriangle(vertices, s2, e3, s3, direction);
    AddDoubleSidedTriangle(vertices, s3, e3, e0, direction);
    AddDoubleSidedTriangle(vertices, s3, e0, s0, direction);
}

/// <summary>
/// Skeletonデバッグ表示で省略する終端Joint名か判定する
/// </summary>
bool IsSkeletonDebugEndMarkerJoint(const std::string& jointName)
{
    constexpr const char* kEndMarkerSuffix = "_End"; // glTF内の終端補助Joint名
    const size_t suffixLength = std::strlen(kEndMarkerSuffix); // 接尾辞の長さ
    if (jointName.size() < suffixLength) {
        return false;
    }

    return jointName.compare(jointName.size() - suffixLength, suffixLength, kEndMarkerSuffix) == 0;
}

/// <summary>
/// Skeletonデバッグ表示の対象Jointか判定する
/// </summary>
bool IsSkeletonDebugTargetJoint(const Object3d::ModelData& modelData, const Object3d::Joint& joint)
{
    if (IsSkeletonDebugEndMarkerJoint(joint.name)) {
        return false;
    }
    if (modelData.skinClusterData.empty()) {
        return true;
    }

    return modelData.skinClusterData.find(joint.name) != modelData.skinClusterData.end();
}

}

/// <summary>
/// NodeからJointを再帰的に作成する
/// </summary>
int32_t Object3d::CreateJoint(const ModelData::Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints)
{
    Joint joint {}; // 作成するJoint
    joint.transform = node.transform;
    joint.localMatrix = node.localMatrix;
    joint.skeletonSpaceMatrix = MathUtil::MakeIdentity4x4();
    joint.name = node.name;
    joint.index = static_cast<int32_t>(joints.size());
    joint.parent = parent;

    joints.push_back(joint);

    for (const ModelData::Node& child : node.children) {
        const int32_t childIndex = CreateJoint(child, joint.index, joints); // 子JointのIndex
        joints[joint.index].children.push_back(childIndex);
    }

    return joint.index;
}

/// <summary>
/// Node階層からSkeletonを作成する
/// </summary>
Object3d::Skeleton Object3d::CreateSkeleton(const ModelData::Node& rootNode)
{
    Skeleton skeleton {}; // 作成するSkeleton
    skeleton.root = CreateJoint(rootNode, std::nullopt, skeleton.joints);

    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap.emplace(joint.name, joint.index);
    }

    Update(skeleton);
    return skeleton;
}

/// <summary>
/// Skeletonの行列情報を更新する
/// </summary>
void Object3d::Update(Skeleton& skeleton)
{
    for (Joint& joint : skeleton.joints) {
        joint.localMatrix = MathUtil::MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);

        if (joint.parent) {
            const Joint& parent = skeleton.joints[*joint.parent]; // 親Joint
            joint.skeletonSpaceMatrix = MathUtil::Multiply(joint.localMatrix, parent.skeletonSpaceMatrix);
        } else {
            joint.skeletonSpaceMatrix = joint.localMatrix;
        }
    }
}

/// <summary>
/// AnimationをSkeletonのJointに適用する
/// </summary>
void Object3d::ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime)
{
    for (Joint& joint : skeleton.joints) {
        auto animationIterator = animation.nodeAnimations.find(joint.name); // Joint名に対応するアニメーション
        if (animationIterator == animation.nodeAnimations.end()) {
            continue;
        }

        const NodeAnimation& nodeAnimation = animationIterator->second; // 適用するNodeAnimation
        if (!nodeAnimation.translate.keyframes.empty()) {
            joint.transform.translate = CalculateValue(nodeAnimation.translate.keyframes, animationTime);
        }
        if (!nodeAnimation.rotate.keyframes.empty()) {
            joint.transform.rotate = CalculateValue(nodeAnimation.rotate.keyframes, animationTime);
        }
        if (!nodeAnimation.scale.keyframes.empty()) {
            joint.transform.scale = CalculateValue(nodeAnimation.scale.keyframes, animationTime);
        }
    }
}
/// <summary>
/// Skinning用GPUリソースを解放する
/// </summary>
void Object3d::ReleaseSkinningResources()
{
    if (object3dCommon_ && object3dCommon_->GetSrvManager()) {
        for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
            if (skinningPaletteSrvIndices_[frameIndex] != UINT32_MAX) {
                object3dCommon_->GetSrvManager()->Free(skinningPaletteSrvIndices_[frameIndex]);
            }
        }
    }

    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        if (skinningPaletteResources_[frameIndex] && mappedSkinningPaletteData_[frameIndex]) {
            skinningPaletteResources_[frameIndex]->Unmap(0, nullptr);
        }
        DeferReleaseResource(skinningPaletteResources_[frameIndex]);
        mappedSkinningPaletteData_[frameIndex] = nullptr;
        skinningPaletteSrvIndices_[frameIndex] = UINT32_MAX;
        skinningPaletteSrvHandlesGPU_[frameIndex] = {};
    }

    skinningPaletteJointCount_ = 0;
    hasSkinCluster_ = false;
}

/// <summary>
/// Skeletonデバッグ描画用GPUリソースを解放する
/// </summary>
void Object3d::ReleaseSkeletonDebugResources()
{
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        if (skeletonDebugVertexResources_[frameIndex] && mappedSkeletonDebugVertexData_[frameIndex]) {
            skeletonDebugVertexResources_[frameIndex]->Unmap(0, nullptr);
        }
        DeferReleaseResource(skeletonDebugVertexResources_[frameIndex]);
        mappedSkeletonDebugVertexData_[frameIndex] = nullptr;
        skeletonDebugVertexBufferViews_[frameIndex] = {};
        skeletonDebugVertexCounts_[frameIndex] = 0;
        skeletonDebugVertexCapacities_[frameIndex] = 0;
        skeletonDebugBoneVertexCounts_[frameIndex] = 0;
        skeletonDebugJointVertexCounts_[frameIndex] = 0;
        if (skeletonDebugBoneMaterialResources_[frameIndex] && mappedSkeletonDebugBoneMaterialData_[frameIndex]) {
            skeletonDebugBoneMaterialResources_[frameIndex]->Unmap(0, nullptr);
        }
        DeferReleaseResource(skeletonDebugBoneMaterialResources_[frameIndex]);
        mappedSkeletonDebugBoneMaterialData_[frameIndex] = nullptr;
        if (skeletonDebugJointMaterialResources_[frameIndex] && mappedSkeletonDebugJointMaterialData_[frameIndex]) {
            skeletonDebugJointMaterialResources_[frameIndex]->Unmap(0, nullptr);
        }
        DeferReleaseResource(skeletonDebugJointMaterialResources_[frameIndex]);
        mappedSkeletonDebugJointMaterialData_[frameIndex] = nullptr;
    }

    skeletonDebugTextureIndex_ = UINT32_MAX;
}

/// <summary>
/// Skeletonデバッグ描画用マテリアルを作成する
/// </summary>
void Object3d::CreateSkeletonDebugMaterialResources()
{
    if (!object3dCommon_ || !object3dCommon_->GetDxCommon()) {
        return;
    }

    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon(); // GPUリソース生成元
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        if (!skeletonDebugBoneMaterialResources_[frameIndex]) {
            skeletonDebugBoneMaterialResources_[frameIndex] = dxCommon->CreateBufferResource(sizeof(Material));
            skeletonDebugBoneMaterialResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedSkeletonDebugBoneMaterialData_[frameIndex]));
        }
        if (!skeletonDebugJointMaterialResources_[frameIndex]) {
            skeletonDebugJointMaterialResources_[frameIndex] = dxCommon->CreateBufferResource(sizeof(Material));
            skeletonDebugJointMaterialResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedSkeletonDebugJointMaterialData_[frameIndex]));
        }

        Material debugMaterial {}; // Skeletonデバッグ描画専用マテリアル
        debugMaterial.enableLighting = 0;
        debugMaterial.uvTransform = MathUtil::MakeIdentity4x4();
        debugMaterial.lightingMode = kDefaultLightingMode;
        debugMaterial.useAlphaCutoutSampler = 0;
        debugMaterial.useAlphaDiscard = 0;
        debugMaterial.shininess = kDefaultShininess;
        debugMaterial.environmentCoefficient = 0.0f;

        if (mappedSkeletonDebugBoneMaterialData_[frameIndex]) {
            debugMaterial.color = skeletonDebugBoneColor_;
            *mappedSkeletonDebugBoneMaterialData_[frameIndex] = debugMaterial;
        }
        if (mappedSkeletonDebugJointMaterialData_[frameIndex]) {
            debugMaterial.color = skeletonDebugJointColor_;
            *mappedSkeletonDebugJointMaterialData_[frameIndex] = debugMaterial;
        }
    }

    if (skeletonDebugTextureIndex_ == UINT32_MAX) {
        skeletonDebugTextureIndex_ = ResolveTextureIndex(kSkeletonDebugTexturePath, nullptr, false);
    }
}

/// <summary>
/// Skeletonデバッグ描画用頂点バッファ容量を確保する
/// </summary>
void Object3d::EnsureSkeletonDebugVertexCapacity(uint32_t vertexCount)
{
    if (vertexCount == 0 || !object3dCommon_ || !object3dCommon_->GetDxCommon()) {
        return;
    }

    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon(); // GPUリソース生成元
    const size_t bufferSize = sizeof(VertexData) * static_cast<size_t>(vertexCount); // 必要な頂点バッファサイズ
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        if (skeletonDebugVertexCapacities_[frameIndex] >= vertexCount && skeletonDebugVertexResources_[frameIndex]) {
            continue;
        }

        if (skeletonDebugVertexResources_[frameIndex] && mappedSkeletonDebugVertexData_[frameIndex]) {
            skeletonDebugVertexResources_[frameIndex]->Unmap(0, nullptr);
        }
        DeferReleaseResource(skeletonDebugVertexResources_[frameIndex]);
        mappedSkeletonDebugVertexData_[frameIndex] = nullptr;
        skeletonDebugVertexResources_[frameIndex] = dxCommon->CreateBufferResource(bufferSize);
        skeletonDebugVertexResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedSkeletonDebugVertexData_[frameIndex]));
        skeletonDebugVertexCapacities_[frameIndex] = vertexCount;
        skeletonDebugVertexBufferViews_[frameIndex].BufferLocation = skeletonDebugVertexResources_[frameIndex]->GetGPUVirtualAddress();
        skeletonDebugVertexBufferViews_[frameIndex].SizeInBytes = static_cast<UINT>(bufferSize);
        skeletonDebugVertexBufferViews_[frameIndex].StrideInBytes = static_cast<UINT>(sizeof(VertexData));
    }
}

/// <summary>
/// Skeletonの現在姿勢からデバッグ描画用メッシュを更新する
/// </summary>
void Object3d::UpdateSkeletonDebugMesh(uint32_t frameIndex)
{
    if (!hasSkeleton_ || skeleton_.joints.empty() || frameIndex >= DirectXCommon::kFrameCount) {
        return;
    }

    const ModelData& sourceModelData = model_ ? model_->GetModelData() : modelData_; // Skinning情報の参照元
    std::vector<VertexData> boneVertices; // Bone描画用頂点
    std::vector<VertexData> jointVertices; // Joint描画用頂点
    boneVertices.reserve(skeleton_.joints.size() * 48);
    jointVertices.reserve(skeleton_.joints.size() * 48);

    for (const Joint& joint : skeleton_.joints) {
        if (!IsSkeletonDebugTargetJoint(sourceModelData, joint)) {
            continue;
        }

        const Vector3 jointPosition = GetMatrixTranslation(joint.skeletonSpaceMatrix); // JointのSkeleton空間位置
        AppendJointOctahedron(jointVertices, jointPosition, skeletonDebugJointRadius_);

        if (joint.parent) {
            const Joint& parent = skeleton_.joints[*joint.parent]; // 親Joint
            if (!IsSkeletonDebugTargetJoint(sourceModelData, parent)) {
                continue;
            }

            const Vector3 parentPosition = GetMatrixTranslation(parent.skeletonSpaceMatrix); // 親JointのSkeleton空間位置
            AppendBonePrism(boneVertices, parentPosition, jointPosition, skeletonDebugBoneRadius_);
        }
    }

    std::vector<VertexData> debugVertices; // 今回描画するSkeletonデバッグ頂点
    debugVertices.reserve(boneVertices.size() + jointVertices.size());
    debugVertices.insert(debugVertices.end(), boneVertices.begin(), boneVertices.end());
    debugVertices.insert(debugVertices.end(), jointVertices.begin(), jointVertices.end());

    const uint32_t boneVertexCount = static_cast<uint32_t>(boneVertices.size()); // Bone描画頂点数
    const uint32_t jointVertexCount = static_cast<uint32_t>(jointVertices.size()); // Joint描画頂点数
    const uint32_t vertexCount = static_cast<uint32_t>(debugVertices.size()); // デバッグ描画頂点数
    EnsureSkeletonDebugVertexCapacity(vertexCount);
    skeletonDebugBoneVertexCounts_[frameIndex] = boneVertexCount;
    skeletonDebugJointVertexCounts_[frameIndex] = jointVertexCount;
    skeletonDebugVertexCounts_[frameIndex] = vertexCount;
    if (vertexCount == 0 || !mappedSkeletonDebugVertexData_[frameIndex]) {
        return;
    }

    std::memcpy(mappedSkeletonDebugVertexData_[frameIndex], debugVertices.data(), sizeof(VertexData) * debugVertices.size());
    skeletonDebugVertexBufferViews_[frameIndex].SizeInBytes = static_cast<UINT>(sizeof(VertexData) * debugVertices.size());
}

/// <summary>
/// Skeletonのデバッグメッシュを描画する
/// </summary>
void Object3d::DrawSkeletonDebug()
{
    if (!skeletonDebugDrawEnabled_ || !hasSkeleton_ || !object3dCommon_ || !object3dCommon_->GetDxCommon()) {
        return;
    }

    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon(); // 描画に使用するDirectX共通処理
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList(); // 描画コマンドリスト
    if (!commandList) {
        return;
    }

    const uint32_t frameIndex = dxCommon->GetCurrentFrameIndex(); // 現在のフレーム番号
    CreateSkeletonDebugMaterialResources();
    UpdateSkeletonDebugMesh(frameIndex);
    if (skeletonDebugVertexCounts_[frameIndex] == 0 || skeletonDebugVertexBufferViews_[frameIndex].SizeInBytes == 0) {
        return;
    }

    object3dCommon_->SetSkeletonDebugDrawSetting();
    commandList->IASetVertexBuffers(0, 1, &skeletonDebugVertexBufferViews_[frameIndex]);

    if (!BindTransformationMatrixResource(commandList, "Object3d::DrawSkeletonDebug")) {
        return;
    }
    BindDirectionalLightResource(commandList, "Object3d::DrawSkeletonDebug");
    BindPointLightResource(commandList);
    BindCameraResource(commandList);
    BindTexture(commandList, skeletonDebugTextureIndex_, "Object3d::DrawSkeletonDebug");

    const uint32_t boneVertexCount = skeletonDebugBoneVertexCounts_[frameIndex]; // Bone描画頂点数
    const uint32_t jointVertexCount = skeletonDebugJointVertexCounts_[frameIndex]; // Joint描画頂点数
    if (boneVertexCount > 0 && skeletonDebugBoneMaterialResources_[frameIndex]) {
        commandList->SetGraphicsRootConstantBufferView(0, skeletonDebugBoneMaterialResources_[frameIndex]->GetGPUVirtualAddress());
        commandList->DrawInstanced(boneVertexCount, 1, 0, 0);
    }
    if (jointVertexCount > 0 && skeletonDebugJointMaterialResources_[frameIndex]) {
        commandList->SetGraphicsRootConstantBufferView(0, skeletonDebugJointMaterialResources_[frameIndex]->GetGPUVirtualAddress());
        commandList->DrawInstanced(jointVertexCount, 1, boneVertexCount, 0);
    }
}
/// <summary>
/// 現在のモデル情報からSkinning用GPUリソースを作成する
/// </summary>
void Object3d::CreateSkinningResources(const ModelData& modelData)
{
    ReleaseSkinningResources();

    if (!object3dCommon_ || !object3dCommon_->GetDxCommon() || !object3dCommon_->GetSrvManager()) {
        return;
    }
    if (!hasSkeleton_ || skeleton_.joints.empty() || modelData.skinClusterData.empty()) {
        return;
    }

    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon(); // GPUリソース生成元
    SrvManager* srvManager = object3dCommon_->GetSrvManager(); // SRV割り当て管理
    skinningPaletteJointCount_ = static_cast<uint32_t>(skeleton_.joints.size());
    const size_t paletteBufferSize = sizeof(WellForGPU) * skinningPaletteJointCount_; // Paletteバッファサイズ

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = skinningPaletteJointCount_;
    srvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        if (!srvManager->CanAllocate()) {
            ReleaseSkinningResources();
            return;
        }

        skinningPaletteResources_[frameIndex] = dxCommon->CreateBufferResource(paletteBufferSize);
        skinningPaletteResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedSkinningPaletteData_[frameIndex]));
        skinningPaletteSrvIndices_[frameIndex] = srvManager->Allocate();
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = srvManager->GetCPUDescriptorHandle(skinningPaletteSrvIndices_[frameIndex]); // Palette SRVのCPUハンドル
        skinningPaletteSrvHandlesGPU_[frameIndex] = srvManager->GetGPUDescriptorHandle(skinningPaletteSrvIndices_[frameIndex]);
        dxCommon->GetDevice()->CreateShaderResourceView(skinningPaletteResources_[frameIndex].Get(), &srvDesc, cpuHandle);
    }

    hasSkinCluster_ = true;
    UpdateSkinningPaletteResources();
}

/// <summary>
/// SkeletonからSkinning用Paletteを更新する
/// </summary>
void Object3d::UpdateSkinningPaletteResources()
{
    if (!hasSkinCluster_ || skinningPaletteJointCount_ == 0) {
        return;
    }

    const ModelData* sourceModelData = model_ ? &model_->GetModelData() : &modelData_; // SkinCluster情報の参照元
    const Math::Matrix4x4 identity = MathUtil::MakeIdentity4x4(); // BindPose未登録時の代替行列

    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        WellForGPU* paletteData = mappedSkinningPaletteData_[frameIndex]; // 更新対象のPalette
        if (!paletteData) {
            continue;
        }

        for (uint32_t jointIndex = 0; jointIndex < skinningPaletteJointCount_; ++jointIndex) {
            const Joint& joint = skeleton_.joints[jointIndex]; // Paletteに反映するJoint
            auto skinClusterIterator = sourceModelData->skinClusterData.find(joint.name); // 逆BindPose情報
            const Math::Matrix4x4 inverseBindPoseMatrix = skinClusterIterator != sourceModelData->skinClusterData.end()
                ? skinClusterIterator->second.inverseBindPoseMatrix
                : identity;
            const Math::Matrix4x4 skeletonSpaceMatrix = MathUtil::Multiply(inverseBindPoseMatrix, joint.skeletonSpaceMatrix); // Skinning用最終行列
            paletteData[jointIndex].skeletonSpaceMatrix = skeletonSpaceMatrix;
            paletteData[jointIndex].skeletonSpaceInverseTransposeMatrix = MathUtil::Transpose(MathUtil::Inverse(skeletonSpaceMatrix));
        }
    }
}

/// <summary>
/// Skinning描画が利用可能か取得する
/// </summary>
bool Object3d::CanUseSkinning() const
{
    if (!skinningEnabled_ || !hasSkeleton_ || !hasSkinCluster_ || skinningPaletteJointCount_ == 0) {
        return false;
    }

    const uint32_t frameIndex = object3dCommon_ && object3dCommon_->GetDxCommon()
        ? object3dCommon_->GetDxCommon()->GetCurrentFrameIndex()
        : 0;
    return skinningPaletteSrvHandlesGPU_[frameIndex].ptr != 0;
}

/// <summary>
/// Skinning用Palette SRVのGPUハンドルを取得する
/// </summary>
D3D12_GPU_DESCRIPTOR_HANDLE Object3d::GetSkinningPaletteSrvHandle() const
{
    const uint32_t frameIndex = object3dCommon_ && object3dCommon_->GetDxCommon()
        ? object3dCommon_->GetDxCommon()->GetCurrentFrameIndex()
        : 0;
    return skinningPaletteSrvHandlesGPU_[frameIndex];
}

/// <summary>
/// 現在時刻のAnimationをObject3dとSkeletonへ反映する
/// </summary>
void Object3d::ApplyAnimationAtCurrentTime()
{
    if (!hasAnimation_ || animation_.duration <= 0.0f || animation_.nodeAnimations.empty()) {
        return;
    }

    if (hasSkeleton_) {
        ApplyAnimation(skeleton_, animation_, animationTime_);
        Update(skeleton_);
        UpdateSkinningPaletteResources();
        if (hasSkinCluster_) {
            animationLocalMatrix_ = MathUtil::MakeIdentity4x4();
            return;
        }
    }

    const std::string& rootNodeName = modelData_.rootNode.name; // ルートノード名
    auto nodeAnimationIterator = animation_.nodeAnimations.find(rootNodeName); // ルートノードのアニメーション
    if (nodeAnimationIterator == animation_.nodeAnimations.end()) {
        if (model_ && model_->GetModelData().rootNode.name != rootNodeName) {
            nodeAnimationIterator = animation_.nodeAnimations.find(model_->GetModelData().rootNode.name);
        }
        if (nodeAnimationIterator == animation_.nodeAnimations.end()) {
            nodeAnimationIterator = animation_.nodeAnimations.begin();
        }
    }

    const NodeAnimation& rootNodeAnimation = nodeAnimationIterator->second; // 適用するノードアニメーション
    if (rootNodeAnimation.translate.keyframes.empty() && rootNodeAnimation.rotate.keyframes.empty() && rootNodeAnimation.scale.keyframes.empty()) {
        return;
    }

    const Math::Vector3 translate = rootNodeAnimation.translate.keyframes.empty()
        ? Math::Vector3 { 0.0f, 0.0f, 0.0f }
        : CalculateValue(rootNodeAnimation.translate.keyframes, animationTime_); // 指定時刻の平行移動
    const Math::Quaternion rotate = rootNodeAnimation.rotate.keyframes.empty()
        ? Math::Quaternion { 0.0f, 0.0f, 0.0f, 1.0f }
        : CalculateValue(rootNodeAnimation.rotate.keyframes, animationTime_); // 指定時刻の回転
    const Math::Vector3 scale = rootNodeAnimation.scale.keyframes.empty()
        ? Math::Vector3 { 1.0f, 1.0f, 1.0f }
        : CalculateValue(rootNodeAnimation.scale.keyframes, animationTime_); // 指定時刻のスケール
    animationLocalMatrix_ = MathUtil::MakeAffineMatrix(scale, rotate, translate);
}
/// <summary>
/// 現在のモデル情報からSkeletonを再構築する
/// </summary>
void Object3d::RebuildSkeletonFromModel()
{
    const ModelData* sourceModelData = nullptr; // Skeleton作成に使うモデルデータ
    if (model_) {
        sourceModelData = &model_->GetModelData();
    } else if (!modelData_.rootNode.name.empty()) {
        sourceModelData = &modelData_;
    }

    if (!sourceModelData || sourceModelData->rootNode.name.empty()) {
        skeleton_ = Skeleton {};
        hasSkeleton_ = false;
        return;
    }

    skeleton_ = CreateSkeleton(sourceModelData->rootNode);
    hasSkeleton_ = !skeleton_.joints.empty();
}

/// <summary>
/// 現在のモデル情報からSkinning用GPUリソースを作り直す
/// </summary>
void Object3d::RefreshSkinningResourcesFromModel()
{
    if (model_) {
        CreateSkinningResources(model_->GetModelData());
        return;
    }

    if (!modelData_.skinClusterData.empty()) {
        CreateSkinningResources(modelData_);
        return;
    }

    ReleaseSkinningResources();
}
