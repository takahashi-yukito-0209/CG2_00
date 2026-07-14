#include "ParticleManager.h"
#include "ImGuiManager.h"
#include "engine/3d/Camera.h"
#include "engine/3d/Model.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/base/DirectXCommon.h"
#include "externals/DirectXTex/d3dx12.h"
#include "engine/utility/Logger.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

using namespace Math;
using namespace MyEngine;

namespace {
constexpr uint32_t kFallbackParticleLimit = 1024; // 初期化前に参照された場合の最低限の保持上限
constexpr float kImGuiFineStep = 0.01f; // 細かい値の調整幅
constexpr float kImGuiPhysicsStep = 0.1f; // 物理系値の調整幅
constexpr float kImGuiLifeMin = 0.1f; // 寿命設定の最小値
constexpr float kImGuiLifeMax = 100.0f; // 寿命設定の最大値
constexpr float kImGuiSpawnPositionMin = -50.0f; // 発生位置範囲の最小値
constexpr float kImGuiSpawnPositionMax = 50.0f; // 発生位置範囲の最大値
constexpr float kImGuiScaleMin = 0.01f; // スケール範囲の最小値
constexpr float kImGuiScaleMax = 10.0f; // スケール範囲の最大値
constexpr float kImGuiPhysicsMin = -100.0f; // 物理系値の最小値
constexpr float kImGuiPhysicsMax = 100.0f; // 物理系値の最大値
constexpr float kImGuiDampingMin = 0.0f; // 減衰率の最小値
constexpr float kImGuiDampingMax = 100.0f; // 減衰率の最大値
constexpr float kHitLengthMin = 0.9f; // ヒットエフェクトの最小長さ
constexpr float kHitLengthMax = 1.8f; // ヒットエフェクトの最大長さ
constexpr float kHitAlphaMin = 0.75f; // ヒットエフェクト透明度の最小値
constexpr float kHitAlphaMax = 1.0f; // ヒットエフェクト透明度の最大値
constexpr float kHitStartScaleX = 0.01f; // ヒットエフェクト開始時の横幅
constexpr float kHitStartScaleYRate = 0.15f; // ヒットエフェクト開始時の縦幅係数
constexpr float kHitEndScaleX = 0.045f; // ヒットエフェクト終了時の横幅
constexpr float kParticleScaleZ = 1.0f; // パーティクルのZスケール
constexpr float kHitLifeTime = 0.6f; // ヒットエフェクトの寿命
constexpr float kSpaceCrackStartWidthRate = 0.15f; // 空間亀裂開始時の幅倍率
constexpr float kSpaceCrackStartLengthRate = 0.2f; // 空間亀裂開始時の長さ倍率
constexpr float kSpaceCrackLifeTimeMin = 0.01f; // 空間亀裂寿命の最小値
constexpr float kParticleTimeStart = 0.0f; // パーティクル経過時間の初期値
constexpr float kParticleRateMin = 0.0f; // パーティクル進行率の最小値
constexpr float kParticleRateMax = 1.0f; // パーティクル進行率の最大値
constexpr float kParticleFadeAlphaBase = 1.0f; // フェードアウト透明度の基準値
constexpr float kParticleLifeTimeCheckMin = 0.0f; // 寿命率計算を行う寿命の下限
constexpr float kDampingEnabledThreshold = 0.0f; // 減衰を適用する下限値
constexpr float kBoundsCenterRate = 0.5f; // 範囲の中心位置を求める倍率
constexpr Vector3 kParticleZeroVector = { 0.0f, 0.0f, 0.0f }; // パーティクルのゼロベクトル
constexpr Vector3 kHitColorRgb = { 1.0f, 1.0f, 1.0f }; // ヒットエフェクトの基本色
constexpr Vector3 kRingStartScale = { 0.2f, 0.2f, kParticleScaleZ }; // Ringエフェクト開始時のスケール
constexpr Vector3 kRingEndScale = { 1.6f, 1.6f, kParticleScaleZ }; // Ringエフェクト終了時のスケール
constexpr Vector4 kRingColor = { 1.0f, 1.0f, 1.0f, 0.75f }; // Ringエフェクトの色
constexpr float kRingLifeTime = 0.8f; // Ringエフェクトの寿命
constexpr Vector3 kCylinderStartScale = { 0.35f, 0.7f, 0.35f }; // Cylinderエフェクト開始時のスケール
constexpr Vector3 kCylinderEndScale = { 1.2f, 0.9f, 1.2f }; // Cylinderエフェクト終了時のスケール
constexpr Vector4 kCylinderColor = { 0.55f, 0.75f, 1.0f, 0.55f }; // Cylinderエフェクトの色
constexpr float kCylinderLifeTime = 1.0f; // Cylinderエフェクトの寿命
constexpr float kRiftRingScaleOffsetStep = 0.08f; // Riftリングごとのスケール差
constexpr float kRiftRingLifeOffsetStep = 0.04f; // Riftリングごとの寿命差
constexpr float kRiftRingLifeTimeMin = 0.01f; // Riftリング寿命の最小値
constexpr float kRiftRingSpawnDelayStep = 0.001f; // Riftリングごとの発生時間差
constexpr float kRiftFragmentLengthMin = 0.45f; // Rift破片の最小長さ
constexpr float kRiftFragmentLengthMax = 1.15f; // Rift破片の最大長さ
constexpr float kRiftFragmentStartWidth = 0.025f; // Rift破片開始時の幅
constexpr float kRiftFragmentEndWidth = 0.01f; // Rift破片終了時の幅
constexpr float kRiftFragmentEndLengthRate = 0.35f; // Rift破片終了時の長さ倍率
constexpr float kRiftFragmentLifeTimeMin = 0.01f; // Rift破片寿命の最小値
constexpr float kRiftFragmentRotationOffset = std::numbers::pi_v<float> * 0.5f;
constexpr uint32_t kGpuParticleThreadCount = 1024; // ComputeShaderの1グループあたりのスレッド数
constexpr uint32_t kComputeRootParameterSourceSrv = 0; // Compute入力SRVのルート番号
constexpr uint32_t kComputeRootParameterOutputUav = 1; // Compute出力UAVのルート番号
constexpr uint32_t kComputeRootParameterInfoCbv = 2; // Compute定数バッファのルート番号 // Rift破片の向き補正
constexpr uint32_t kComputeRootParameterEmitterCbv = 3; // GPU Emitter用CBVのルート番号
constexpr uint32_t kComputeRootParameterPerFrameCbv = 4; // GPU Emitter用フレームCBVのルート番号
constexpr uint32_t kComputeRootParameterFreeCounterUav = 5; // GPU Emitter用Counter UAVのルート番号
}

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
    InitializeGpuParticleResources();
}

/// <summary>
/// パーティクルマネージャーを終了する
/// </summary>
void ParticleManager::Finalize()
{
    particleGroups_.clear();
    instancingLimitWarnedGroups_.clear();
    totalParticleCount_ = 0;
    FinalizeGpuParticleResources();
}

/// <summary>
/// 保持できるパーティクル数の上限を取得する
/// </summary>
uint32_t ParticleManager::GetParticleLimit() const
{
    if (object3dCommon_) {
        const uint32_t instancingSlotCount = object3dCommon_->GetInstancingSlotCount(); // GPUへ転送できる最大インスタンス数
        if (instancingSlotCount > 0) {
            return instancingSlotCount;
        }
    }

    return kFallbackParticleLimit;
}

/// <summary>
/// 現在の保持数を考慮して実際に生成できるパーティクル数を取得する
/// </summary>
uint32_t ParticleManager::GetEmitCountWithinLimit(const ParticleGroup& group, uint32_t requestCount) const
{
    const uint32_t particleLimit = GetParticleLimit(); // 全体と1グループで保持する最大数
    const size_t currentGroupCount = group.particles.size(); // 対象グループで保持しているパーティクル数
    const size_t currentTotalCount = totalParticleCount_; // 全グループで保持しているパーティクル数

    if (currentGroupCount >= static_cast<size_t>(particleLimit) || currentTotalCount >= static_cast<size_t>(particleLimit)) {
        return 0;
    }

    const uint32_t groupRemainingCount = particleLimit - static_cast<uint32_t>(currentGroupCount); // 対象グループで追加生成できる残り数
    const uint32_t totalRemainingCount = particleLimit - static_cast<uint32_t>(currentTotalCount); // 全体で追加生成できる残り数
    return std::min<uint32_t>(requestCount, std::min<uint32_t>(groupRemainingCount, totalRemainingCount));
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
            index = texManager_->GetTextureIndexByFilePath(textureFilePath);
        }
        it->second.srvIndex = (index == UINT32_MAX) ? 0u : index;
    }

    if (it->second.renderObject) {
        it->second.renderObject->SetTexture(textureFilePath);
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
/// 指定グループの描画Primitiveを設定する
/// </summary>
void ParticleManager::SetParticleObject(const std::string& name, Object3d* object)
{
    auto it = particleGroups_.find(name);
    if (it == particleGroups_.end()) {
        return;
    }

    it->second.renderObject = object;
    if (object && !it->second.texturePath.empty()) {
        object->SetTexture(it->second.texturePath);
    }
}

/// <summary>
/// 指定グループのビルボード使用設定を変更する
/// </summary>
void ParticleManager::SetGroupBillboard(const std::string& name, bool useBillboard)
{
    auto it = particleGroups_.find(name);
    if (it == particleGroups_.end()) {
        return;
    }

    it->second.useBillboard = useBillboard;
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
    totalParticleCount_ -= group.particles.size();
    group.texturePath = textureFilePath;
    group.particles.clear();

    if (texManager_) {
        uint32_t index = texManager_->GetTextureIndexByFilePath(textureFilePath); // テクスチャ番号
        if (index == UINT32_MAX) {
            texManager_->LoadTexture(textureFilePath);
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

    auto& particles = it->second.particles; // 生成先のパーティクル配列
    const uint32_t emitCount = GetEmitCountWithinLimit(it->second, count); // 上限を考慮した実際の生成数
    if (emitCount == 0) {
        return;
    }
    particles.reserve(particles.size() + emitCount);

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

    for (uint32_t i = 0; i < emitCount; ++i) {
        PM_CpuParticle particle {}; // 生成するパーティクル
        particle.transform.scale = { rsx(rng_), rsy(rng_), rsz(rng_) };
        particle.startScale = particle.transform.scale;
        particle.endScale = particle.transform.scale;
        particle.transform.rotate = kParticleZeroVector;
        particle.transform.translate = { position.x + rx(rng_), position.y + ry(rng_), position.z + rz(rng_) };
        particle.velocity = { rvx(rng_), rvy(rng_), rvz(rng_) };
        particle.color = { rcx(rng_), rcy(rng_), rcz(rng_), rca(rng_) };
        particle.startColor = particle.color;
        particle.lifeTime = lifeDist(rng_);
        particle.currentTime = kParticleTimeStart;
        particle.spawnTime = globalTime_;

        particles.push_back(particle);
    }
    totalParticleCount_ += emitCount;
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

    auto& particles = it->second.particles; // 生成先のパーティクル配列
    const uint32_t emitCount = GetEmitCountWithinLimit(it->second, count); // 上限を考慮した実際の生成数
    if (emitCount == 0) {
        return;
    }
    particles.reserve(particles.size() + emitCount);

    std::uniform_real_distribution<float> rotateDist(-std::numbers::pi_v<float>, std::numbers::pi_v<float>); // Z回転範囲
    std::uniform_real_distribution<float> scaleYDist(
        kHitLengthMin,
        kHitLengthMax); // 縦方向スケール範囲
    std::uniform_real_distribution<float> alphaDist(kHitAlphaMin, kHitAlphaMax); // 透明度範囲

    for (uint32_t i = 0; i < emitCount; ++i) {
        const float length = scaleYDist(rng_); // 光の筋の最大長さ
        const float alpha = alphaDist(rng_); // 発生時の透明度

        PM_CpuParticle particle {}; // 生成するパーティクル
        particle.startScale = {
            kHitStartScaleX,
            length * kHitStartScaleYRate,
            kParticleScaleZ
        };
        particle.endScale = { kHitEndScaleX, length, kParticleScaleZ };
        particle.transform.scale = particle.startScale;
        particle.transform.rotate = { 0.0f, 0.0f, rotateDist(rng_) };
        particle.transform.translate = position;
        particle.velocity = kParticleZeroVector;
        particle.color = {
            kHitColorRgb.x,
            kHitColorRgb.y,
            kHitColorRgb.z,
            alpha
        };
        particle.startColor = particle.color;
        particle.lifeTime = kHitLifeTime;
        particle.currentTime = kParticleTimeStart;
        particle.spawnTime = globalTime_;
        particle.useScaleOverLife = true;
        particle.useFadeOut = true;

        particles.push_back(particle);
    }
    totalParticleCount_ += emitCount;
}

/// <summary>
/// 指定した形状で空間亀裂用のパーティクルを生成する
/// </summary>
void ParticleManager::EmitSpaceCrack(
    const std::string& name,
    const Vector3& position,
    float rotationZ,
    float length,
    float width,
    const Vector4& color,
    float lifeTime)
{
    auto groupIterator = particleGroups_.find(name); // 亀裂を追加するパーティクルグループ
    if (groupIterator == particleGroups_.end()) {
        return;
    }

    const uint32_t emitCount = GetEmitCountWithinLimit(groupIterator->second, 1); // 実際に生成できる亀裂数
    if (emitCount == 0) {
        return;
    }
    groupIterator->second.particles.reserve(groupIterator->second.particles.size() + emitCount);

    PM_CpuParticle particle {}; // 生成する空間亀裂パーティクル
    particle.startScale = { width * kSpaceCrackStartWidthRate, length * kSpaceCrackStartLengthRate, kParticleScaleZ };
    particle.endScale = { width, length, kParticleScaleZ };
    particle.transform.scale = particle.startScale;
    particle.transform.rotate = { 0.0f, 0.0f, rotationZ };
    particle.transform.translate = position;
    particle.velocity = kParticleZeroVector;
    particle.color = color;
    particle.startColor = color;
    particle.lifeTime = (std::max)(lifeTime, kSpaceCrackLifeTimeMin);
    particle.currentTime = kParticleTimeStart;
    particle.spawnTime = globalTime_;
    particle.useScaleOverLife = true;
    particle.useFadeOut = true;

    groupIterator->second.particles.push_back(particle);
    ++totalParticleCount_;
}

/// <summary>
/// Ringエフェクト用のパーティクルを生成する
/// </summary>
void ParticleManager::EmitRingEffect(const std::string& name, const Vector3& position, uint32_t count)
{
    auto it = particleGroups_.find(name);
    if (it == particleGroups_.end()) {
        return;
    }

    auto& particles = it->second.particles; // 生成先のパーティクル配列
    const uint32_t emitCount = GetEmitCountWithinLimit(it->second, count); // 上限を考慮した実際の生成数
    if (emitCount == 0) {
        return;
    }
    particles.reserve(particles.size() + emitCount);

    for (uint32_t i = 0; i < emitCount; ++i) {
        PM_CpuParticle particle {}; // 生成するパーティクル
        particle.startScale = kRingStartScale;
        particle.endScale = kRingEndScale;
        particle.transform.scale = particle.startScale;
        particle.transform.rotate = kParticleZeroVector;
        particle.transform.translate = position;
        particle.velocity = kParticleZeroVector;
        particle.color = kRingColor;
        particle.startColor = particle.color;
        particle.lifeTime = kRingLifeTime;
        particle.currentTime = kParticleTimeStart;
        particle.spawnTime = globalTime_;
        particle.useScaleOverLife = true;
        particle.useFadeOut = true;

        particles.push_back(particle);
    }
    totalParticleCount_ += emitCount;
}

/// <summary>
/// Cylinderエフェクト用のパーティクルを生成する
/// </summary>
void ParticleManager::EmitCylinderEffect(const std::string& name, const Vector3& position, uint32_t count)
{
    auto it = particleGroups_.find(name);
    if (it == particleGroups_.end()) {
        return;
    }

    auto& particles = it->second.particles; // 生成先のパーティクル配列
    const uint32_t emitCount = GetEmitCountWithinLimit(it->second, count); // 上限を考慮した実際の生成数
    if (emitCount == 0) {
        return;
    }
    particles.reserve(particles.size() + emitCount);

    for (uint32_t i = 0; i < emitCount; ++i) {
        PM_CpuParticle particle {}; // 生成するパーティクル
        particle.startScale = kCylinderStartScale;
        particle.endScale = kCylinderEndScale;
        particle.transform.scale = particle.startScale;
        particle.transform.rotate = kParticleZeroVector;
        particle.transform.translate = position;
        particle.velocity = kParticleZeroVector;
        particle.color = kCylinderColor;
        particle.startColor = particle.color;
        particle.lifeTime = kCylinderLifeTime;
        particle.currentTime = kParticleTimeStart;
        particle.spawnTime = globalTime_;
        particle.useScaleOverLife = true;
        particle.useFadeOut = true;

        particles.push_back(particle);
    }
    totalParticleCount_ += emitCount;
}

/// <summary>
/// 次元破砕用の色付きリングを生成する
/// </summary>
void ParticleManager::EmitRiftRing(
    const std::string& name,
    const Vector3& position,
    uint32_t count,
    const Vector4& color,
    float startScale,
    float endScale,
    float lifeTime)
{
    auto groupIterator = particleGroups_.find(name); // リングを追加するパーティクルグループ
    if (groupIterator == particleGroups_.end()) {
        return;
    }

    auto& particles = groupIterator->second.particles; // 生成先のパーティクルリスト
    const uint32_t emitCount = GetEmitCountWithinLimit(groupIterator->second, count); // 実際に生成するリング数
    if (emitCount == 0) {
        return;
    }
    particles.reserve(particles.size() + emitCount);

    for (uint32_t ringIndex = 0; ringIndex < emitCount; ++ringIndex) {
        const float scaleOffset = static_cast<float>(ringIndex)
            * kRiftRingScaleOffsetStep; // 同時生成リングの大きさ差
        PM_CpuParticle particle {}; // 生成するリングパーティクル
        particle.startScale = {
            startScale + scaleOffset,
            startScale + scaleOffset,
            kParticleScaleZ
        };
        particle.endScale = {
            endScale + scaleOffset,
            endScale + scaleOffset,
            kParticleScaleZ
        };
        particle.transform.scale = particle.startScale;
        particle.transform.translate = position;
        particle.color = color;
        particle.startColor = color;
        particle.lifeTime = (std::max)(
            lifeTime + static_cast<float>(ringIndex) * kRiftRingLifeOffsetStep,
            kRiftRingLifeTimeMin);
        particle.spawnTime = globalTime_
            + static_cast<float>(ringIndex) * kRiftRingSpawnDelayStep;
        particle.useScaleOverLife = true;
        particle.useFadeOut = true;
        particles.push_back(particle);
    }
    totalParticleCount_ += emitCount;
}

/// <summary>
/// 次元破砕用の放射状破片を生成する
/// </summary>
void ParticleManager::EmitRiftFragments(
    const std::string& name,
    const Vector3& position,
    uint32_t count,
    const Vector4& color,
    float minimumSpeed,
    float maximumSpeed,
    float lifeTime)
{
    auto groupIterator = particleGroups_.find(name); // 破片を追加するパーティクルグループ
    if (groupIterator == particleGroups_.end()) {
        return;
    }

    auto& particles = groupIterator->second.particles; // 生成先のパーティクルリスト
    const uint32_t emitCount = GetEmitCountWithinLimit(groupIterator->second, count); // 実際に生成する破片数
    if (emitCount == 0) {
        return;
    }
    particles.reserve(particles.size() + emitCount);

    const float minSpeed = (std::min)(minimumSpeed, maximumSpeed); // 破片の最低速度
    const float maxSpeed = (std::max)(minimumSpeed, maximumSpeed); // 破片の最高速度
    std::uniform_real_distribution<float> angleDistribution(-std::numbers::pi_v<float>, std::numbers::pi_v<float>); // 放射方向
    std::uniform_real_distribution<float> speedDistribution(minSpeed, maxSpeed); // 放出速度
    std::uniform_real_distribution<float> lengthDistribution(kRiftFragmentLengthMin, kRiftFragmentLengthMax); // 破片の長さ

    for (uint32_t fragmentIndex = 0; fragmentIndex < emitCount; ++fragmentIndex) {
        const float angle = angleDistribution(rng_); // 現在の破片方向
        const float speed = speedDistribution(rng_); // 現在の破片速度
        const float length = lengthDistribution(rng_); // 現在の破片長さ
        PM_CpuParticle particle {}; // 生成する破片パーティクル
        particle.startScale = { kRiftFragmentStartWidth, length, kParticleScaleZ };
        particle.endScale = { kRiftFragmentEndWidth, length * kRiftFragmentEndLengthRate, kParticleScaleZ };
        particle.transform.scale = particle.startScale;
        particle.transform.rotate = { 0.0f, 0.0f, angle - kRiftFragmentRotationOffset };
        particle.transform.translate = position;
        particle.velocity = { std::cos(angle) * speed, std::sin(angle) * speed, 0.0f };
        particle.color = color;
        particle.startColor = color;
        particle.lifeTime = (std::max)(lifeTime, kRiftFragmentLifeTimeMin);
        particle.spawnTime = globalTime_;
        particle.useScaleOverLife = true;
        particle.useFadeOut = true;
        particles.push_back(particle);
    }
    totalParticleCount_ += emitCount;

}
/// <summary>
/// パーティクルを更新する
/// </summary>
/// <summary>
/// GPUパーティクル変換に必要なリソースを作成する。
/// </summary>
void ParticleManager::InitializeGpuParticleResources()
{
    gpuParticleReady_ = false;
    if (!dxCommon_ || !srvManager_) {
        return;
    }

    ID3D12Device* device = dxCommon_->GetDevice();
    if (!device) {
        return;
    }

    D3D12_DESCRIPTOR_RANGE sourceSrvRange {};
    sourceSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    sourceSrvRange.NumDescriptors = 1;
    sourceSrvRange.BaseShaderRegister = 0;
    sourceSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE outputUavRange {};
    outputUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    outputUavRange.NumDescriptors = 1;
    outputUavRange.BaseShaderRegister = 0;
    outputUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeCounterUavRange {};
    freeCounterUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeCounterUavRange.NumDescriptors = 1;
    freeCounterUavRange.BaseShaderRegister = 1;
    freeCounterUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[6] {};
    rootParameters[kComputeRootParameterSourceSrv].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[kComputeRootParameterSourceSrv].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[kComputeRootParameterSourceSrv].DescriptorTable.pDescriptorRanges = &sourceSrvRange;
    rootParameters[kComputeRootParameterSourceSrv].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[kComputeRootParameterOutputUav].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[kComputeRootParameterOutputUav].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[kComputeRootParameterOutputUav].DescriptorTable.pDescriptorRanges = &outputUavRange;
    rootParameters[kComputeRootParameterOutputUav].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[kComputeRootParameterInfoCbv].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[kComputeRootParameterInfoCbv].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[kComputeRootParameterInfoCbv].Descriptor.ShaderRegister = 0;
    rootParameters[kComputeRootParameterEmitterCbv].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[kComputeRootParameterEmitterCbv].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[kComputeRootParameterEmitterCbv].Descriptor.ShaderRegister = 1;
    rootParameters[kComputeRootParameterPerFrameCbv].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[kComputeRootParameterPerFrameCbv].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[kComputeRootParameterPerFrameCbv].Descriptor.ShaderRegister = 2;
    rootParameters[kComputeRootParameterFreeCounterUav].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[kComputeRootParameterFreeCounterUav].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[kComputeRootParameterFreeCounterUav].DescriptorTable.pDescriptorRanges = &freeCounterUavRange;
    rootParameters[kComputeRootParameterFreeCounterUav].DescriptorTable.NumDescriptorRanges = 1;
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc {};
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to serialize compute root signature.\n");
        return;
    }

    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature_));
    if (FAILED(hr)) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to create compute root signature.\n");
        return;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(L"resources/shaders/GPUParticleUpdate.CS.hlsl", L"cs_6_0");
    if (!computeShaderBlob) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to compile GPUParticleUpdate.CS.hlsl.\n");
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc {};
    pipelineDesc.pRootSignature = computeRootSignature_.Get();
    pipelineDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };
    hr = device->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&computePipelineState_));
    if (FAILED(hr)) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to create compute pipeline state.\n");
        return;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> initializeShaderBlob = dxCommon_->CompileShader(L"resources/shaders/InitializeParticle.CS.hlsl", L"cs_6_0");
    if (!initializeShaderBlob) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to compile InitializeParticle.CS.hlsl.\n");
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC initializePipelineDesc {};
    initializePipelineDesc.pRootSignature = computeRootSignature_.Get();
    initializePipelineDesc.CS = { initializeShaderBlob->GetBufferPointer(), initializeShaderBlob->GetBufferSize() };
    hr = device->CreateComputePipelineState(&initializePipelineDesc, IID_PPV_ARGS(&initializeParticlePipelineState_));
    if (FAILED(hr)) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to create initialize particle pipeline state.\n");
        return;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> emitShaderBlob = dxCommon_->CompileShader(L"resources/shaders/EmitParticle.CS.hlsl", L"cs_6_0");
    if (!emitShaderBlob) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to compile EmitParticle.CS.hlsl.\n");
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC emitPipelineDesc {};
    emitPipelineDesc.pRootSignature = computeRootSignature_.Get();
    emitPipelineDesc.CS = { emitShaderBlob->GetBufferPointer(), emitShaderBlob->GetBufferSize() };
    hr = device->CreateComputePipelineState(&emitPipelineDesc, IID_PPV_ARGS(&emitParticlePipelineState_));
    if (FAILED(hr)) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to create emit particle pipeline state.\n");
        return;
    }

    const uint32_t particleLimit = GetParticleLimit();
    const size_t sourceBufferSize = sizeof(PM_GpuParticleSource) * particleLimit;
    const size_t infoBufferSize = (sizeof(PM_GpuParticleTransformInfo) + 0xff) & ~static_cast<size_t>(0xff);
    const size_t outputBufferSize = sizeof(PM_GpuParticleSource) * particleLimit;
    const size_t emitterBufferSize = (sizeof(PM_GpuEmitterSphere) + 0xff) & ~static_cast<size_t>(0xff);
    const size_t perFrameBufferSize = (sizeof(PM_GpuPerFrame) + 0xff) & ~static_cast<size_t>(0xff);
    const size_t freeCounterBufferSize = sizeof(int32_t);

    D3D12_SHADER_RESOURCE_VIEW_DESC sourceSrvDesc {};
    sourceSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    sourceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    sourceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sourceSrvDesc.Buffer.NumElements = particleLimit;
    sourceSrvDesc.Buffer.StructureByteStride = sizeof(PM_GpuParticleSource);
    sourceSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    D3D12_SHADER_RESOURCE_VIEW_DESC outputSrvDesc {};
    outputSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    outputSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    outputSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    outputSrvDesc.Buffer.NumElements = particleLimit;
    outputSrvDesc.Buffer.StructureByteStride = sizeof(PM_GpuParticleSource);
    outputSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    D3D12_UNORDERED_ACCESS_VIEW_DESC outputUavDesc {};
    outputUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    outputUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    outputUavDesc.Buffer.NumElements = particleLimit;
    outputUavDesc.Buffer.StructureByteStride = sizeof(PM_GpuParticleSource);
    outputUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    D3D12_UNORDERED_ACCESS_VIEW_DESC freeCounterUavDesc {};
    freeCounterUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    freeCounterUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    freeCounterUavDesc.Buffer.NumElements = 1;
    freeCounterUavDesc.Buffer.StructureByteStride = sizeof(int32_t);
    freeCounterUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        if (!srvManager_->CanAllocate()) {
            Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to allocate source SRV.\n");
            FinalizeGpuParticleResources();
            return;
        }
        gpuParticleSourceResources_[frameIndex] = dxCommon_->CreateBufferResource(sourceBufferSize);
        gpuParticleSourceResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&gpuParticleSourceData_[frameIndex]));
        gpuParticleSourceSrvIndices_[frameIndex] = srvManager_->Allocate();
        device->CreateShaderResourceView(gpuParticleSourceResources_[frameIndex].Get(), &sourceSrvDesc, srvManager_->GetCPUDescriptorHandle(gpuParticleSourceSrvIndices_[frameIndex]));

        gpuParticleInfoResources_[frameIndex] = dxCommon_->CreateBufferResource(infoBufferSize);
        gpuParticleInfoResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&gpuParticleInfoData_[frameIndex]));

        gpuEmitterResources_[frameIndex] = dxCommon_->CreateBufferResource(emitterBufferSize);
        gpuEmitterResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&gpuEmitterData_[frameIndex]));
        gpuPerFrameResources_[frameIndex] = dxCommon_->CreateBufferResource(perFrameBufferSize);
        gpuPerFrameResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&gpuPerFrameData_[frameIndex]));

        D3D12_HEAP_PROPERTIES defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC outputResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(outputBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        hr = device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &outputResourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&gpuParticleOutputResources_[frameIndex]));
        if (FAILED(hr)) {
            Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to create output buffer.\n");
            FinalizeGpuParticleResources();
            return;
        }
        gpuParticleOutputStates_[frameIndex] = D3D12_RESOURCE_STATE_COMMON;
        gpuFreeCounterStates_[frameIndex] = D3D12_RESOURCE_STATE_COMMON;

        if (!srvManager_->CanAllocate()) {
            Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to allocate output SRV.\n");
            FinalizeGpuParticleResources();
            return;
        }
        gpuParticleOutputSrvIndices_[frameIndex] = srvManager_->Allocate();
        gpuParticleOutputSrvHandlesGPU_[frameIndex] = srvManager_->GetGPUDescriptorHandle(gpuParticleOutputSrvIndices_[frameIndex]);
        device->CreateShaderResourceView(gpuParticleOutputResources_[frameIndex].Get(), &outputSrvDesc, srvManager_->GetCPUDescriptorHandle(gpuParticleOutputSrvIndices_[frameIndex]));

        if (!srvManager_->CanAllocate()) {
            Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to allocate output UAV.\n");
            FinalizeGpuParticleResources();
            return;
        }
        gpuParticleOutputUavIndices_[frameIndex] = srvManager_->Allocate();
        device->CreateUnorderedAccessView(gpuParticleOutputResources_[frameIndex].Get(), nullptr, &outputUavDesc, srvManager_->GetCPUDescriptorHandle(gpuParticleOutputUavIndices_[frameIndex]));

        D3D12_RESOURCE_DESC freeCounterResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(freeCounterBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        hr = device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &freeCounterResourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&gpuFreeCounterResources_[frameIndex]));
        if (FAILED(hr)) {
            Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to create free counter buffer.\n");
            FinalizeGpuParticleResources();
            return;
        }

        if (!srvManager_->CanAllocate()) {
            Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to allocate free counter UAV.\n");
            FinalizeGpuParticleResources();
            return;
        }
        gpuFreeCounterUavIndices_[frameIndex] = srvManager_->Allocate();
        device->CreateUnorderedAccessView(gpuFreeCounterResources_[frameIndex].Get(), nullptr, &freeCounterUavDesc, srvManager_->GetCPUDescriptorHandle(gpuFreeCounterUavIndices_[frameIndex]));
    }

    gpuParticleReady_ = true;
}

/// <summary>
/// GPUパーティクル変換に必要なリソースを解放する。
/// </summary>
void ParticleManager::FinalizeGpuParticleResources()
{
    if (srvManager_) {
        for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
            if (gpuParticleSourceSrvIndices_[frameIndex] != UINT32_MAX) {
                srvManager_->Free(gpuParticleSourceSrvIndices_[frameIndex]);
            }
            if (gpuParticleOutputSrvIndices_[frameIndex] != UINT32_MAX) {
                srvManager_->Free(gpuParticleOutputSrvIndices_[frameIndex]);
            }
            if (gpuParticleOutputUavIndices_[frameIndex] != UINT32_MAX) {
                srvManager_->Free(gpuParticleOutputUavIndices_[frameIndex]);
            }
        }
    }

    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        gpuParticleSourceSrvIndices_[frameIndex] = UINT32_MAX;
        gpuParticleOutputSrvIndices_[frameIndex] = UINT32_MAX;
        gpuParticleOutputUavIndices_[frameIndex] = UINT32_MAX;
        gpuFreeCounterUavIndices_[frameIndex] = UINT32_MAX;
        gpuParticleOutputSrvHandlesGPU_[frameIndex] = {};
        gpuParticleOutputStates_[frameIndex] = D3D12_RESOURCE_STATE_COMMON;
        gpuFreeCounterStates_[frameIndex] = D3D12_RESOURCE_STATE_COMMON;
        gpuParticleInitialized_[frameIndex] = false;
        gpuParticleSourceData_[frameIndex] = nullptr;
        gpuParticleInfoData_[frameIndex] = nullptr;
        gpuParticleSourceResources_[frameIndex].Reset();
        gpuParticleInfoResources_[frameIndex].Reset();
        gpuParticleOutputResources_[frameIndex].Reset();
    }

    computePipelineState_.Reset();
    initializeParticlePipelineState_.Reset();
    computeRootSignature_.Reset();
    gpuParticleReady_ = false;
}

/// <summary>
/// GPUへ渡すパーティクル入力データを現在のグループ内容から作成する。
/// </summary>
uint32_t ParticleManager::UploadGpuParticleSource(const ParticleGroup& group, uint32_t count, const Matrix4x4& view, const Matrix4x4& projection)
{
    if (!dxCommon_) {
        return 0;
    }

    const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex();
    if (!gpuParticleSourceData_[frameIndex] || !gpuParticleInfoData_[frameIndex]) {
        return 0;
    }

    const uint32_t particleLimit = GetParticleLimit();
    const uint32_t uploadCount = (std::min)(count, particleLimit);
    for (uint32_t particleIndex = 0; particleIndex < uploadCount; ++particleIndex) {
        const PM_CpuParticle& particle = group.particles[particleIndex];
        PM_GpuParticleSource& gpuParticle = gpuParticleSourceData_[frameIndex][particleIndex];
        gpuParticle.scale = particle.transform.scale;
        gpuParticle.lifeTime = particle.lifeTime;
        gpuParticle.rotate = particle.transform.rotate;
        gpuParticle.currentTime = particle.currentTime;
        gpuParticle.translate = particle.transform.translate;
        gpuParticle.translate.z += static_cast<float>(particleIndex) * 1e-3f;
        gpuParticle.padding0 = 0.0f;
        gpuParticle.velocity = particle.velocity;
        gpuParticle.padding1 = 0.0f;
        gpuParticle.color = particle.color;
    }

    PM_GpuParticleTransformInfo& info = *gpuParticleInfoData_[frameIndex];
    info.particleCount = uploadCount;
    info.view = view;
    info.projection = projection;

    return uploadCount;
}

/// <summary>
/// ComputeShaderでパーティクルをインスタンシング用行列へ変換する。
/// </summary>
/// <summary>
/// GPU上のParticle Resourceを初期化する。
/// </summary>
bool ParticleManager::DispatchInitializeGpuParticles()
{
    if (!gpuParticleReady_ || !dxCommon_ || !srvManager_) {
        return false;
    }

    const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex();
    if (gpuParticleInitialized_[frameIndex]) {
        return true;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    if (!commandList || !computeRootSignature_ || !initializeParticlePipelineState_ || !gpuParticleOutputResources_[frameIndex]) {
        return false;
    }

    D3D12_RESOURCE_STATES& outputState = gpuParticleOutputStates_[frameIndex];
    if (outputState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER toUavBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            gpuParticleOutputResources_[frameIndex].Get(),
            outputState,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &toUavBarrier);
        outputState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    D3D12_RESOURCE_STATES& counterState = gpuFreeCounterStates_[frameIndex];
    if (counterState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER counterBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            gpuFreeCounterResources_[frameIndex].Get(),
            counterState,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &counterBarrier);
        counterState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(initializeParticlePipelineState_.Get());
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterOutputUav, gpuParticleOutputUavIndices_[frameIndex]);
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterFreeCounterUav, gpuFreeCounterUavIndices_[frameIndex]);
    commandList->Dispatch(1, 1, 1);

    D3D12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(gpuParticleOutputResources_[frameIndex].Get());
    commandList->ResourceBarrier(1, &uavBarrier);

    D3D12_RESOURCE_BARRIER toSrvBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        gpuParticleOutputResources_[frameIndex].Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &toSrvBarrier);
    outputState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    gpuParticleInitialized_[frameIndex] = true;
    return true;
}
/// <summary>
/// GPU上でEmitterからParticleを発生させる。
/// </summary>
bool ParticleManager::DispatchEmitGpuParticles()
{
    if (!gpuParticleReady_ || !dxCommon_ || !srvManager_ || gpuEmitterState_.emit == 0) {
        return true;
    }

    const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex();
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    if (!commandList || !computeRootSignature_ || !emitParticlePipelineState_ || !gpuParticleOutputResources_[frameIndex] || !gpuFreeCounterResources_[frameIndex]) {
        return false;
    }

    if (gpuEmitterData_[frameIndex]) {
        *gpuEmitterData_[frameIndex] = gpuEmitterState_;
    }
    if (gpuPerFrameData_[frameIndex]) {
        *gpuPerFrameData_[frameIndex] = gpuPerFrameState_;
    }

    D3D12_RESOURCE_STATES& outputState = gpuParticleOutputStates_[frameIndex];
    if (outputState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER toUavBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            gpuParticleOutputResources_[frameIndex].Get(),
            outputState,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &toUavBarrier);
        outputState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    D3D12_RESOURCE_STATES& counterState = gpuFreeCounterStates_[frameIndex];
    if (counterState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER counterBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            gpuFreeCounterResources_[frameIndex].Get(),
            counterState,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &counterBarrier);
        counterState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(emitParticlePipelineState_.Get());
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterOutputUav, gpuParticleOutputUavIndices_[frameIndex]);
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterFreeCounterUav, gpuFreeCounterUavIndices_[frameIndex]);
    commandList->SetComputeRootConstantBufferView(kComputeRootParameterEmitterCbv, gpuEmitterResources_[frameIndex]->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(kComputeRootParameterPerFrameCbv, gpuPerFrameResources_[frameIndex]->GetGPUVirtualAddress());
    commandList->Dispatch(1, 1, 1);

    D3D12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(gpuParticleOutputResources_[frameIndex].Get());
    commandList->ResourceBarrier(1, &uavBarrier);

    D3D12_RESOURCE_BARRIER toSrvBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        gpuParticleOutputResources_[frameIndex].Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &toSrvBarrier);
    outputState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    return true;
}

/// <summary>
/// GPU Emitterの経過時間と射出許可を更新する。
/// </summary>
void ParticleManager::UpdateGpuEmitter(float dt)
{
    gpuPerFrameState_.time = globalTime_;
    gpuPerFrameState_.deltaTime = dt;
    gpuEmitterState_.frequencyTime += dt;
    if (gpuEmitterState_.frequencyTime >= gpuEmitterState_.frequency) {
        gpuEmitterState_.frequencyTime -= gpuEmitterState_.frequency;
        gpuEmitterState_.emit = 1;
        gpuEmitterVisibleCount_ = (std::min)(GetParticleLimit(), gpuEmitterVisibleCount_ + gpuEmitterState_.count);
    } else {
        gpuEmitterState_.emit = 0;
    }
}
bool ParticleManager::DispatchGpuParticleTransform(uint32_t count)
{
    if (!gpuParticleReady_ || count == 0 || !dxCommon_ || !srvManager_) {
        return false;
    }

    const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex();
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    if (!commandList || !computeRootSignature_ || !computePipelineState_ || !gpuParticleOutputResources_[frameIndex]) {
        return false;
    }

    D3D12_RESOURCE_STATES& outputState = gpuParticleOutputStates_[frameIndex];
    if (outputState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER toUavBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            gpuParticleOutputResources_[frameIndex].Get(),
            outputState,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &toUavBarrier);
        outputState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(computePipelineState_.Get());
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterSourceSrv, gpuParticleSourceSrvIndices_[frameIndex]);
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterOutputUav, gpuParticleOutputUavIndices_[frameIndex]);
    commandList->SetComputeRootConstantBufferView(kComputeRootParameterInfoCbv, gpuParticleInfoResources_[frameIndex]->GetGPUVirtualAddress());

    const uint32_t dispatchGroupCount = (count + kGpuParticleThreadCount - 1) / kGpuParticleThreadCount;
    commandList->Dispatch(dispatchGroupCount, 1, 1);

    D3D12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(gpuParticleOutputResources_[frameIndex].Get());
    commandList->ResourceBarrier(1, &uavBarrier);

    D3D12_RESOURCE_BARRIER toSrvBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        gpuParticleOutputResources_[frameIndex].Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &toSrvBarrier);
    outputState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    return true;
}

/// <summary>
/// パーティクルを更新する。
/// </summary>
void ParticleManager::Update(float dt)
{
    globalTime_ += dt;
    UpdateGpuEmitter(dt);

    for (auto& kv : particleGroups_) {
        auto& particles = kv.second.particles;
        size_t writeIndex = 0;

        for (size_t readIndex = 0; readIndex < particles.size(); ++readIndex) {
            PM_CpuParticle particle = particles[readIndex];

            if (fieldEnabled_) {
                const Vector3& position = particle.transform.translate;
                if (position.x >= fieldMin_.x && position.y >= fieldMin_.y && position.z >= fieldMin_.z && position.x <= fieldMax_.x && position.y <= fieldMax_.y && position.z <= fieldMax_.z) {
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

            if (damping_ > kDampingEnabledThreshold) {
                const float dampingRate = std::clamp(kParticleRateMax - damping_ * dt, kParticleRateMin, kParticleRateMax);
                particle.velocity.x *= dampingRate;
                particle.velocity.y *= dampingRate;
                particle.velocity.z *= dampingRate;
            }

            particle.transform.translate.x += particle.velocity.x * dt;
            particle.transform.translate.y += particle.velocity.y * dt;
            particle.transform.translate.z += particle.velocity.z * dt;

            particle.currentTime += dt;
            const float lifeRate = particle.lifeTime > kParticleLifeTimeCheckMin ? std::clamp(particle.currentTime / particle.lifeTime, kParticleRateMin, kParticleRateMax) : kParticleRateMax;
            if (particle.useScaleOverLife) {
                const float scaleRate = std::sin(lifeRate * std::numbers::pi_v<float>);
                particle.transform.scale.x = particle.startScale.x + (particle.endScale.x - particle.startScale.x) * scaleRate;
                particle.transform.scale.y = particle.startScale.y + (particle.endScale.y - particle.startScale.y) * scaleRate;
                particle.transform.scale.z = particle.startScale.z + (particle.endScale.z - particle.startScale.z) * scaleRate;
            }
            if (particle.useFadeOut) {
                particle.color.w = particle.startColor.w * (kParticleFadeAlphaBase - lifeRate);
            }

            if (particle.currentTime < particle.lifeTime) {
                particles[writeIndex] = particle;
                ++writeIndex;
            }
        }

        const size_t removedCount = particles.size() - writeIndex;
        if (removedCount > 0) {
            particles.resize(writeIndex);
            totalParticleCount_ -= removedCount;
        }
    }
}

/// <summary>
/// パーティクルを描画する。
/// </summary>
void ParticleManager::Draw()
{
    if (!dxCommon_ || !object3dCommon_ || !particlePlane_) {
        return;
    }

    object3dCommon_->SetInstancingDrawSetting();

    Camera* camera = object3dCommon_->GetDefaultCamera();
    Matrix4x4 view = camera ? camera->GetViewMatrix() : Matrix4x4();
    Matrix4x4 projection = camera ? camera->GetProjectionMatrix() : Matrix4x4();
    Matrix4x4 viewProjection = MathUtil::Multiply(view, projection);
    Vector3 cameraRight = { view.m[0][0], view.m[1][0], view.m[2][0] };
    Vector3 cameraUp = { view.m[0][1], view.m[1][1], view.m[2][1] };

    const uint32_t instancingSlots = object3dCommon_->GetInstancingSlotCount();
    if (instancingSlots == 0) {
        return;
    }


    for (auto& kv : particleGroups_) {
        ParticleGroup& group = kv.second;
        const uint32_t particleCount = static_cast<uint32_t>(group.particles.size());
        if (particleCount == 0) {
            continue;
        }

        uint32_t count = particleCount;
        if (particleCount > instancingSlots) {
            count = instancingSlots;
            if (instancingLimitWarnedGroups_.insert(kv.first).second) {
                Logger::Warn("Particle group '" + kv.first + "' exceeds instancing slots. particles=" + std::to_string(particleCount) + ", slots=" + std::to_string(instancingSlots));
            }
        } else {
            instancingLimitWarnedGroups_.erase(kv.first);
        }

        Object3d* renderObject = group.renderObject ? group.renderObject : particlePlane_;
        if (!renderObject) {
            continue;
        }

        object3dCommon_->SetBillboardCameraWithVP(cameraRight, cameraUp, viewProjection, group.useBillboard);

        const uint32_t gpuParticleCount = UploadGpuParticleSource(group, count, view, projection);
        if (gpuParticleCount != count || !DispatchInitializeGpuParticles() || !DispatchGpuParticleTransform(count)) {
            object3dCommon_->ClearInstancingSrvOverride();
            continue;
        }

        const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex();
        object3dCommon_->SetInstancingSrvOverride(gpuParticleOutputSrvHandlesGPU_[frameIndex]);
        object3dCommon_->SetInstancingDrawSetting();

        if (auto* model = renderObject->GetModel()) {
            model->DrawInstanced(renderObject, count);
        } else {
            renderObject->DrawInstanced(count);
        }
        object3dCommon_->ClearInstancingSrvOverride();
    }

    if (gpuEmitterState_.emit != 0 && !particleGroups_.empty()) {
        auto drawGroupIterator = particleGroups_.begin();
        ParticleGroup& gpuEmitterGroup = drawGroupIterator->second;
        Object3d* renderObject = gpuEmitterGroup.renderObject ? gpuEmitterGroup.renderObject : particlePlane_;
        const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex();
        gpuParticleInitialized_[frameIndex] = false;
        if (renderObject && DispatchInitializeGpuParticles() && DispatchEmitGpuParticles()) {
            const uint32_t drawCount = (std::min)(gpuEmitterState_.count, instancingSlots);
            object3dCommon_->SetBillboardCameraWithVP(cameraRight, cameraUp, viewProjection, gpuEmitterGroup.useBillboard);
            object3dCommon_->SetInstancingSrvOverride(gpuParticleOutputSrvHandlesGPU_[frameIndex]);
            object3dCommon_->SetInstancingDrawSetting();
            if (auto* model = renderObject->GetModel()) {
                model->DrawInstanced(renderObject, drawCount);
            } else {
                renderObject->DrawInstanced(drawCount);
            }
            object3dCommon_->ClearInstancingSrvOverride();
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

void ParticleManager::SetSpawnPosRange(const Vector3& mn, const Vector3& mx)
{
    spawnPosMin_ = mn;
    spawnPosMax_ = mx;
}
void ParticleManager::SetVelocityRange(const Vector3& mn, const Vector3& mx)
{
    velMin_ = mn;
    velMax_ = mx;
}
void ParticleManager::SetScaleRange(const Vector3& mn, const Vector3& mx)
{
    scaleMin_ = mn;
    scaleMax_ = mx;
}
void ParticleManager::SetColorRange(const Vector4& mn, const Vector4& mx)
{
    colMin_ = mn;
    colMax_ = mx;
}

void ParticleManager::SetGravityEnabled(bool enabled) { gravityEnabled_ = enabled; }
void ParticleManager::SetGravity(const Vector3& g) { gravity_ = g; }
void ParticleManager::SetDamping(float d) { damping_ = d < kDampingEnabledThreshold ? kDampingEnabledThreshold : d; }

/// <summary>
/// ImGuiでパーティクル設定を編集する
/// </summary>
void ParticleManager::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Groups: %zu", particleGroups_.size());

    if (ImGui::CollapsingHeader("Lifetime", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloatRange2(
            "Life Min/Max",
            &lifeMin_,
            &lifeMax_,
            kImGuiFineStep,
            kImGuiLifeMin,
            kImGuiLifeMax);
    }

    if (ImGui::CollapsingHeader("Spawn Random", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3(
            "Spawn Pos Min",
            &spawnPosMin_.x,
            kImGuiFineStep,
            kImGuiSpawnPositionMin,
            kImGuiSpawnPositionMax);
        ImGui::DragFloat3(
            "Spawn Pos Max",
            &spawnPosMax_.x,
            kImGuiFineStep,
            kImGuiSpawnPositionMin,
            kImGuiSpawnPositionMax);
        ImGui::DragFloat3(
            "Scale Min",
            &scaleMin_.x,
            kImGuiFineStep,
            kImGuiScaleMin,
            kImGuiScaleMax);
        ImGui::DragFloat3(
            "Scale Max",
            &scaleMax_.x,
            kImGuiFineStep,
            kImGuiScaleMin,
            kImGuiScaleMax);
    }

    if (ImGui::CollapsingHeader("Velocity / Physics")) {
        ImGui::DragFloat3(
            "Vel Min",
            &velMin_.x,
            kImGuiFineStep,
            kImGuiSpawnPositionMin,
            kImGuiSpawnPositionMax);
        ImGui::DragFloat3(
            "Vel Max",
            &velMax_.x,
            kImGuiFineStep,
            kImGuiSpawnPositionMin,
            kImGuiSpawnPositionMax);

        ImGui::Checkbox("Enable Gravity", &gravityEnabled_);
        ImGui::DragFloat3(
            "Gravity",
            &gravity_.x,
            kImGuiPhysicsStep,
            kImGuiPhysicsMin,
            kImGuiPhysicsMax);
        ImGui::DragFloat(
            "Damping",
            &damping_,
            kImGuiFineStep,
            kImGuiDampingMin,
            kImGuiDampingMax);
    }

    if (ImGui::CollapsingHeader("Field")) {
        ImGui::Checkbox("Enable Field", &fieldEnabled_);
        ImGui::DragFloat3(
            "Field Accel",
            &fieldAccel_.x,
            kImGuiPhysicsStep,
            kImGuiPhysicsMin,
            kImGuiPhysicsMax);
        ImGui::DragFloat3(
            "Field Min",
            &fieldMin_.x,
            kImGuiPhysicsStep,
            kImGuiPhysicsMin,
            kImGuiPhysicsMax);
        ImGui::DragFloat3(
            "Field Max",
            &fieldMax_.x,
            kImGuiPhysicsStep,
            kImGuiPhysicsMin,
            kImGuiPhysicsMax);
    }

    if (ImGui::CollapsingHeader("Color")) {
        ImGui::ColorEdit4("Color Min", &colMin_.x);
        ImGui::ColorEdit4("Color Max", &colMax_.x);
    }

    if (ImGui::CollapsingHeader("Groups")) {
        for (auto& kv : particleGroups_) {
            if (ImGui::TreeNode(kv.first.c_str())) {
                ImGui::Text("Count = %zu", kv.second.particles.size());
                ImGui::Text("Texture = %s", kv.second.texturePath.c_str());
                if (!kv.second.particles.empty()) {
                    bool hasBounds = false; // 範囲の初期化が済んでいるか
                    Vector3 minimumPosition {}; // グループ内の最小座標
                    Vector3 maximumPosition {}; // グループ内の最大座標
                    const PM_CpuParticle* firstParticle = nullptr; // 先頭パーティクルの参照

                    for (const PM_CpuParticle& particle : kv.second.particles) {
                        const Vector3& position = particle.transform.translate; // 現在のワールド座標
                        if (!hasBounds) {
                            minimumPosition = position;
                            maximumPosition = position;
                            firstParticle = &particle;
                            hasBounds = true;
                            continue;
                        }

                        minimumPosition.x = (std::min)(minimumPosition.x, position.x);
                        minimumPosition.y = (std::min)(minimumPosition.y, position.y);
                        minimumPosition.z = (std::min)(minimumPosition.z, position.z);
                        maximumPosition.x = (std::max)(maximumPosition.x, position.x);
                        maximumPosition.y = (std::max)(maximumPosition.y, position.y);
                        maximumPosition.z = (std::max)(maximumPosition.z, position.z);
                    }

                    const Vector3 centerPosition {
                        (minimumPosition.x + maximumPosition.x) * kBoundsCenterRate,
                        (minimumPosition.y + maximumPosition.y) * kBoundsCenterRate,
                        (minimumPosition.z + maximumPosition.z) * kBoundsCenterRate
                    }; // グループ全体の中心座標

                    ImGui::Text("Center = %.2f, %.2f, %.2f", centerPosition.x, centerPosition.y, centerPosition.z);
                    ImGui::Text("Min = %.2f, %.2f, %.2f", minimumPosition.x, minimumPosition.y, minimumPosition.z);
                    ImGui::Text("Max = %.2f, %.2f, %.2f", maximumPosition.x, maximumPosition.y, maximumPosition.z);
                    if (firstParticle) {
                        const Vector3& firstPosition = firstParticle->transform.translate; // 先頭パーティクルの座標
                        ImGui::Text("First = %.2f, %.2f, %.2f", firstPosition.x, firstPosition.y, firstPosition.z);
                        const Vector3& firstScale = firstParticle->transform.scale; // 先頭パーティクルのスケール
                        const Vector4& firstColor = firstParticle->color; // 先頭パーティクルの色
                        ImGui::Text("Scale = %.2f, %.2f, %.2f", firstScale.x, firstScale.y, firstScale.z);
                        ImGui::Text("Color = %.2f, %.2f, %.2f, %.2f", firstColor.x, firstColor.y, firstColor.z, firstColor.w);
                    }
                }

                ImGui::Checkbox("Use Billboard", &kv.second.useBillboard);
                ImGui::TreePop();
            }
        }
    }
#endif
}
