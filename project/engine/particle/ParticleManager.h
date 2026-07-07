#pragma once

#include "engine/2d/TextureManager.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/utility/mathUtility.h"
#include <cstddef>
#include <vector>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>

// CPU側のパーティクルデータ
struct PM_CpuParticle {
    Math::Transform transform;
    Math::Vector3 startScale;
    Math::Vector3 endScale;
    Math::Vector3 velocity;
    Math::Vector4 color;
    Math::Vector4 startColor;
    float lifeTime = 1.0f;
    float currentTime = 0.0f;
    float spawnTime = 0.0f;
    bool useScaleOverLife = false;
    bool useFadeOut = false;
};

// パーティクルグループ
struct ParticleGroup {
    std::string texturePath; // 使用するテクスチャ
    std::vector<PM_CpuParticle> particles; // パーティクル配列
    uint32_t srvIndex = 0; // SRVインデックス
    MyEngine::Object3d* renderObject = nullptr; // 描画に使うPrimitive
    bool useBillboard = true; // ビルボード描画を使うか
};

namespace MyEngine {

/// <summary>
/// パーティクルマネージャークラス
/// </summary>
class ParticleManager {
public:
    /// <summary>
    /// シングルトンインスタンスを取得する
    /// </summary>
    static ParticleManager* GetInstance()
    {
        static ParticleManager instance;
        return &instance;
    }

    /// <summary>
    /// パーティクルマネージャーを初期化する
    /// </summary>
    void Initialize(DirectXCommon* dx, Object3dCommon* objCommon, SrvManager* srv, TextureManager* texMgr, class ImGuiManager* imguiManager = nullptr);

    /// <summary>
    /// パーティクルマネージャーを終了する
    /// </summary>
    void Finalize();

    /// <summary>
    /// 既定の描画Primitiveを設定する
    /// </summary>
    void SetParticlePlane(Object3d* plane);

    /// <summary>
    /// 指定グループの描画Primitiveを設定する
    /// </summary>
    void SetParticleObject(const std::string& name, Object3d* object);

    /// <summary>
    /// 指定グループのビルボード使用設定を変更する
    /// </summary>
    void SetGroupBillboard(const std::string& name, bool useBillboard);

    /// <summary>
    /// 新しいパーティクルグループを作成する
    /// </summary>
    void CreateParticleGroup(const std::string& name, const std::string& textureFilePath);

    /// <summary>
    /// 既存グループにテクスチャを割り当てる
    /// </summary>
    void SetGroupTexture(const std::string& name, const std::string& textureFilePath);

    /// <summary>
    /// 通常パーティクルを生成する
    /// </summary>
    void Emit(const std::string& name, const Math::Vector3& position, uint32_t count);

    /// <summary>
    /// ヒットエフェクト用のパーティクルを生成する
    /// </summary>
    void EmitHitEffect(const std::string& name, const Math::Vector3& position, uint32_t count);

    /// <summary>
    /// 指定した形状で空間亀裂用のパーティクルを生成する
    /// </summary>
    void EmitSpaceCrack(
        const std::string& name,
        const Math::Vector3& position,
        float rotationZ,
        float length,
        float width,
        const Math::Vector4& color,
        float lifeTime);

    /// <summary>
    /// Ringエフェクト用のパーティクルを生成する
    /// </summary>
    void EmitRingEffect(const std::string& name, const Math::Vector3& position, uint32_t count);

    /// <summary>
    /// Cylinderエフェクト用のパーティクルを生成する
    /// </summary>
    void EmitCylinderEffect(const std::string& name, const Math::Vector3& position, uint32_t count);

    /// <summary>
    /// 次元破砕用の色付きリングを生成する
    /// </summary>
    void EmitRiftRing(
        const std::string& name,
        const Math::Vector3& position,
        uint32_t count,
        const Math::Vector4& color,
        float startScale,
        float endScale,
        float lifeTime);

    /// <summary>
    /// 次元破砕用の放射状破片を生成する
    /// </summary>
    void EmitRiftFragments(
        const std::string& name,
        const Math::Vector3& position,
        uint32_t count,
        const Math::Vector4& color,
        float minimumSpeed,
        float maximumSpeed,
        float lifeTime);
    /// <summary>
    /// パーティクルを更新する
    /// </summary>
    void Update(float dt);

    /// <summary>
    /// パーティクルを描画する
    /// </summary>
    void Draw();

    const std::unordered_map<std::string, ParticleGroup>& GetGroups() const { return particleGroups_; }

    void SetLifetimeRange(float minL, float maxL);
    void SetFieldEnabled(bool enabled);
    void SetFieldAccel(const Math::Vector3& a);
    void SetFieldAABB(const Math::Vector3& mn, const Math::Vector3& mx);
    void SetGravityEnabled(bool enabled);
    void SetGravity(const Math::Vector3& g);
    void SetDamping(float d);
    void SetSpawnPosRange(const Math::Vector3& mn, const Math::Vector3& mx);
    void SetVelocityRange(const Math::Vector3& mn, const Math::Vector3& mx);
    void SetScaleRange(const Math::Vector3& mn, const Math::Vector3& mx);
    void SetColorRange(const Math::Vector4& mn, const Math::Vector4& mx);

    /// <summary>
    /// ImGuiでパーティクル設定を編集する
    /// </summary>
    void DrawImGui();

private:
    ParticleManager() = default;

    /// <summary>
    /// 保持できるパーティクル数の上限を取得する
    /// </summary>
    uint32_t GetParticleLimit() const;

    /// <summary>
    /// 現在の保持数を考慮して実際に生成できるパーティクル数を取得する
    /// </summary>
    uint32_t GetEmitCountWithinLimit(const ParticleGroup& group, uint32_t requestCount) const;

private:
    std::unordered_map<std::string, ParticleGroup> particleGroups_; // グループ一覧
    std::unordered_set<std::string> instancingLimitWarnedGroups_; // インスタンシング上限警告済みグループ
    DirectXCommon* dxCommon_ = nullptr; // DirectX共通処理
    Object3dCommon* object3dCommon_ = nullptr; // 3D共通処理
    SrvManager* srvManager_ = nullptr; // SRV管理
    TextureManager* texManager_ = nullptr; // テクスチャ管理
    Object3d* particlePlane_ = nullptr; // 既定の描画Primitive
    size_t totalParticleCount_ = 0; // 全グループで保持しているパーティクル数

    float lifeMin_ = 1.0f;
    float lifeMax_ = 3.0f;
    bool fieldEnabled_ = false;
    Math::Vector3 fieldAccel_ { 0.0f, 0.0f, 0.0f };
    Math::Vector3 fieldMin_ { -1.0f, -1.0f, -1.0f };
    Math::Vector3 fieldMax_ { 1.0f, 1.0f, 1.0f };
    float globalTime_ = 0.0f;
    std::mt19937 rng_ { std::random_device {}() };

    Math::Vector3 spawnPosMin_ { 0.0f, 0.0f, 0.0f };
    Math::Vector3 spawnPosMax_ { 0.0f, 0.0f, 0.0f };
    Math::Vector3 velMin_ { 0.0f, 0.5f, 0.0f };
    Math::Vector3 velMax_ { 0.0f, 0.5f, 0.0f };
    Math::Vector3 scaleMin_ { 1.0f, 1.0f, 1.0f };
    Math::Vector3 scaleMax_ { 1.0f, 1.0f, 1.0f };
    Math::Vector4 colMin_ { 1.0f, 1.0f, 1.0f, 1.0f };
    Math::Vector4 colMax_ { 1.0f, 1.0f, 1.0f, 1.0f };

    bool gravityEnabled_ = false;
    Math::Vector3 gravity_ { 0.0f, -9.8f, 0.0f };
    float damping_ = 0.0f;

    class ImGuiManager* imguiManager_ = nullptr;
};

} // namespace MyEngine
