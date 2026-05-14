#pragma once

#include "engine/2d/TextureManager.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/utility/mathUtility.h"
#include <list>
#include <random>
#include <string>
#include <unordered_map>

// CPU側のパーティクルデータ構造体
struct PM_CpuParticle {
    Math::Transform transform;
    Math::Vector3 velocity;
    Math::Vector4 color;
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
/// <summary>
/// パーティクルマネージャクラス
/// </summary>
class ParticleManager {
public: // メンバ関数
    /// <summary>
    /// シングルトンインスタンス取得
    /// </summary>
    static ParticleManager* GetInstance()
    {
        static ParticleManager instance;
        return &instance;
    }

    /// <summary>
    /// 初期化
    /// </summary>
    // Single Initialize: optional ImGuiManager parameter (default nullptr)
    void Initialize(DirectXCommon* dx, Object3dCommon* objCommon, SrvManager* srv, TextureManager* texMgr, class ImGuiManager* imguiManager = nullptr);

    // Finalize/unregister ImGui callbacks
    void Finalize();

    /// <summary>
    /// 描画に使用するプレーン（Object3d）を設定
    /// </summary>
    void SetParticlePlane(Object3d* plane);

    /// <summary>
    /// 新しいパーティクルグループを作成
    /// </summary>
    void CreateParticleGroup(const std::string& name, const std::string& textureFilePath);

    /// <summary>
    /// 既存グループにテクスチャを割り当て
    /// </summary>
    void SetGroupTexture(const std::string& name, const std::string& textureFilePath);

    /// <summary>
    /// 指定グループからパーティクルを生成
    /// </summary>
    void Emit(const std::string& name, const Math::Vector3& position, uint32_t count);

    /// <summary>
    /// パーティクルを更新する（位置・寿命・物理簡易処理）
    /// </summary>
    void Update(float dt);

    /// <summary>
    /// パーティクルを描画
    /// </summary>
    void Draw();

    // パーティクルグループの取得
    const std::unordered_map<std::string, ParticleGroup>& GetGroups() const { return particleGroups_; }

    // 生成時間範囲の取得
    void SetLifetimeRange(float minL, float maxL);

    // フィールド関連のセッター
    void SetFieldEnabled(bool enabled);
    void SetFieldAccel(const Math::Vector3& a);
    // フィールドのAABB範囲設定
    void SetFieldAABB(const Math::Vector3& mn, const Math::Vector3& mx);
    // 重力と減衰のセッター
    void SetGravityEnabled(bool enabled);
    void SetGravity(const Math::Vector3& g);
    void SetDamping(float d);

    // 発生時のランダム範囲設定
    void SetSpawnPosRange(const Math::Vector3& mn, const Math::Vector3& mx); // 発生位置のジッター
    void SetVelocityRange(const Math::Vector3& mn, const Math::Vector3& mx); // 初速の範囲
    void SetScaleRange(const Math::Vector3& mn, const Math::Vector3& mx); // 初期スケール範囲
    void SetColorRange(const Math::Vector4& mn, const Math::Vector4& mx); // 初期カラー範囲

    /// <summary>
    /// ImGui コントロールの構築（ParticleManager 固有の設定を編集するUI）
    /// </summary>
    void DrawImGui();

private: // メンバ関数
    ParticleManager() = default;

private: // メンバ変数
    // パーティクルグループ一覧（キー: グループ名）
    std::unordered_map<std::string, ParticleGroup> particleGroups_;
    // 参照: 外部がライフサイクルを管理するポインタ群
    DirectXCommon* dxCommon_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    TextureManager* texManager_ = nullptr;
    // 描画に使うプレーンモデル
    Object3d* particlePlane_ = nullptr;

    // 設定値
    float lifeMin_ = 1.0f;
    float lifeMax_ = 3.0f;
    bool fieldEnabled_ = false; // フィールド（ランダム加速度）有効フラグ
    Math::Vector3 fieldAccel_ { 0.0f, 0.0f, 0.0f }; // フィールド加速度
    Math::Vector3 fieldMin_ { -1.0f, -1.0f, -1.0f };
    Math::Vector3 fieldMax_ { 1.0f, 1.0f, 1.0f };
    float globalTime_ = 0.0f;
    std::mt19937 rng_ { std::random_device {}() };

    // 発生時のランダムパラメータ
    Math::Vector3 spawnPosMin_ { 0.0f, 0.0f, 0.0f }; // 発生位置最小値
    Math::Vector3 spawnPosMax_ { 0.0f, 0.0f, 0.0f }; // 発生位置最大値
    Math::Vector3 velMin_ { 0.0f, 0.5f, 0.0f }; // 初速最小値
    Math::Vector3 velMax_ { 0.0f, 0.5f, 0.0f }; // 初速最大値
    Math::Vector3 scaleMin_ { 1.0f, 1.0f, 1.0f }; // スケール最小値
    Math::Vector3 scaleMax_ { 1.0f, 1.0f, 1.0f }; // スケール最大値
    Math::Vector4 colMin_ { 1.0f, 1.0f, 1.0f, 1.0f }; // カラー最小値
    Math::Vector4 colMax_ { 1.0f, 1.0f, 1.0f, 1.0f }; // カラー最大値

    // 追加: 重力と減衰
    bool gravityEnabled_ = false; // 重力有効フラグ
    Math::Vector3 gravity_ { 0.0f, -9.8f, 0.0f }; // 重力ベクトル（m/s^2 想定）
    float damping_ = 0.0f; // 速度の減衰係数（1/s）

    // ImGuiManager への参照（オプション、ImGui コントロールの自動登録に使用）
    class ImGuiManager* imguiManager_ = nullptr;
};

} // namespace MyEngine
