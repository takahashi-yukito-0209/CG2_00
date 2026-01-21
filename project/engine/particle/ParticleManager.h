#pragma once

#include <string>
#include <unordered_map>
#include <list>
#include <random>
#include "engine/utility/mathUtility.h"
#include "engine/2d/TextureManager.h"
#include "engine/base/SrvManager.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Object3d.h"

// 簡易CPUパーティクル（最小構成）
struct PM_CpuParticle {
    Transform transform;
    Vector3 velocity;
    Vector4 color;
    float lifeTime = 1.0f;
    float currentTime = 0.0f;
    float spawnTime = 0.0f;
};

// パーティクルグループ
struct ParticleGroup {
    std::string texturePath; // マテリアル/テクスチャファイルパス
    std::list<PM_CpuParticle> particles; // パーティクルのリスト
    uint32_t srvIndex = 0; // SRVインデックス（将来使用）
};

namespace MyEngine {

class ParticleManager {
public:
    static ParticleManager* GetInstance() {
        static ParticleManager instance;
        return &instance;
    }

    // 初期化（必要な参照を受け取る）
    void Initialize(MyEngine::DirectXCommon* dx,
                    MyEngine::Object3dCommon* objCommon,
                    MyEngine::SrvManager* srv,
                    MyEngine::TextureManager* texMgr);

    // 描画用のプレーン（モデル）設定
    void SetParticlePlane(MyEngine::Object3d* plane);

    // パーティクルグループの生成
    void CreateParticleGroup(const std::string& name, const std::string& textureFilePath);
    // Set texture for an existing group (filePath should be loaded or will be loaded)
    void SetGroupTexture(const std::string& name, const std::string& textureFilePath);

    // Emit
    void Emit(const std::string& name, const Vector3& position, uint32_t count);

    // 更新（寿命と移動のみの最小ロジック）
    void Update(float dt);

    // 描画（各グループ1DrawCall）
    void Draw();

    const std::unordered_map<std::string, ParticleGroup>& GetGroups() const { return particleGroups_; }

    // 設定用セッター
    void SetLifetimeRange(float minL, float maxL);
    void SetFieldEnabled(bool enabled);
    void SetFieldAccel(const Vector3& a);
    void SetFieldAABB(const Vector3& mn, const Vector3& mx);
    void SetGravityEnabled(bool enabled);
    void SetGravity(const Vector3& g);
    void SetDamping(float d);

    // 発生時のランダム範囲設定
    void SetSpawnPosRange(const Vector3& mn, const Vector3& mx);   // 発生位置のジッター
    void SetVelocityRange(const Vector3& mn, const Vector3& mx);   // 初速の範囲
    void SetScaleRange(const Vector3& mn, const Vector3& mx);      // 初期スケール範囲
    void SetColorRange(const Vector4& mn, const Vector4& mx);      // 初期カラー範囲

    // Draw ImGui controls for this manager
    void DrawImGui();

private:
    ParticleManager() = default;
    std::unordered_map<std::string, ParticleGroup> particleGroups_;
    // 参照
    MyEngine::DirectXCommon* dxCommon_ = nullptr;
    MyEngine::Object3dCommon* object3dCommon_ = nullptr;
    MyEngine::SrvManager* srvManager_ = nullptr;
    MyEngine::TextureManager* texManager_ = nullptr;
    MyEngine::Object3d* particlePlane_ = nullptr; // 描画用プレーン

    // 設定
    float lifeMin_ = 1.0f;
    float lifeMax_ = 3.0f;
    bool fieldEnabled_ = false;
    Vector3 fieldAccel_ {0.0f, 0.0f, 0.0f};
    Vector3 fieldMin_ {-1.0f, -1.0f, -1.0f};
    Vector3 fieldMax_ { 1.0f,  1.0f,  1.0f};
    float globalTime_ = 0.0f;
    std::mt19937 rng_{ std::random_device{}() };

    // 発生時のランダムパラメータ
    Vector3 spawnPosMin_{ 0.0f, 0.0f, 0.0f };
    Vector3 spawnPosMax_{ 0.0f, 0.0f, 0.0f };
    Vector3 velMin_{ 0.0f, 0.5f, 0.0f };
    Vector3 velMax_{ 0.0f, 0.5f, 0.0f };
    Vector3 scaleMin_{ 1.0f, 1.0f, 1.0f };
    Vector3 scaleMax_{ 1.0f, 1.0f, 1.0f };
    Vector4 colMin_{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector4 colMax_{ 1.0f, 1.0f, 1.0f, 1.0f };

    // 追加: 重力と減衰
    bool gravityEnabled_ = false;
    Vector3 gravity_{ 0.0f, -9.8f, 0.0f };
    float damping_ = 0.0f; // 速度の減衰係数（1/s）
};

} // namespace MyEngine
