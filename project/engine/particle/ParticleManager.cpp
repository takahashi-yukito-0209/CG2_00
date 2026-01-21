#include "ParticleManager.h"
#include "engine/utility/Logger.h"
#include "engine/base/DirectXCommon.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Model.h"
#include "engine/3d/Camera.h"
#include <algorithm>
#include <vector>
#include "externals/imgui/imgui.h"

using namespace MyEngine;

void ParticleManager::Initialize(MyEngine::DirectXCommon* dx,
                                 MyEngine::Object3dCommon* objCommon,
                                 MyEngine::SrvManager* srv,
                                 MyEngine::TextureManager* texMgr)
{
    dxCommon_ = dx;
    object3dCommon_ = objCommon;
    srvManager_ = srv;
    texManager_ = texMgr;
}

void ParticleManager::SetGroupTexture(const std::string& name, const std::string& textureFilePath)
{
    auto it = particleGroups_.find(name);
    if (it == particleGroups_.end()) return;
    it->second.texturePath = textureFilePath;
    if (texManager_) {
        uint32_t idx = texManager_->GetTextureIndexByFilePath(textureFilePath);
        if (idx == UINT32_MAX) {
            texManager_->LoadTexture(textureFilePath);
            texManager_->ExecuteResourceUpload();
            idx = texManager_->GetTextureIndexByFilePath(textureFilePath);
        }
        it->second.srvIndex = (idx == UINT32_MAX) ? 0u : idx;
    }
}

void ParticleManager::SetParticlePlane(MyEngine::Object3d* plane)
{
    particlePlane_ = plane;
}

void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath)
{
    if (name.empty()) { return; }
    auto& grp = particleGroups_[name];
    grp.texturePath = textureFilePath;
    // テクスチャを確実にロードしてSRVインデックスを記録
    if (texManager_) {
        uint32_t idx = texManager_->GetTextureIndexByFilePath(textureFilePath);
        if (idx == UINT32_MAX) {
            texManager_->LoadTexture(textureFilePath);
            texManager_->ExecuteResourceUpload();
            idx = texManager_->GetTextureIndexByFilePath(textureFilePath);
        }
        grp.srvIndex = (idx == UINT32_MAX) ? 0u : idx;
    }
}

void ParticleManager::Emit(const std::string& name, const Vector3& position, uint32_t count)
{
    auto it = particleGroups_.find(name);
    if (it == particleGroups_.end()) { return; }
    auto& list = it->second.particles;

    std::uniform_real_distribution<float> lifeDist(lifeMin_, lifeMax_);
    std::uniform_real_distribution<float> rx(spawnPosMin_.x, spawnPosMax_.x);
    std::uniform_real_distribution<float> ry(spawnPosMin_.y, spawnPosMax_.y);
    std::uniform_real_distribution<float> rz(spawnPosMin_.z, spawnPosMax_.z);
    std::uniform_real_distribution<float> rvx(velMin_.x, velMax_.x);
    std::uniform_real_distribution<float> rvy(velMin_.y, velMax_.y);
    std::uniform_real_distribution<float> rvz(velMin_.z, velMax_.z);
    std::uniform_real_distribution<float> rsx(scaleMin_.x, scaleMax_.x);
    std::uniform_real_distribution<float> rsy(scaleMin_.y, scaleMax_.y);
    std::uniform_real_distribution<float> rsz(scaleMin_.z, scaleMax_.z);
    std::uniform_real_distribution<float> rcx(colMin_.x, colMax_.x);
    std::uniform_real_distribution<float> rcy(colMin_.y, colMax_.y);
    std::uniform_real_distribution<float> rcz(colMin_.z, colMax_.z);
    std::uniform_real_distribution<float> rca(colMin_.w, colMax_.w);

    for (uint32_t i = 0; i < count; ++i) {
        PM_CpuParticle p{};
        p.transform.scale = { rsx(rng_), rsy(rng_), rsz(rng_) };
        p.transform.rotate = { 0.0f, 0.0f, 0.0f };
        p.transform.translate = { position.x + rx(rng_), position.y + ry(rng_), position.z + rz(rng_) };
        p.velocity = { rvx(rng_), rvy(rng_), rvz(rng_) };
        p.color = { rcx(rng_), rcy(rng_), rcz(rng_), rca(rng_) };
        float life = lifeDist(rng_);
        if (life < 0.01f) life = 0.01f;
        p.lifeTime = life;
        p.currentTime = 0.0f;
        p.spawnTime = globalTime_;
        list.push_back(p);
    }
}

void ParticleManager::Update(float dt)
{
    globalTime_ += dt;
    for (auto& kv : particleGroups_) {
        auto& plist = kv.second.particles;
        for (auto it = plist.begin(); it != plist.end(); ) {
            PM_CpuParticle& p = *it;
            // フィールド適用（AABB内）
            if (fieldEnabled_) {
                const Vector3& pos = p.transform.translate;
                if (pos.x >= fieldMin_.x && pos.y >= fieldMin_.y && pos.z >= fieldMin_.z &&
                    pos.x <= fieldMax_.x && pos.y <= fieldMax_.y && pos.z <= fieldMax_.z) {
                    p.velocity.x += fieldAccel_.x * dt;
                    p.velocity.y += fieldAccel_.y * dt;
                    p.velocity.z += fieldAccel_.z * dt;
                }
            }
            // 重力
            if (gravityEnabled_) {
                p.velocity.x += gravity_.x * dt;
                p.velocity.y += gravity_.y * dt;
                p.velocity.z += gravity_.z * dt;
            }
            // 減衰（一次減衰）
            if (damping_ > 0.0f) {
                float k = damping_ * dt;
                p.velocity.x *= (1.0f - k);
                p.velocity.y *= (1.0f - k);
                p.velocity.z *= (1.0f - k);
            }
            // 位置更新
            p.transform.translate.x += p.velocity.x * dt;
            p.transform.translate.y += p.velocity.y * dt;
            p.transform.translate.z += p.velocity.z * dt;

            // 経過時間
            p.currentTime += dt;
            if (p.currentTime >= p.lifeTime) {
                it = plist.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void ParticleManager::Draw()
{
    if (!dxCommon_ || !object3dCommon_ || !particlePlane_) { return; }

    // PSO/RS 切り替え（パーティクル用）
    object3dCommon_->SetInstancingDrawSetting();

    // カメラ取得（非ビルボード時のWVP計算用）
    Camera* cam = object3dCommon_->GetDefaultCamera();
    Matrix4x4 view = cam ? cam->GetViewMatrix() : Matrix4x4();
    Matrix4x4 proj = cam ? cam->GetProjectionMatrix() : Matrix4x4();

    // インスタンシングバッファ
    auto instData = object3dCommon_->GetInstancingData();
    const uint32_t instSlots = object3dCommon_->GetInstancingSlotCount();
    if (!instData || instSlots == 0) { return; }

    MathUtility math;

    for (auto& kv : particleGroups_) {
        auto& grp = kv.second;
        // 描画個数を決定
        uint32_t count = static_cast<uint32_t>(grp.particles.size());
        if (count == 0) { continue; }
        count = std::min<uint32_t>(count, instSlots);

        // 安定ソート用に参照配列を作る
        std::vector<std::reference_wrapper<const PM_CpuParticle>> sorted;
        sorted.reserve(grp.particles.size());
        for (const auto& p : grp.particles) { sorted.emplace_back(std::cref(p)); }
        std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            return a.get().spawnTime < b.get().spawnTime;
        });

        // パーティクルプレーンにテクスチャ適用
        if (!grp.texturePath.empty()) {
            particlePlane_->SetTexture(grp.texturePath);
        }

        // インスタンスデータ転送
        for (uint32_t i = 0; i < count; ++i) {
            const auto& pt = sorted[i].get();
            Transform tr = pt.transform;
            // ZオフセットでZ Fighting軽減
            tr.translate.z += static_cast<float>(i) * 1e-3f;
            Matrix4x4 world = math.MakeAffineMatrix(tr.scale, tr.rotate, tr.translate);
            Matrix4x4 wvp = math.Multiply(world, math.Multiply(view, proj));
            Matrix4x4 inv = math.Inverse(world);
            Matrix4x4 invT = math.Transpose(inv);
            instData[i].World = world;
            instData[i].WVP = wvp;
            instData[i].WorldInverseTranspose = invT;
            instData[i].color = pt.color;
        }

        // インスタンス描画
        if (auto* model = particlePlane_->GetModel()) {
            model->DrawInstanced(particlePlane_, count);
        } else {
            // モデルが無い場合は通常描画で1枚だけ（安全策）
            particlePlane_->Draw();
        }
    }
}

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

void ParticleManager::SetFieldEnabled(bool enabled)
{
    fieldEnabled_ = enabled;
}

void ParticleManager::SetFieldAccel(const Vector3& a)
{
    fieldAccel_ = a;
}

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

void ParticleManager::DrawImGui()
{
    ImGui::Text("Groups: %zu", particleGroups_.size());
    // Show some global settings
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
    ImGui::DragFloat("Damping (1/s)", &damping_, 0.01f, 0.0f, 10.0f);

    ImGui::Separator();
    ImGui::Text("Spawn Ranges");
    ImGui::DragFloat3("Pos Min", &spawnPosMin_.x, 0.01f, -10.0f, 10.0f);
    ImGui::DragFloat3("Pos Max", &spawnPosMax_.x, 0.01f, -10.0f, 10.0f);
    ImGui::DragFloat3("Vel Min", &velMin_.x, 0.01f, -50.0f, 50.0f);
    ImGui::DragFloat3("Vel Max", &velMax_.x, 0.01f, -50.0f, 50.0f);
    ImGui::DragFloat3("Scale Min", &scaleMin_.x, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat3("Scale Max", &scaleMax_.x, 0.01f, 0.01f, 10.0f);
    // Color ranges (min/max)
    ImGui::ColorEdit4("Color Min", &colMin_.x);
    ImGui::ColorEdit4("Color Max", &colMax_.x);

    // List groups
    ImGui::Separator();
    ImGui::Text("Groups");
    for (auto& kv : particleGroups_) {
        if (ImGui::TreeNode(kv.first.c_str())) {
            ImGui::Text("Count = %zu", kv.second.particles.size());
            ImGui::TreePop();
        }
    }
}
