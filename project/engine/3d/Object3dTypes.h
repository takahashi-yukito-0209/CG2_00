#pragma once

#include "MathTypes.h"
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace MyEngine::Object3dTypes {

static constexpr uint32_t kNumMaxInfluence = 4; // 1頂点に割り当てる最大Joint影響数

struct VertexData {
    Math::Vector4 position; // 頂点座標
    Math::Vector2 texcoord; // テクスチャ座標
    Math::Vector3 normal; // 法線
};

struct VertexInfluence {
    std::array<float, kNumMaxInfluence> weights {}; // 各Jointの重み
    std::array<int32_t, kNumMaxInfluence> jointIndices {}; // 影響するJointのIndex
};

struct JointWeightData {
    Math::Matrix4x4 inverseBindPoseMatrix; // BindPoseを打ち消すための逆行列
};

struct WellForGPU {
    Math::Matrix4x4 skeletonSpaceMatrix; // Skeleton空間での最終変換行列
    Math::Matrix4x4 skeletonSpaceInverseTransposeMatrix; // 法線変換用の逆転置行列
};

struct Material {
    Math::Vector4 color; // マテリアル色
    int32_t enableLighting; // ライティングを使用するか
    float padding[3]; // GPU転送用の余白
    Math::Matrix4x4 uvTransform; // UV変換行列
    int lightingMode; // ライティング方式
    int32_t useAlphaCutoutSampler; // 0以外の場合、アルファカットアウト用にpoint+clampサンプラーを使用
    int32_t useAlphaDiscard; // 0以外の場合、透過ピクセルをdiscardする
    int32_t useTexture; // 0以外の場合、テクスチャ色を使用する
    float shininess; // 反射の鋭さ（スペキュラー強度の指数）
    float environmentCoefficient; // 環境マップ反射強度
    float pad3[2]; // GPU転送用の余白
};

struct TransformationMatrix {
    Math::Matrix4x4 WVP; // World-View-Projection行列
    Math::Matrix4x4 World; // World行列
    Math::Vector4 color; // インスタンスごとの色（wはアルファ）
    Math::Matrix4x4 WorldInverseTranspose; // 法線変換用の逆転置行列
};

struct DirectionalLight {
    Math::Vector4 color; // ライトの色
    Math::Vector3 direction; // ライトの向き
    float intensity; // 輝度
};

struct PointLight {
    Math::Vector4 position; // ライト位置
    Math::Vector4 color; // ライト色
    float radius; // 影響半径
    float decay; // 減衰率
    int32_t enabled; // 有効状態
    float padding; // GPU転送用の余白
};

struct SpotLight {
    Math::Vector4 position; // ライト位置
    Math::Vector4 color; // ライト色
    Math::Vector3 direction; // ライト方向
    float distance; // 影響距離
    float decay; // 減衰率
    float cosAngle; // 照射角度のcos値
    float cosFalloffStart; // 減衰開始角度のcos値
    int32_t enabled; // 有効状態
    float padding; // GPU転送用の余白
};

struct MaterialData {
    std::string textureFilePath; // テクスチャファイルパス
    uint32_t textureIndex = UINT32_MAX; // TextureManager上のSRV番号
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

struct ModelData {
    struct Node {
        Math::QuaternionTransform transform; // ノードの座標変換情報
        Math::Matrix4x4 localMatrix; // ノードのローカル行列
        std::string name; // ノード名
        std::vector<Node> children; // 子ノードの一覧
    };

    struct MeshPart {
        uint32_t indexOffset = 0; // IndexBuffer内でこのパーツが始まる位置
        uint32_t indexCount = 0; // このパーツが描画に使用するIndex数
        uint32_t materialIndex = 0; // materials内で参照するマテリアル番号
    };

    std::vector<VertexData> vertices; // 展開済み頂点データ
    std::vector<uint32_t> indices; // Index描画で参照する頂点番号
    std::vector<MeshPart> meshParts; // サブメッシュごとの描画範囲とマテリアル参照
    std::vector<MaterialData> materials; // モデルに含まれる複数マテリアル情報
    std::vector<VertexInfluence> vertexInfluences; // 展開済み頂点ごとのSkinning影響情報
    std::unordered_map<std::string, JointWeightData> skinClusterData; // Joint名ごとの逆BindPose情報
    MaterialData material; // 旧描画経路と単一マテリアル用の代表マテリアル情報
    Node rootNode; // Assimpのノードツリー
};

struct Joint {
    Math::QuaternionTransform transform; // Jointの座標変換情報
    Math::Matrix4x4 localMatrix; // Jointのローカル行列
    Math::Matrix4x4 skeletonSpaceMatrix; // Skeleton空間での変換行列
    std::string name; // Joint名
    std::vector<int32_t> children; // 子JointのIndex一覧
    int32_t index = 0; // 自分のIndex
    std::optional<int32_t> parent; // 親JointのIndex（なければnull）
};

struct Skeleton {
    int32_t root = 0; // RootJointのIndex
    std::map<std::string, int32_t> jointMap; // Joint名からIndexを引くための辞書
    std::vector<Joint> joints; // 所属しているJoint一覧
};

} // namespace MyEngine::Object3dTypes