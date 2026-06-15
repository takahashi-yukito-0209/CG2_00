#include "ParticleManager.h"
#include "engine/utility/Logger.h"
#include "engine/base/DirectXCommon.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Model.h"
#include "engine/3d/Camera.h"
#include "ImGuiManager.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <vector>

using namespace Math;
using namespace MyEngine;

/// <summary>
/// パーティクルマネージャーを初期化する
/// </summary>
void ParticleManager::Initialize(DirectXCommon* dx, Object3dCommon* objCommon, SrvManager* srv, TextureManager* texMgr, ImGuiManager* imguiManager)
{
    dxCommon_ = dx;
    object3dCommon_ = objCommon;
    srvManager_ = srv;
    texManager_ = texMgr;
    imguiManager_ = imguiManager;
}

/// <summary>
/// パーティクルマネージャーを終了する
/// </summary>
void ParticleManager::Finalize()
{
    particleGroups_.clear();
}

/// <summary>
/// 既存グループにテクスチャを割り当てる
/// </summary>
void ParticleManager::SetGroupTexture(const std::string& name, const std::string& textureFilePath)
{
    auto it = particleGroups_.find(name);
    if (it == particleGroups_.end()) {
        return;
    }

    it->second.texturePath = textureFilePath;
    if (texManager_) {
        uint32_t index = texManager_->GetTextureIndexByFilePath(textureFilePath); // テクスチャ番号
        if (index == UINT32_MAX) {
            texManager_->LoadTexture(textureFilePath);
            texManager_->ExecuteResourceUpload();
            index = texManager_->GetTextureIndexByFilePath(textureFilePath);
        }
        it->second.srvIndex = (index == UINT32_MAX) ? 0u : index;
    }
}

/// <summary>
/// 描画に使用するプレーンを設定する
/// </summary>
void ParticleManager::SetParticlePlane(Object3d* plane)
{
    particlePlane_ = plane;
}

/// <summary>
/// 新しいパーティクルグループを作成する
/// </summary>
void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath)
{
    if (name.empty()) {
        return;
    }

    auto& group = particleGroups_[name]; // 作成または更新するグループ
    group.texturePath = textureFilePath;
    group.particles.clear();

    if (texManager_) {
        uint32_t index = texManager_->GetTextureIndexByFilePath(textureFilePath); // テクスチャ番号
        if (index == UINT32_MAX) {
            texManager_->LoadTexture(textureFilePath);
            texManager_->ExecuteResourceUpload();
            index = texManager_->GetTextureIndexByFilePath(textureFilePath);
        }
        group.srvIndex = (index == UINT32_MAX) ? 0u : index;
    }
}

/// <summary>
/// 通常パーティクルを生成する
/// </summary>
void ParticleManager::Emit(const std::string& name, const Vector3& position, uint32_t count)
{
    auto it = particleGroups_.find(name);
    if (it == particleGroups_.end()) {
        return;
    }

    auto& list = it->second.particles; // 生成先のパーティクルリスト
    std::uniform_real_distribution<float> lifeDist(lifeMin_, lifeMax_); // 寿命範囲
    std::uniform_real_distribution<float> rx(spawnPosMin_.x, spawnPosMax_.x); // X位置範囲
    std::uniform_real_distribution<float> ry(spawnPosMin_.y, spawnPosMax_.y); // Y位置範囲
    std::uniform_real_distribution<float> rz(spawnPosMin_.z, spawnPosMax_.z); // Z位置範囲
    std::uniform_real_distribution<float> rvx(velMin_.x, velMax_.x); // X速度範囲
    std::uniform_real_distribution<float> rvy(velMin_.y, velMax_.y); // Y速度範囲
    std::uniform_real_distribution<float> rvz(velMin_.z, velMax_.z); // Z速度範囲
    std::uniform_real_distribution<float> rsx(scaleMin_.x, scaleMax_.x); // Xスケール範囲
    std::uniform_real_distribution<float> rsy(scaleMin_.y, scaleMax_.y); // Yスケール範囲
    std::uniform_real_distribution<float> rsz(scaleMin_.z, scaleMax_.z); // Zスケール範囲
    std::uniform_real_distribution<float> rcx(colMin_.x, colMax_.x); // R範囲
    std::uniform_real_distribution<float> rcy(colMin_.y, colMax_.y); // G範囲
    std::uniform_real_distribution<float> rcz(colMin_.z, colMax_.z); // B範囲
    std::uniform_real_distribution<float> rca(colMin_.w, colMax_.w); // A範囲

    for (uint32_t i = 0; i < count; ++i) {
        PM_CpuParticle particle {}; // 生成するパーティクル
        particle.transform.scale = { rsx(rng_), rsy(rng_), rsz(rng_) };
        particle.startScale = particle.transform.scale;
        particle.endScale = particle.transform.scale;
        particle.transform.rotate = { 0.0f, 0.0f, 0.0f };
        particle.transform.translate = { position.x + rx(rng_), position.y + ry(rng_), position.z + rz(rng_) };
        particle.velocity = { rvx(rng_), rvy(rng_), rvz(rng_) };
        particle.color = { rcx(rng_), rcy(rng_), rcz(rng_), rca(rng_) };
        particle.startColor = particle.color;
        particle.lifeTime = lifeDist(rng_);
        particle.currentTime = 0.0f;
        particle.spawnTime = globalTime_;

        list.push_back(particle);
    }
}

/// <summary>
/// ヒットエフェクト用の細長いパーティクルを生成する
/// </summary>
void ParticleManager::EmitHitEffect(const std::string& name, const Vector3& position, uint32_t count)
{
    auto it = particleGroups_.find(name);
    if (it == particleGroups_.end()) {
        return;
    }

    auto& list = it->second.particles; // 生成先のパーティクルリスト
    std::uniform_real_distribution<float> rotateDist(-std::numbers::pi_v<float>, std::numbers::pi_v<float>); // Z回転範囲
    std::uniform_real_distribution<float> scaleYDist(0.9f, 1.8f); // 縦方向スケール範囲
    std::uniform_real_distribution<float> alphaDist(0.75f, 1.0f); // 透明度範囲

    for (uint32_t i = 0; i < count; ++i) {
        const float length = scaleYDist(rng_); // 光の筋の最大長さ
        const float alpha = alphaDist(rng_); // 発生時の透明度

        PM_CpuParticle particle {}; // 生成するパーティクル
        particle.startScale = { 0.01f, length * 0.15f, 1.0f };
        particle.endScale = { 0.045f, length, 1.0f };
        particle.transform.scale = particle.startScale;
        particle.transform.rotate = { 0.0f, 0.0f, rotateDist(rng_) };
        particle.transform.translate = position;
        particle.velocity = { 0.0f, 0.0f, 0.0f };
        particle.color = { 1.0f, 1.0f, 1.0f, alpha };
        particle.startColor = particle.color;
        particle.lifeTime = 0.6f;
        particle.currentTime = 0.0f;
        particle.spawnTime = globalTime_;
        particle.useScaleOverLife = true;
        particle.useFadeOut = true;

        list.push_back(particle);
    }
}

/// <summary>
/// パーティクルを更新する
/// </summary>
void ParticleManager::Update(float dt)
{
    globalTime_ += dt;

    for (auto& kv : particleGroups_) {
        auto& particles = kv.second.particles; // 更新するパーティクルリスト
        for (auto it = particles.begin(); it != particles.end();) {
            PM_CpuParticle& particle = *it; // 更新対象のパーティクル

            if (fieldEnabled_) {
                const Vector3& position = particle.transform.translate; // 現在位置
                if (position.x >= fieldMin_.x && position.y >= fieldMin_.y && position.z >= fieldMin_.z &&
                    position.x <= fieldMax_.x && position.y <= fieldMax_.y && position.z <= fieldMax_.z) {
                    particle.velocity.x += fieldAccel_.x * dt;
                    particle.velocity.y += fieldAccel_.y * dt;
                    particle.velocity.z += fieldAccel_.z * dt;
                }
            }

            if (gravityEnabled_) {
                particle.velocity.x += gravity_.x * dt;
                particle.velocity.y += gravity_.y * dt;
                particle.velocity.z += gravity_.z * dt;
            }

            if (damping_ > 0.0f) {
                const float dampingRate = std::clamp(1.0f - damping_ * dt, 0.0f, 1.0f); // 減衰率
                particle.velocity.x *= dampingRate;
                particle.velocity.y *= dampingRate;
                particle.velocity.z *= dampingRate;
            }

            particle.transform.translate.x += particle.velocity.x * dt;
            particle.transform.translate.y += particle.velocity.y * dt;
            particle.transform.translate.z += particle.velocity.z * dt;

            particle.currentTime += dt;
            const float lifeRate = particle.lifeTime > 0.0f ? std::clamp(particle.currentTime / particle.lifeTime, 0.0f, 1.0f) : 1.0f; // 寿命の進行率
            if (particle.useScaleOverLife) {
                const float scaleRate = std::sin(lifeRate * std::numbers::pi_v<float>); // 中間で最大になる倍率
                particle.transform.scale.x = particle.startScale.x + (particle.endScale.x - particle.startScale.x) * scaleRate;
                particle.transform.scale.y = particle.startScale.y + (particle.endScale.y - particle.startScale.y) * scaleRate;
                particle.transform.scale.z = particle.startScale.z + (particle.endScale.z - particle.startScale.z) * scaleRate;
            }
            if (particle.useFadeOut) {
                particle.color.w = particle.startColor.w * (1.0f - lifeRate);
            }

            if (particle.currentTime >= particle.lifeTime) {
                it = particles.erase(it);
            } else {
                ++it;
            }
        }
    }
}

/// <summary>
/// パーティクルを描画する
/// </summary>
void ParticleManager::Draw()
{
    if (!dxCommon_ || !object3dCommon_ || !particlePlane_) {
        return;
    }

    object3dCommon_->SetInstancingDrawSetting();

    Camera* camera = object3dCommon_->GetDefaultCamera(); // 描画に使うカメラ
    Matrix4x4 view = camera ? camera->GetViewMatrix() : Matrix4x4();
    Matrix4x4 projection = camera ? camera->GetProjectionMatrix() : Matrix4x4();

    auto instancingData = object3dCommon_->GetInstancingData(); // インスタンス転送先
    const uint32_t instancingSlots = object3dCommon_->GetInstancingSlotCount(); // 最大描画数
    if (!instancingData || instancingSlots == 0) {
        return;
    }

    for (auto& kv : particleGroups_) {
        ParticleGroup& group = kv.second; // 描画対象グループ
        uint32_t count = static_cast<uint32_t>(group.particles.size());
        if (count == 0) {
            continue;
        }
        count = std::min<uint32_t>(count, instancingSlots);

        std::vector<std::reference_wrapper<const PM_CpuParticle>> sortedParticles; // 生成順に並べる参照配列
        sortedParticles.reserve(group.particles.size());
        for (const auto& particle : group.particles) {
            sortedParticles.emplace_back(std::cref(particle));
        }
        std::stable_sort(sortedParticles.begin(), sortedParticles.end(), [](const auto& a, const auto& b) {
            return a.get().spawnTime < b.get().spawnTime;
        });

        if (!group.texturePath.empty()) {
            particlePlane_->SetTexture(group.texturePath);
        }

        for (uint32_t i = 0; i < count; ++i) {
            const PM_CpuParticle& particle = sortedParticles[i].get(); // 転送するパーティクル
            Transform transform = particle.transform;
            transform.translate.z += static_cast<float>(i) * 1e-3f;

            Matrix4x4 world = MathUtil::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
            Matrix4x4 wvp = MathUtil::Multiply(world, MathUtil::Multiply(view, projection));
            Matrix4x4 worldInverse = MathUtil::Inverse(world);
            Matrix4x4 worldInverseTranspose = MathUtil::Transpose(worldInverse);

            instancingData[i].World = world;
            instancingData[i].WVP = wvp;
            instancingData[i].WorldInverseTranspose = worldInverseTranspose;
            instancingData[i].color = particle.color;
        }

        if (auto* model = particlePlane_->GetModel()) {
            model->DrawInstanced(particlePlane_, count);
        } else {
            particlePlane_->DrawInstanced(count);
        }
    }
}

/// <summary>
/// 寿命の範囲を設定する
/// </summary>
void ParticleManager::SetLifetimeRange(float minL, float maxL)
{
    if (minL <= maxL) {
        lifeMin_ = minL;
        lifeMax_ = maxL;
    } else {
        lifeMin_ = maxL;
        lifeMax_ = minL;
    }
}

/// <summary>
/// フィールド有効フラグを設定する
/// </summary>
void ParticleManager::SetFieldEnabled(bool enabled) { fieldEnabled_ = enabled; }

/// <summary>
/// フィールド加速度を設定する
/// </summary>
void ParticleManager::SetFieldAccel(const Vector3& a) { fieldAccel_ = a; }

/// <summary>
/// フィールドの影響範囲を設定する
/// </summary>
void ParticleManager::SetFieldAABB(const Vector3& mn, const Vector3& mx)
{
    fieldMin_ = mn;
    fieldMax_ = mx;
}

void ParticleManager::SetSpawnPosRange(const Vector3& mn, const Vector3& mx) { spawnPosMin_ = mn; spawnPosMax_ = mx; }
void ParticleManager::SetVelocityRange(const Vector3& mn, const Vector3& mx) { velMin_ = mn; velMax_ = mx; }
void ParticleManager::SetScaleRange(const Vector3& mn, const Vector3& mx) { scaleMin_ = mn; scaleMax_ = mx; }
void ParticleManager::SetColorRange(const Vector4& mn, const Vector4& mx) { colMin_ = mn; colMax_ = mx; }

void ParticleManager::SetGravityEnabled(bool enabled) { gravityEnabled_ = enabled; }
void ParticleManager::SetGravity(const Vector3& g) { gravity_ = g; }
void ParticleManager::SetDamping(float d) { damping_ = d < 0.0f ? 0.0f : d; }

/// <summary>
/// ImGuiでパーティクル設定を編集する
/// </summary>
void ParticleManager::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Groups: %zu", particleGroups_.size());
    ImGui::Checkbox("Enable Field", &fieldEnabled_);
    ImGui::DragFloat3("Field Accel", &fieldAccel_.x, 0.1f, -100.0f, 100.0f);
    ImGui::DragFloat3("Field Min", &fieldMin_.x, 0.1f, -100.0f, 100.0f);
    ImGui::DragFloat3("Field Max", &fieldMax_.x, 0.1f, -100.0f, 100.0f);

    ImGui::Separator();
    ImGui::Text("Lifetime");
    ImGui::DragFloatRange2("Life Min/Max", &lifeMin_, &lifeMax_, 0.01f, 0.1f, 100.0f);

    ImGui::Separator();
    ImGui::Text("Dynamics");
    ImGui::Checkbox("Enable Gravity", &gravityEnabled_);
    ImGui::DragFloat3("Gravity", &gravity_.x, 0.1f, -100.0f, 100.0f);
    ImGui::DragFloat("Damping", &damping_, 0.01f, 0.0f, 100.0f);

    ImGui::Separator();
    ImGui::Text("Spawn Random");
    ImGui::DragFloat3("Spawn Pos Min", &spawnPosMin_.x, 0.01f, -50.0f, 50.0f);
    ImGui::DragFloat3("Spawn Pos Max", &spawnPosMax_.x, 0.01f, -50.0f, 50.0f);
    ImGui::DragFloat3("Vel Min", &velMin_.x, 0.01f, -50.0f, 50.0f);
    ImGui::DragFloat3("Vel Max", &velMax_.x, 0.01f, -50.0f, 50.0f);
    ImGui::DragFloat3("Scale Min", &scaleMin_.x, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat3("Scale Max", &scaleMax_.x, 0.01f, 0.01f, 10.0f);

    ImGui::ColorEdit4("Color Min", &colMin_.x);
    ImGui::ColorEdit4("Color Max", &colMax_.x);

    ImGui::Separator();
    ImGui::Text("Groups");
    for (auto& kv : particleGroups_) {
        if (ImGui::TreeNode(kv.first.c_str())) {
            ImGui::Text("Count = %zu", kv.second.particles.size());
            ImGui::TreePop();
        }
    }
#else
    (void)particleGroups_;
    (void)fieldEnabled_;
    (void)fieldAccel_;
    (void)fieldMin_;
    (void)fieldMax_;
    (void)lifeMin_;
    (void)lifeMax_;
    (void)gravityEnabled_;
    (void)gravity_;
    (void)damping_;
    (void)spawnPosMin_;
    (void)spawnPosMax_;
    (void)velMin_;
    (void)velMax_;
    (void)scaleMin_;
    (void)scaleMax_;
    (void)colMin_;
    (void)colMax_;
#endif
}
