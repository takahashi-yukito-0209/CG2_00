#include "ParticleManager.h"
#include "GpuEmitterSettingsUtility.h"
#include "ImGuiManager.h"
#include "engine/3d/Camera.h"
#include "engine/3d/Model.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/PostProcess.h"
#include "externals/DirectXTex/d3dx12.h"
#include "engine/utility/Logger.h"
#include "engine/utility/FileUtility.h"
#include "engine/utility/JsonUtility.h"
#include "engine/utility/RandomUtility.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
constexpr uint32_t kGpuEmitterThreadCount = 256; // GPU Emitter発生CSの1グループあたりのスレッド数
constexpr uint32_t kComputeRootParameterSourceSrv = 0; // Compute入力SRVのルート番号
constexpr uint32_t kComputeRootParameterOutputUav = 1; // Compute出力UAVのルート番号
constexpr uint32_t kComputeRootParameterInfoCbv = 2; // Compute定数バッファのルート番号 // Rift破片の向き補正
constexpr uint32_t kComputeRootParameterEmitterCbv = 3; // GPU Emitter用CBVのルート番号
constexpr uint32_t kComputeRootParameterPerFrameCbv = 4; // GPU Emitter用フレームCBVのルート番号
constexpr uint32_t kComputeRootParameterFreeCounterUav = 5; // GPU Emitter用FreeListIndex UAVのルート番号
constexpr uint32_t kComputeRootParameterFreeListUav = 6; // GPU Emitter用FreeList UAVのルート番号

struct CpuParticleEffectDesc {
    Vector3 position; // 発生位置
    Vector3 startScale; // 開始時のスケール
    Vector3 endScale; // 終了時のスケール
    Vector3 rotate; // 回転角
    Vector3 velocity; // 移動速度
    Vector4 color; // 表示色
    float lifeTime = 1.0f; // 寿命
    bool useScaleOverLife = true; // 寿命に応じてスケールを変えるか
    bool useFadeOut = true; // 寿命に応じて透明度を下げるか
};

/// <summary>
/// CPUパーティクル設定から1粒分のパーティクルを作成する。
/// </summary>
PM_CpuParticle CreateCpuParticle(const CpuParticleEffectDesc& desc, float spawnTime)
{
    PM_CpuParticle particle {}; // 生成するCPUパーティクル
    particle.startScale = desc.startScale;
    particle.endScale = desc.endScale;
    particle.transform.scale = desc.startScale;
    particle.transform.rotate = desc.rotate;
    particle.transform.translate = desc.position;
    particle.velocity = desc.velocity;
    particle.color = desc.color;
    particle.startColor = desc.color;
    particle.lifeTime = desc.lifeTime;
    particle.currentTime = kParticleTimeStart;
    particle.spawnTime = spawnTime;
    particle.useScaleOverLife = desc.useScaleOverLife;
    particle.useFadeOut = desc.useFadeOut;
    return particle;
}

/// <summary>
/// CPUパーティクル設定から指定数のパーティクルを配列へ追加する。
/// </summary>
void AppendCpuParticles(std::vector<PM_CpuParticle>& particles, const CpuParticleEffectDesc& desc, uint32_t emitCount, float spawnTime)
{
    particles.reserve(particles.size() + emitCount);
    for (uint32_t particleIndex = 0; particleIndex < emitCount; ++particleIndex) {
        particles.push_back(CreateCpuParticle(desc, spawnTime));
    }
}

} // namespace

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


    for (uint32_t i = 0; i < emitCount; ++i) {
        PM_CpuParticle particle {}; // 生成するパーティクル
        particle.transform.scale = RandomUtility::RandomVector3(scaleMin_, scaleMax_);
        particle.startScale = particle.transform.scale;
        particle.endScale = particle.transform.scale;
        particle.transform.rotate = kParticleZeroVector;
        particle.transform.translate = position + RandomUtility::RandomVector3(spawnPosMin_, spawnPosMax_);
        particle.velocity = RandomUtility::RandomVector3(velMin_, velMax_);
        particle.color = RandomUtility::RandomVector4(colMin_, colMax_);
        particle.startColor = particle.color;
        particle.lifeTime = RandomUtility::RandomFloat(lifeMin_, lifeMax_);
        particle.currentTime = kParticleTimeStart;
        particle.spawnTime = globalTime_;

        particles.push_back(particle);
    }
    totalParticleCount_ += emitCount;
}

/// <summary>
/// ヒットエフェクト用の細長いパーティクルを生成する。
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


    for (uint32_t i = 0; i < emitCount; ++i) {
        const float length = RandomUtility::RandomFloat(kHitLengthMin, kHitLengthMax); // 光の筋の最大長さ
        const float alpha = RandomUtility::RandomFloat(kHitAlphaMin, kHitAlphaMax); // 発生時の透明度
        const CpuParticleEffectDesc desc {
            position,
            { kHitStartScaleX, length * kHitStartScaleYRate, kParticleScaleZ },
            { kHitEndScaleX, length, kParticleScaleZ },
            { 0.0f, 0.0f, RandomUtility::RandomFloat(-std::numbers::pi_v<float>, std::numbers::pi_v<float>) },
            kParticleZeroVector,
            { kHitColorRgb.x, kHitColorRgb.y, kHitColorRgb.z, alpha },
            kHitLifeTime,
            true,
            true
        }; // ヒット演出用の生成設定
        particles.push_back(CreateCpuParticle(desc, globalTime_));
    }
    totalParticleCount_ += emitCount;
}

/// <summary>
/// 指定した形状で空間亀裂用のパーティクルを生成する。
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
    const CpuParticleEffectDesc desc {
        position,
        { width * kSpaceCrackStartWidthRate, length * kSpaceCrackStartLengthRate, kParticleScaleZ },
        { width, length, kParticleScaleZ },
        { 0.0f, 0.0f, rotationZ },
        kParticleZeroVector,
        color,
        (std::max)(lifeTime, kSpaceCrackLifeTimeMin),
        true,
        true
    }; // 空間亀裂演出用の生成設定
    AppendCpuParticles(groupIterator->second.particles, desc, emitCount, globalTime_);
    ++totalParticleCount_;
}

/// <summary>
/// リングエフェクト用のパーティクルを生成する。
/// </summary>
void ParticleManager::EmitRingEffect(const std::string& name, const Vector3& position, uint32_t count)
{
    auto groupIterator = particleGroups_.find(name); // 生成先のパーティクルグループ
    if (groupIterator == particleGroups_.end()) {
        return;
    }

    ParticleGroup& group = groupIterator->second; // 生成先グループ
    const uint32_t emitCount = GetEmitCountWithinLimit(group, count); // 上限を考慮した実際の生成数
    if (emitCount == 0) {
        return;
    }

    const CpuParticleEffectDesc desc {
        position,
        kRingStartScale,
        kRingEndScale,
        kParticleZeroVector,
        kParticleZeroVector,
        kRingColor,
        kRingLifeTime,
        true,
        true
    }; // リング演出用の生成設定
    AppendCpuParticles(group.particles, desc, emitCount, globalTime_);
    totalParticleCount_ += emitCount;
}

/// <summary>
/// 円柱エフェクト用のCPUパーティクルを生成する。
/// </summary>
void ParticleManager::EmitCylinderEffect(const std::string& name, const Vector3& position, uint32_t count)
{
    auto groupIterator = particleGroups_.find(name); // 生成先のパーティクルグループ
    if (groupIterator == particleGroups_.end()) {
        return;
    }

    ParticleGroup& group = groupIterator->second; // 生成先グループ
    const uint32_t emitCount = GetEmitCountWithinLimit(group, count); // 上限を考慮した実際の生成数
    if (emitCount == 0) {
        return;
    }

    const CpuParticleEffectDesc desc {
        position,
        kCylinderStartScale,
        kCylinderEndScale,
        kParticleZeroVector,
        kParticleZeroVector,
        kCylinderColor,
        kCylinderLifeTime,
        true,
        true
    }; // 円柱演出用の生成設定
    AppendCpuParticles(group.particles, desc, emitCount, globalTime_);
    totalParticleCount_ += emitCount;
}

/// <summary>
/// 次元破砕用の色付きリングを生成する。
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
        const CpuParticleEffectDesc desc {
            position,
            { startScale + scaleOffset, startScale + scaleOffset, kParticleScaleZ },
            { endScale + scaleOffset, endScale + scaleOffset, kParticleScaleZ },
            kParticleZeroVector,
            kParticleZeroVector,
            color,
            (std::max)(lifeTime + static_cast<float>(ringIndex) * kRiftRingLifeOffsetStep, kRiftRingLifeTimeMin),
            true,
            true
        }; // 次元破砕リング用の生成設定
        const float spawnTime = globalTime_ + static_cast<float>(ringIndex) * kRiftRingSpawnDelayStep; // リングごとの発生時刻
        particles.push_back(CreateCpuParticle(desc, spawnTime));
    }
    totalParticleCount_ += emitCount;
}

/// <summary>
/// 次元破砕用の放射状破片を生成する。
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

    for (uint32_t fragmentIndex = 0; fragmentIndex < emitCount; ++fragmentIndex) {
        const Vector2 direction = RandomUtility::RandomDirection2D(); // 現在の破片方向
        const float speed = RandomUtility::RandomFloat(minSpeed, maxSpeed); // 現在の破片速度
        const float length = RandomUtility::RandomFloat(kRiftFragmentLengthMin, kRiftFragmentLengthMax); // 現在の破片長さ
        const CpuParticleEffectDesc desc {
            position,
            { kRiftFragmentStartWidth, length, kParticleScaleZ },
            { kRiftFragmentEndWidth, length * kRiftFragmentEndLengthRate, kParticleScaleZ },
            { 0.0f, 0.0f, std::atan2(direction.y, direction.x) - kRiftFragmentRotationOffset },
            { direction.x * speed, direction.y * speed, 0.0f },
            color,
            (std::max)(lifeTime, kRiftFragmentLifeTimeMin),
            true,
            true
        }; // 次元破砕破片用の生成設定
        particles.push_back(CreateCpuParticle(desc, globalTime_));
    }

    totalParticleCount_ += emitCount;
}

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

    D3D12_DESCRIPTOR_RANGE freeListUavRange {};
    freeListUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListUavRange.NumDescriptors = 1;
    freeListUavRange.BaseShaderRegister = 2;
    freeListUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[7] {};
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
    rootParameters[kComputeRootParameterFreeListUav].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[kComputeRootParameterFreeListUav].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[kComputeRootParameterFreeListUav].DescriptorTable.pDescriptorRanges = &freeListUavRange;
    rootParameters[kComputeRootParameterFreeListUav].DescriptorTable.NumDescriptorRanges = 1;
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

    
    Microsoft::WRL::ComPtr<IDxcBlob> updateShaderBlob = dxCommon_->CompileShader(L"resources/shaders/UpdateParticle.CS.hlsl", L"cs_6_0");
    if (!updateShaderBlob) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to compile UpdateParticle.CS.hlsl.\n");
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC updatePipelineDesc {};
    updatePipelineDesc.pRootSignature = computeRootSignature_.Get();
    updatePipelineDesc.CS = { updateShaderBlob->GetBufferPointer(), updateShaderBlob->GetBufferSize() };
    hr = device->CreateComputePipelineState(&updatePipelineDesc, IID_PPV_ARGS(&updateParticlePipelineState_));
    if (FAILED(hr)) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to create update particle pipeline state.\n");
        return;
    }

    const uint32_t particleLimit = GetParticleLimit();
    const size_t sourceBufferSize = sizeof(PM_GpuParticleSource) * particleLimit;
    const size_t infoBufferSize = (sizeof(PM_GpuParticleTransformInfo) + 0xff) & ~static_cast<size_t>(0xff);
    const size_t outputBufferSize = sizeof(PM_GpuParticleSource) * particleLimit;
    const size_t emitterBufferSize = (sizeof(PM_GpuEmitterSphere) + 0xff) & ~static_cast<size_t>(0xff);
    const size_t perFrameBufferSize = (sizeof(PM_GpuPerFrame) + 0xff) & ~static_cast<size_t>(0xff);
    const size_t freeCounterBufferSize = sizeof(int32_t);
    const size_t freeListBufferSize = sizeof(uint32_t) * particleLimit;

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

    D3D12_UNORDERED_ACCESS_VIEW_DESC freeListUavDesc {};
    freeListUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    freeListUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    freeListUavDesc.Buffer.NumElements = particleLimit;
    freeListUavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
    freeListUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

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
        gpuFreeListStates_[frameIndex] = D3D12_RESOURCE_STATE_COMMON;

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

        D3D12_HEAP_PROPERTIES readbackHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
        D3D12_RESOURCE_DESC freeCounterReadbackDesc = CD3DX12_RESOURCE_DESC::Buffer(freeCounterBufferSize);
        hr = device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &freeCounterReadbackDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&gpuFreeCounterReadbackResources_[frameIndex]));
        if (FAILED(hr)) {
            Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to create free counter readback buffer.\n");
            FinalizeGpuParticleResources();
            return;
        }
        gpuFreeCounterReadbackResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&gpuFreeCounterReadbackData_[frameIndex]));
        if (gpuFreeCounterReadbackData_[frameIndex]) {
            *gpuFreeCounterReadbackData_[frameIndex] = static_cast<int32_t>(particleLimit) - 1;
        }

        D3D12_RESOURCE_DESC freeListResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(freeListBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        hr = device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &freeListResourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&gpuFreeListResources_[frameIndex]));
        if (FAILED(hr)) {
            Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to create free list buffer.\n");
            FinalizeGpuParticleResources();
            return;
        }

        if (!srvManager_->CanAllocate()) {
            Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to allocate free list UAV.\n");
            FinalizeGpuParticleResources();
            return;
        }
        gpuFreeListUavIndices_[frameIndex] = srvManager_->Allocate();
        device->CreateUnorderedAccessView(gpuFreeListResources_[frameIndex].Get(), nullptr, &freeListUavDesc, srvManager_->GetCPUDescriptorHandle(gpuFreeListUavIndices_[frameIndex]));
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
            if (gpuFreeCounterUavIndices_[frameIndex] != UINT32_MAX) {
                srvManager_->Free(gpuFreeCounterUavIndices_[frameIndex]);
            }
            if (gpuFreeListUavIndices_[frameIndex] != UINT32_MAX) {
                srvManager_->Free(gpuFreeListUavIndices_[frameIndex]);
            }
        }
    }

    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        gpuParticleSourceSrvIndices_[frameIndex] = UINT32_MAX;
        gpuParticleOutputSrvIndices_[frameIndex] = UINT32_MAX;
        gpuParticleOutputUavIndices_[frameIndex] = UINT32_MAX;
        gpuFreeCounterUavIndices_[frameIndex] = UINT32_MAX;
        gpuFreeListUavIndices_[frameIndex] = UINT32_MAX;
        gpuParticleOutputSrvHandlesGPU_[frameIndex] = {};
        gpuParticleOutputStates_[frameIndex] = D3D12_RESOURCE_STATE_COMMON;
        gpuFreeCounterStates_[frameIndex] = D3D12_RESOURCE_STATE_COMMON;
        gpuFreeListStates_[frameIndex] = D3D12_RESOURCE_STATE_COMMON;
        gpuParticleInitialized_[frameIndex] = false;
        gpuParticleSourceData_[frameIndex] = nullptr;
        gpuParticleInfoData_[frameIndex] = nullptr;
        gpuParticleSourceResources_[frameIndex].Reset();
        gpuParticleInfoResources_[frameIndex].Reset();
        gpuParticleOutputResources_[frameIndex].Reset();
        if (gpuFreeCounterReadbackResources_[frameIndex]) {
            gpuFreeCounterReadbackResources_[frameIndex]->Unmap(0, nullptr);
        }
        gpuFreeCounterReadbackData_[frameIndex] = nullptr;
        gpuFreeCounterResources_[frameIndex].Reset();
        gpuFreeCounterReadbackResources_[frameIndex].Reset();
        gpuFreeListResources_[frameIndex].Reset();
    }

    computePipelineState_.Reset();
    initializeParticlePipelineState_.Reset();
    emitParticlePipelineState_.Reset();
    updateParticlePipelineState_.Reset();
    computeRootSignature_.Reset();
    gpuParticleReady_ = false;
    gpuAliveCountEstimate_ = 0;
}

/// <summary>
/// GPU FreeListIndexをReadback用Resourceへコピーする。
/// </summary>
void ParticleManager::CopyGpuFreeCounterToReadback(uint32_t frameIndex)
{
    if (!dxCommon_ || frameIndex >= DirectXCommon::kFrameCount || !gpuFreeCounterResources_[frameIndex] || !gpuFreeCounterReadbackResources_[frameIndex]) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList(); // Readbackコピーを積むコマンドリスト
    if (!commandList) {
        return;
    }

    D3D12_RESOURCE_STATES& counterState = gpuFreeCounterStates_[frameIndex]; // FreeListIndex Resourceの現在状態
    if (counterState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        D3D12_RESOURCE_BARRIER toCopySource = CD3DX12_RESOURCE_BARRIER::Transition(
            gpuFreeCounterResources_[frameIndex].Get(),
            counterState,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        commandList->ResourceBarrier(1, &toCopySource);
        counterState = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }

    commandList->CopyResource(gpuFreeCounterReadbackResources_[frameIndex].Get(), gpuFreeCounterResources_[frameIndex].Get());
}

/// <summary>
/// Readback済みのFreeListIndexからGPU Particleの生存数推定値を更新する。
/// </summary>
void ParticleManager::UpdateGpuAliveCountEstimate()
{
    const uint32_t particleLimit = GetParticleLimit(); // GPU Particleの最大数
    uint32_t aliveCount = 0; // 読み戻し済みフレームから推定した最大生存数

    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        const int32_t* counterData = gpuFreeCounterReadbackData_[frameIndex]; // Readback済みFreeListIndex
        if (!counterData) {
            continue;
        }

        const int32_t freeListIndex = *counterData;
        const int32_t estimatedAlive = static_cast<int32_t>(particleLimit) - 1 - freeListIndex;
        if (estimatedAlive > 0) {
            aliveCount = (std::max)(aliveCount, static_cast<uint32_t>((std::min)(estimatedAlive, static_cast<int32_t>(particleLimit))));
        }
    }

    gpuAliveCountEstimate_ = aliveCount;
}

/// <summary>
/// GPU Emitter設定をJSONファイルへ保存する。
/// </summary>
bool ParticleManager::SaveGpuEmitterSettings(const std::string& filePath) const
{
    const std::filesystem::path outputPath(filePath); // 保存先パス
    const std::string parentDirectory = FileUtility::GetParentDirectory(filePath); // 保存先の親ディレクトリ
    if (!parentDirectory.empty() && !FileUtility::CreateDirectoryIfNeeded(parentDirectory)) {
        Logger::Warn("ParticleManager::SaveGpuEmitterSettings: failed to create directory " + parentDirectory + "\n");
        return false;
    }

    std::ofstream file(outputPath); // JSONを書き出すファイルストリーム
    if (!file) {
        Logger::Warn("ParticleManager::SaveGpuEmitterSettings: failed to open " + filePath + "\n");
        return false;
    }

    WriteGpuEmitterSettingsJson(file);
    return true;
}

/// <summary>
/// GPU Emitter設定をJSON形式で書き出す。
/// </summary>
void ParticleManager::WriteGpuEmitterSettingsJson(std::ostream& file) const
{
    const Vector3 postRadialBlurCenter { gpuEmitterRadialBlurCenter_.x, gpuEmitterRadialBlurCenter_.y, 0.0f }; // JSON保存用のRadialBlur中心
    const Vector3 postDistortionCenter { gpuEmitterDistortionCenter_.x, gpuEmitterDistortionCenter_.y, 0.0f }; // JSON保存用のDistortion中心

    file << std::fixed << std::setprecision(4);
    file << "{\n";
    file << "  \"version\": 2,\n";

    file << "  \"effect\": {\n";
    file << "    \"effectName\": \"" << JsonUtility::EscapeString(gpuEmitterEffectName_) << "\",\n";
    file << "    \"description\": \"" << JsonUtility::EscapeString(gpuEmitterDescription_) << "\"\n";
    file << "  },\n";

    file << "  \"playback\": {\n";
    file << "    \"autoEmit\": " << (gpuEmitterAutoEmit_ ? 1 : 0) << ",\n";
    file << "    \"updateParticles\": " << (gpuParticleUpdateEnabled_ ? 1 : 0) << ",\n";
    file << "    \"drawParticles\": " << (gpuParticleDrawEnabled_ ? 1 : 0) << "\n";
    file << "  },\n";

    file << "  \"render\": {\n";
    file << "    \"texture\": \"" << JsonUtility::EscapeString(gpuEmitterTexturePath_) << "\",\n";
    file << "    \"usePostProcess\": " << (gpuEmitterUsePostProcess_ ? 1 : 0) << "\n";
    file << "  },\n";

    file << "  \"emitter\": {\n";
    file << "    \"spawnShape\": " << gpuEmitterState_.spawnShape << ",\n";
    file << "    \"translate\": [" << gpuEmitterState_.translate.x << ", " << gpuEmitterState_.translate.y << ", " << gpuEmitterState_.translate.z << "],\n";
    file << "    \"radius\": " << gpuEmitterState_.radius << ",\n";
    file << "    \"count\": " << gpuEmitterState_.count << ",\n";
    file << "    \"frequency\": " << gpuEmitterState_.frequency << ",\n";
    file << "    \"baseScale\": [" << gpuEmitterState_.baseScale.x << ", " << gpuEmitterState_.baseScale.y << ", " << gpuEmitterState_.baseScale.z << "],\n";
    file << "    \"randomScale\": " << gpuEmitterState_.randomScale << ",\n";
    file << "    \"velocityScale\": [" << gpuEmitterState_.velocityScale.x << ", " << gpuEmitterState_.velocityScale.y << ", " << gpuEmitterState_.velocityScale.z << "],\n";
    file << "    \"lifeTime\": " << gpuEmitterState_.lifeTime << ",\n";
    file << "    \"colorMin\": [" << gpuEmitterState_.colorMin.x << ", " << gpuEmitterState_.colorMin.y << ", " << gpuEmitterState_.colorMin.z << ", " << gpuEmitterState_.colorMin.w << "],\n";
    file << "    \"colorMax\": [" << gpuEmitterState_.colorMax.x << ", " << gpuEmitterState_.colorMax.y << ", " << gpuEmitterState_.colorMax.z << ", " << gpuEmitterState_.colorMax.w << "],\n";
    file << "    \"scaleOverLife\": " << gpuEmitterState_.scaleOverLife << ",\n";
    file << "    \"endScale\": [" << gpuEmitterState_.endScale.x << ", " << gpuEmitterState_.endScale.y << ", " << gpuEmitterState_.endScale.z << "],\n";
    file << "    \"gravity\": [" << gpuEmitterState_.gravity.x << ", " << gpuEmitterState_.gravity.y << ", " << gpuEmitterState_.gravity.z << "],\n";
    file << "    \"damping\": " << gpuEmitterState_.damping << ",\n";
    file << "    \"colorOverLife\": " << gpuEmitterState_.colorOverLife << ",\n";
    file << "    \"endColor\": [" << gpuEmitterState_.endColor.x << ", " << gpuEmitterState_.endColor.y << ", " << gpuEmitterState_.endColor.z << ", " << gpuEmitterState_.endColor.w << "]\n";
    file << "  },\n";

    file << "  \"postProcess\": {\n";
    file << "    \"postProcessEnabled\": " << (gpuEmitterPostProcessEnabled_ ? 1 : 0) << ",\n";
    file << "    \"postEffectType\": " << gpuEmitterPostEffectType_ << ",\n";
    file << "    \"postRadialBlurCenter\": [" << postRadialBlurCenter.x << ", " << postRadialBlurCenter.y << ", " << postRadialBlurCenter.z << "],\n";
    file << "    \"postRadialBlurWidth\": " << gpuEmitterRadialBlurWidth_ << ",\n";
    file << "    \"postRadialBlurSampleCount\": " << gpuEmitterRadialBlurSampleCount_ << ",\n";
    file << "    \"postDistortionCenter\": [" << postDistortionCenter.x << ", " << postDistortionCenter.y << ", " << postDistortionCenter.z << "],\n";
    file << "    \"postDistortionStrength\": " << gpuEmitterDistortionStrength_ << ",\n";
    file << "    \"postDistortionRadius\": " << gpuEmitterDistortionRadius_ << ",\n";
    file << "    \"postDistortionWaveCount\": " << gpuEmitterDistortionWaveCount_ << ",\n";
    file << "    \"postDistortionProgress\": " << gpuEmitterDistortionProgress_ << ",\n";
    file << "    \"postDissolveThreshold\": " << gpuEmitterDissolveThreshold_ << ",\n";
    file << "    \"postDissolveEdgeWidth\": " << gpuEmitterDissolveEdgeWidth_ << ",\n";
    file << "    \"postDissolveEdgeColor\": [" << gpuEmitterDissolveEdgeColor_.x << ", " << gpuEmitterDissolveEdgeColor_.y << ", " << gpuEmitterDissolveEdgeColor_.z << "],\n";
    file << "    \"postRandomStrength\": " << gpuEmitterRandomStrength_ << ",\n";
    file << "    \"postRandomScale\": " << gpuEmitterRandomScale_ << ",\n";
    file << "    \"postRandomSpeed\": " << gpuEmitterRandomSpeed_ << "\n";
    file << "  }\n";
    file << "}\n";
}

/// <summary>
/// GPU Emitter設定をJSONファイルから読み込む。
/// </summary>
bool ParticleManager::LoadGpuEmitterSettings(const std::string& filePath)
{
    std::string jsonText; // 読み込んだJSON文字列
    if (!FileUtility::TryReadText(filePath, jsonText)) {
        Logger::Warn("ParticleManager::LoadGpuEmitterSettings: failed to open " + filePath + "\n");
        return false;
    }

    std::string effectSection = jsonText; // effectカテゴリの読み取り元
    std::string playbackSection = jsonText; // playbackカテゴリの読み取り元
    std::string renderSection = jsonText; // renderカテゴリの読み取り元
    std::string emitterSection = jsonText; // emitterカテゴリの読み取り元
    std::string postProcessSection = jsonText; // postProcessカテゴリの読み取り元
    JsonUtility::ExtractObjectSection(jsonText, "effect", effectSection);
    JsonUtility::ExtractObjectSection(jsonText, "playback", playbackSection);
    JsonUtility::ExtractObjectSection(jsonText, "render", renderSection);
    JsonUtility::ExtractObjectSection(jsonText, "emitter", emitterSection);
    JsonUtility::ExtractObjectSection(jsonText, "postProcess", postProcessSection);

    LoadGpuEmitterEffectSettings(effectSection, renderSection);
    LoadGpuEmitterPlaybackSettings(playbackSection, renderSection);
    LoadGpuEmitterPostProcessSettings(postProcessSection);
    LoadGpuEmitterStateSettings(emitterSection);
    NormalizeGpuEmitterStateAfterLoad();
    return true;
}

/// <summary>
/// JSONのeffect/renderカテゴリからGPU Emitterの基本情報を読み込む。
/// </summary>
void ParticleManager::LoadGpuEmitterEffectSettings(const std::string& effectSection, const std::string& renderSection)
{
    JsonUtility::ExtractString(effectSection, "effectName", gpuEmitterEffectName_);
    JsonUtility::ExtractString(effectSection, "description", gpuEmitterDescription_);
    if (JsonUtility::ExtractString(renderSection, "texture", gpuEmitterTexturePath_)) {
        ApplyGpuEmitterTextureToDrawGroup();
    }
}

/// <summary>
/// JSONのplayback/renderカテゴリからGPU Emitterの再生設定を読み込む。
/// </summary>
void ParticleManager::LoadGpuEmitterPlaybackSettings(const std::string& playbackSection, const std::string& renderSection)
{
    uint32_t autoEmit = gpuEmitterAutoEmit_ ? 1u : 0u; // JSON読み込み用の自動発生フラグ
    if (JsonUtility::ExtractUint(playbackSection, "autoEmit", autoEmit)) {
        gpuEmitterAutoEmit_ = autoEmit != 0;
    }

    uint32_t updateParticles = gpuParticleUpdateEnabled_ ? 1u : 0u; // JSON読み込み用の更新フラグ
    if (JsonUtility::ExtractUint(playbackSection, "updateParticles", updateParticles)) {
        gpuParticleUpdateEnabled_ = updateParticles != 0;
    }

    uint32_t drawParticles = gpuParticleDrawEnabled_ ? 1u : 0u; // JSON読み込み用の描画フラグ
    if (JsonUtility::ExtractUint(playbackSection, "drawParticles", drawParticles)) {
        gpuParticleDrawEnabled_ = drawParticles != 0;
    }

    uint32_t usePostProcess = gpuEmitterUsePostProcess_ ? 1u : 0u; // JSON読み込み用のPostProcess使用フラグ
    if (JsonUtility::ExtractUint(renderSection, "usePostProcess", usePostProcess)) {
        gpuEmitterUsePostProcess_ = usePostProcess != 0;
    }
}

/// <summary>
/// JSONのpostProcessカテゴリからGPU EmitterのPostProcess設定を読み込む。
/// </summary>
void ParticleManager::LoadGpuEmitterPostProcessSettings(const std::string& postProcessSection)
{
    uint32_t postProcessEnabled = gpuEmitterPostProcessEnabled_ ? 1u : 0u; // JSON読み込み用のPostProcess有効フラグ
    if (JsonUtility::ExtractUint(postProcessSection, "postProcessEnabled", postProcessEnabled)) {
        gpuEmitterPostProcessEnabled_ = postProcessEnabled != 0;
    }

    JsonUtility::ExtractUint(postProcessSection, "postEffectType", gpuEmitterPostEffectType_);
    Vector3 postRadialBlurCenter { gpuEmitterRadialBlurCenter_.x, gpuEmitterRadialBlurCenter_.y, 0.0f }; // JSON読込用RadialBlur中心
    if (JsonUtility::ExtractVector3(postProcessSection, "postRadialBlurCenter", postRadialBlurCenter)) {
        gpuEmitterRadialBlurCenter_ = { postRadialBlurCenter.x, postRadialBlurCenter.y };
    }
    JsonUtility::ExtractFloat(postProcessSection, "postRadialBlurWidth", gpuEmitterRadialBlurWidth_);
    JsonUtility::ExtractUint(postProcessSection, "postRadialBlurSampleCount", gpuEmitterRadialBlurSampleCount_);

    Vector3 postDistortionCenter { gpuEmitterDistortionCenter_.x, gpuEmitterDistortionCenter_.y, 0.0f }; // JSON読込用Distortion中心
    if (JsonUtility::ExtractVector3(postProcessSection, "postDistortionCenter", postDistortionCenter)) {
        gpuEmitterDistortionCenter_ = { postDistortionCenter.x, postDistortionCenter.y };
    }
    JsonUtility::ExtractFloat(postProcessSection, "postDistortionStrength", gpuEmitterDistortionStrength_);
    JsonUtility::ExtractFloat(postProcessSection, "postDistortionRadius", gpuEmitterDistortionRadius_);
    JsonUtility::ExtractFloat(postProcessSection, "postDistortionWaveCount", gpuEmitterDistortionWaveCount_);
    JsonUtility::ExtractFloat(postProcessSection, "postDistortionProgress", gpuEmitterDistortionProgress_);
    JsonUtility::ExtractFloat(postProcessSection, "postDissolveThreshold", gpuEmitterDissolveThreshold_);
    JsonUtility::ExtractFloat(postProcessSection, "postDissolveEdgeWidth", gpuEmitterDissolveEdgeWidth_);
    JsonUtility::ExtractVector3(postProcessSection, "postDissolveEdgeColor", gpuEmitterDissolveEdgeColor_);
    JsonUtility::ExtractFloat(postProcessSection, "postRandomStrength", gpuEmitterRandomStrength_);
    JsonUtility::ExtractFloat(postProcessSection, "postRandomScale", gpuEmitterRandomScale_);
    JsonUtility::ExtractFloat(postProcessSection, "postRandomSpeed", gpuEmitterRandomSpeed_);
}

/// <summary>
/// JSONのemitterカテゴリからGPU Emitterの発生設定を読み込む。
/// </summary>
void ParticleManager::LoadGpuEmitterStateSettings(const std::string& emitterSection)
{
    JsonUtility::ExtractUint(emitterSection, "spawnShape", gpuEmitterState_.spawnShape);
    JsonUtility::ExtractVector3(emitterSection, "translate", gpuEmitterState_.translate);
    JsonUtility::ExtractFloat(emitterSection, "radius", gpuEmitterState_.radius);
    JsonUtility::ExtractUint(emitterSection, "count", gpuEmitterState_.count);
    JsonUtility::ExtractFloat(emitterSection, "frequency", gpuEmitterState_.frequency);
    JsonUtility::ExtractVector3(emitterSection, "baseScale", gpuEmitterState_.baseScale);
    JsonUtility::ExtractFloat(emitterSection, "randomScale", gpuEmitterState_.randomScale);
    JsonUtility::ExtractVector3(emitterSection, "velocityScale", gpuEmitterState_.velocityScale);
    JsonUtility::ExtractFloat(emitterSection, "lifeTime", gpuEmitterState_.lifeTime);
    JsonUtility::ExtractVector4(emitterSection, "colorMin", gpuEmitterState_.colorMin);
    JsonUtility::ExtractVector4(emitterSection, "colorMax", gpuEmitterState_.colorMax);
    JsonUtility::ExtractUint(emitterSection, "scaleOverLife", gpuEmitterState_.scaleOverLife);
    JsonUtility::ExtractVector3(emitterSection, "endScale", gpuEmitterState_.endScale);
    JsonUtility::ExtractVector3(emitterSection, "gravity", gpuEmitterState_.gravity);
    JsonUtility::ExtractFloat(emitterSection, "damping", gpuEmitterState_.damping);
    JsonUtility::ExtractUint(emitterSection, "colorOverLife", gpuEmitterState_.colorOverLife);
    JsonUtility::ExtractVector4(emitterSection, "endColor", gpuEmitterState_.endColor);
}

/// <summary>
/// GPU Emitter設定を実行時に扱える範囲へ整える。
/// </summary>
void ParticleManager::NormalizeGpuEmitterStateForRuntime()
{
    gpuEmitterState_.count = (std::min)(gpuEmitterState_.count, GetParticleLimit());
    gpuEmitterState_.frequency = (std::max)(gpuEmitterState_.frequency, 0.001f);
    gpuEmitterState_.radius = (std::max)(gpuEmitterState_.radius, 0.0f);
    gpuEmitterState_.randomScale = (std::max)(gpuEmitterState_.randomScale, 0.0f);
    gpuEmitterState_.lifeTime = std::clamp(gpuEmitterState_.lifeTime, kImGuiLifeMin, kImGuiLifeMax);
    gpuEmitterState_.damping = std::clamp(gpuEmitterState_.damping, kImGuiDampingMin, kImGuiDampingMax);
    gpuEmitterState_.scaleOverLife = gpuEmitterState_.scaleOverLife != 0 ? 1u : 0u;
    gpuEmitterState_.colorOverLife = gpuEmitterState_.colorOverLife != 0 ? 1u : 0u;
    gpuEmitterState_.spawnShape = (std::min)(gpuEmitterState_.spawnShape, 3u);
}

/// <summary>
/// 読み込み後のGPU Emitter設定を実行時に扱える範囲へ整える。
/// </summary>
void ParticleManager::NormalizeGpuEmitterStateAfterLoad()
{
    NormalizeGpuEmitterStateForRuntime();
    gpuEmitterState_.frequencyTime = gpuEmitterState_.frequency;
    ClearGpuEmitterRuntimeParticleState();
}

/// <summary>
/// GPU Emitterプリセットを名前から読み込む。
/// </summary>
bool ParticleManager::LoadGpuEmitterPreset(const std::string& presetName)
{
    const std::filesystem::path presetPath = GpuEmitterSettingsUtility::ResolvePresetPath(presetName); // 読み込むプリセットファイル
    const std::string loadPath = presetPath.generic_string(); // ログと読み込みに使うパス文字列
    if (!LoadGpuEmitterSettings(loadPath)) {
        gpuEmitterSettingsMessage_ = "Preset load failed: " + presetName;
        return false;
    }

    gpuEmitterLoadedSettingsName_ = FileUtility::GetStem(loadPath);
    gpuEmitterSettingsName_ = gpuEmitterLoadedSettingsName_;
    gpuEmitterSettingsMessage_ = "Preset loaded: " + loadPath;
    return true;
}

/// <summary>
/// 現在のGPU Emitter設定で次フレームに1回だけ発生させる。
/// </summary>
void ParticleManager::RequestGpuEmitterEmit()
{
    gpuEmitterManualEmitRequested_ = true;
}

/// <summary>
/// GPU Emitterプリセットを読み込み、設定内の位置で1回だけ発生させる。
/// </summary>
bool ParticleManager::PlayGpuEmitterPreset(const std::string& presetName)
{
    if (!LoadGpuEmitterPreset(presetName)) {
        return false;
    }

    gpuEmitterAutoEmit_ = false;
    RequestGpuEmitterEmit();
    return true;
}

/// <summary>
/// GPU Emitterプリセットを読み込み、指定位置で1回だけ発生させる。
/// </summary>
bool ParticleManager::PlayGpuEmitterPreset(const std::string& presetName, const Vector3& position)
{
    if (!LoadGpuEmitterPreset(presetName)) {
        return false;
    }

    gpuEmitterAutoEmit_ = false;
    gpuEmitterState_.translate = position;
    RequestGpuEmitterEmit();
    return true;
}
/// <summary>
/// GPU Emitter用テクスチャを描画グループへ反映する。
/// </summary>
void ParticleManager::ApplyGpuEmitterTextureToDrawGroup()
{
    if (gpuEmitterTexturePath_.empty() || particleGroups_.empty()) {
        return;
    }

    ParticleGroup& drawGroup = particleGroups_.begin()->second; // GPU Particle描画に使う先頭グループ
    drawGroup.texturePath = gpuEmitterTexturePath_;
    if (texManager_) {
        uint32_t textureIndex = texManager_->GetTextureIndexByFilePath(gpuEmitterTexturePath_); // 反映するテクスチャのSRV番号
        if (textureIndex == UINT32_MAX) {
            texManager_->LoadTexture(gpuEmitterTexturePath_);
            textureIndex = texManager_->GetTextureIndexByFilePath(gpuEmitterTexturePath_);
        }
        drawGroup.srvIndex = textureIndex == UINT32_MAX ? drawGroup.srvIndex : textureIndex;
    }
    if (drawGroup.renderObject) {
        drawGroup.renderObject->SetTexture(gpuEmitterTexturePath_);
    }
}
/// <summary>
/// GPUへ渡すパーティクル入力データを現在のグループ内容から作成する。
/// </summary>
uint32_t ParticleManager::UploadGpuParticleSource(const ParticleGroup& group, uint32_t count, const Matrix4x4& view, const Matrix4x4& projection)
{
    if (!dxCommon_) {
        return 0;
    }

        const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex(); // 更新対象のフレーム番号
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
        gpuParticle.startScale = particle.transform.scale;
        gpuParticle.padding2 = 0.0f;
        gpuParticle.startColor = particle.color;
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

        const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex(); // 更新対象のフレーム番号
    if (gpuParticleInitialized_[frameIndex]) {
        return true;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList(); // Dispatchを発行するコマンドリスト
    if (!commandList || !computeRootSignature_ || !initializeParticlePipelineState_ || !gpuParticleOutputResources_[frameIndex] || !gpuFreeCounterResources_[frameIndex] || !gpuFreeListResources_[frameIndex]) {
        return false;
    }

    D3D12_RESOURCE_STATES& outputState = gpuParticleOutputStates_[frameIndex]; // Particle Resourceの現在状態
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

    D3D12_RESOURCE_STATES& freeListState = gpuFreeListStates_[frameIndex];
    if (freeListState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER freeListBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            gpuFreeListResources_[frameIndex].Get(),
            freeListState,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &freeListBarrier);
        freeListState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(initializeParticlePipelineState_.Get());
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterOutputUav, gpuParticleOutputUavIndices_[frameIndex]);
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterFreeCounterUav, gpuFreeCounterUavIndices_[frameIndex]);
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterFreeListUav, gpuFreeListUavIndices_[frameIndex]);
    const uint32_t dispatchGroupCount = (GetParticleLimit() + kGpuParticleThreadCount - 1) / kGpuParticleThreadCount; // 初期化対象のDispatch数
    commandList->Dispatch(dispatchGroupCount, 1, 1);

    D3D12_RESOURCE_BARRIER uavBarriers[] = {
        CD3DX12_RESOURCE_BARRIER::UAV(gpuParticleOutputResources_[frameIndex].Get()),
        CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeCounterResources_[frameIndex].Get()),
        CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeListResources_[frameIndex].Get()),
    };
    commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);
    CopyGpuFreeCounterToReadback(frameIndex);

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

        const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex(); // 更新対象のフレーム番号
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList(); // Dispatchを発行するコマンドリスト
    if (!commandList || !computeRootSignature_ || !emitParticlePipelineState_ || !gpuParticleOutputResources_[frameIndex] || !gpuFreeCounterResources_[frameIndex] || !gpuFreeListResources_[frameIndex]) {
        return false;
    }

    if (gpuEmitterData_[frameIndex]) {
        *gpuEmitterData_[frameIndex] = gpuEmitterState_;
    }
    if (gpuPerFrameData_[frameIndex]) {
        *gpuPerFrameData_[frameIndex] = gpuPerFrameState_;
    }

    D3D12_RESOURCE_STATES& outputState = gpuParticleOutputStates_[frameIndex]; // Particle Resourceの現在状態
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

    D3D12_RESOURCE_STATES& freeListState = gpuFreeListStates_[frameIndex];
    if (freeListState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER freeListBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            gpuFreeListResources_[frameIndex].Get(),
            freeListState,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &freeListBarrier);
        freeListState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }


    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(emitParticlePipelineState_.Get());
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterOutputUav, gpuParticleOutputUavIndices_[frameIndex]);
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterFreeCounterUav, gpuFreeCounterUavIndices_[frameIndex]);
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterFreeListUav, gpuFreeListUavIndices_[frameIndex]);
    commandList->SetComputeRootConstantBufferView(kComputeRootParameterEmitterCbv, gpuEmitterResources_[frameIndex]->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(kComputeRootParameterPerFrameCbv, gpuPerFrameResources_[frameIndex]->GetGPUVirtualAddress());
    const uint32_t emitGroupCount = (gpuEmitterState_.count + kGpuEmitterThreadCount - 1) / kGpuEmitterThreadCount; // 発生数に応じたDispatch数
    commandList->Dispatch(emitGroupCount, 1, 1);

    D3D12_RESOURCE_BARRIER uavBarriers[] = {
        CD3DX12_RESOURCE_BARRIER::UAV(gpuParticleOutputResources_[frameIndex].Get()),
        CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeCounterResources_[frameIndex].Get()),
        CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeListResources_[frameIndex].Get()),
    };
    commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);
    CopyGpuFreeCounterToReadback(frameIndex);

    D3D12_RESOURCE_BARRIER toSrvBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        gpuParticleOutputResources_[frameIndex].Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &toSrvBarrier);
    outputState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    return true;
}


/// <summary>
/// GPU上のParticleを経過時間で更新する。
/// </summary>
bool ParticleManager::DispatchUpdateGpuParticles()
{
    if (!gpuParticleReady_ || !dxCommon_ || !srvManager_ || gpuEmitterVisibleCount_ == 0) {
        return true;
    }

        const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex(); // 更新対象のフレーム番号
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList(); // Dispatchを発行するコマンドリスト
    if (!commandList || !computeRootSignature_ || !updateParticlePipelineState_ || !gpuParticleOutputResources_[frameIndex] || !gpuFreeCounterResources_[frameIndex] || !gpuFreeListResources_[frameIndex]) {
        return false;
    }

    if (gpuPerFrameData_[frameIndex]) {
        *gpuPerFrameData_[frameIndex] = gpuPerFrameState_;
    }

    D3D12_RESOURCE_STATES& outputState = gpuParticleOutputStates_[frameIndex]; // Particle Resourceの現在状態
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

    D3D12_RESOURCE_STATES& freeListState = gpuFreeListStates_[frameIndex];
    if (freeListState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER freeListBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            gpuFreeListResources_[frameIndex].Get(),
            freeListState,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &freeListBarrier);
        freeListState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(updateParticlePipelineState_.Get());
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterOutputUav, gpuParticleOutputUavIndices_[frameIndex]);
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterFreeCounterUav, gpuFreeCounterUavIndices_[frameIndex]);
    srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterFreeListUav, gpuFreeListUavIndices_[frameIndex]);
    commandList->SetComputeRootConstantBufferView(kComputeRootParameterPerFrameCbv, gpuPerFrameResources_[frameIndex]->GetGPUVirtualAddress());
    const uint32_t updateGroupCount = (GetParticleLimit() + kGpuParticleThreadCount - 1) / kGpuParticleThreadCount; // 更新対象のDispatch数
    commandList->Dispatch(updateGroupCount, 1, 1);

    D3D12_RESOURCE_BARRIER uavBarriers[] = {
        CD3DX12_RESOURCE_BARRIER::UAV(gpuParticleOutputResources_[frameIndex].Get()),
        CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeCounterResources_[frameIndex].Get()),
        CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeListResources_[frameIndex].Get()),
    };
    commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);
    CopyGpuFreeCounterToReadback(frameIndex);

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
    gpuPerFrameState_.deltaTime = gpuParticleUpdateEnabled_ ? dt : 0.0f;
    gpuPerFrameState_.gravity = gpuEmitterState_.gravity;
    gpuPerFrameState_.damping = gpuEmitterState_.damping;
    gpuPerFrameState_.endScale = gpuEmitterState_.endScale;
    gpuPerFrameState_.endColor = gpuEmitterState_.endColor;
    gpuPerFrameState_.scaleOverLife = gpuEmitterState_.scaleOverLife;
    gpuPerFrameState_.colorOverLife = gpuEmitterState_.colorOverLife;
    gpuEmitterState_.emit = 0;

    if (gpuEmitterManualEmitRequested_) {
        gpuEmitterManualEmitRequested_ = false;
        gpuEmitterState_.emit = 1;
        gpuEmitterVisibleCount_ = (std::min)(GetParticleLimit(), gpuEmitterVisibleCount_ + gpuEmitterState_.count);
        return;
    }

    if (!gpuEmitterAutoEmit_) {
        return;
    }

    gpuEmitterState_.frequencyTime += dt;
    if (gpuEmitterState_.frequencyTime >= gpuEmitterState_.frequency) {
        gpuEmitterState_.frequencyTime -= gpuEmitterState_.frequency;
        gpuEmitterState_.emit = 1;
        gpuEmitterVisibleCount_ = (std::min)(GetParticleLimit(), gpuEmitterVisibleCount_ + gpuEmitterState_.count);
    }
}
bool ParticleManager::DispatchGpuParticleTransform(uint32_t count)
{
    if (!gpuParticleReady_ || count == 0 || !dxCommon_ || !srvManager_) {
        return false;
    }

        const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex(); // 更新対象のフレーム番号
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList(); // Dispatchを発行するコマンドリスト
    if (!commandList || !computeRootSignature_ || !computePipelineState_ || !gpuParticleOutputResources_[frameIndex]) {
        return false;
    }

    D3D12_RESOURCE_STATES& outputState = gpuParticleOutputStates_[frameIndex]; // Particle Resourceの現在状態
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

        const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex(); // 更新対象のフレーム番号
        object3dCommon_->SetInstancingSrvOverride(gpuParticleOutputSrvHandlesGPU_[frameIndex]);
        object3dCommon_->SetInstancingDrawSetting();

        if (auto* model = renderObject->GetModel()) {
            model->DrawInstanced(renderObject, count);
        } else {
            renderObject->DrawInstanced(count);
        }
        object3dCommon_->ClearInstancingSrvOverride();
    }

    if (gpuParticleDrawEnabled_ && gpuEmitterVisibleCount_ > 0 && !particleGroups_.empty()) {
        auto drawGroupIterator = particleGroups_.begin();
        ParticleGroup& gpuEmitterGroup = drawGroupIterator->second;
        Object3d* renderObject = gpuEmitterGroup.renderObject ? gpuEmitterGroup.renderObject : particlePlane_;
        const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex(); // 更新対象のフレーム番号
        if (renderObject && DispatchInitializeGpuParticles() && DispatchEmitGpuParticles() && DispatchUpdateGpuParticles()) {
            const uint32_t drawCount = (std::min)(GetParticleLimit(), instancingSlots);
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
/// GPU Emitterの実行時パーティクル状態をクリアする。
/// </summary>
void ParticleManager::ClearGpuEmitterRuntimeParticleState()
{
    gpuEmitterVisibleCount_ = 0;
    gpuAliveCountEstimate_ = 0;
    for (bool& initialized : gpuParticleInitialized_) {
        initialized = false;
    }
}
/// <summary>
/// 現在のPostProcess設定をGPU Emitter設定へ取り込む。
/// </summary>
void ParticleManager::CaptureGpuEmitterPostProcessSettings(const PostProcess& postProcess)
{
    gpuEmitterPostProcessEnabled_ = postProcess.IsEnabled();
    gpuEmitterPostEffectType_ = static_cast<uint32_t>(postProcess.GetEffectType());
    gpuEmitterRadialBlurCenter_ = postProcess.GetRadialBlurCenter();
    gpuEmitterRadialBlurWidth_ = postProcess.GetRadialBlurWidth();
    gpuEmitterRadialBlurSampleCount_ = postProcess.GetRadialBlurSampleCount();
    gpuEmitterDistortionCenter_ = postProcess.GetDistortionCenter();
    gpuEmitterDistortionStrength_ = postProcess.GetDistortionStrength();
    gpuEmitterDistortionRadius_ = postProcess.GetDistortionRadius();
    gpuEmitterDistortionWaveCount_ = postProcess.GetDistortionWaveCount();
    gpuEmitterDistortionProgress_ = postProcess.GetDistortionProgress();
    gpuEmitterDissolveThreshold_ = postProcess.GetDissolveThreshold();
    gpuEmitterDissolveEdgeWidth_ = postProcess.GetDissolveEdgeWidth();
    gpuEmitterDissolveEdgeColor_ = postProcess.GetDissolveEdgeColor();
    gpuEmitterRandomStrength_ = postProcess.GetRandomStrength();
    gpuEmitterRandomScale_ = postProcess.GetRandomScale();
    gpuEmitterRandomSpeed_ = postProcess.GetRandomSpeed();
}

/// <summary>
/// GPU Emitterに保存しているPostProcess設定を反映する。
/// </summary>
void ParticleManager::ApplyGpuEmitterPostProcessSettings(PostProcess& postProcess) const
{
    const uint32_t maxPostEffectType = static_cast<uint32_t>(PostEffectType::Count) - 1u; // PostEffectTypeの最大番号
    const uint32_t postEffectType = (std::min)(gpuEmitterPostEffectType_, maxPostEffectType); // PostEffectTypeの有効範囲に丸めた値
    postProcess.SetEnabled(gpuEmitterPostProcessEnabled_);
    postProcess.SetEffectType(static_cast<PostEffectType>(postEffectType));
    postProcess.SetRadialBlurCenter(gpuEmitterRadialBlurCenter_);
    postProcess.SetRadialBlurWidth(gpuEmitterRadialBlurWidth_);
    postProcess.SetRadialBlurSampleCount(gpuEmitterRadialBlurSampleCount_);
    postProcess.SetDistortionCenter(gpuEmitterDistortionCenter_);
    postProcess.SetDistortionStrength(gpuEmitterDistortionStrength_);
    postProcess.SetDistortionRadius(gpuEmitterDistortionRadius_);
    postProcess.SetDistortionWaveCount(gpuEmitterDistortionWaveCount_);
    postProcess.SetDistortionProgress(gpuEmitterDistortionProgress_);
    postProcess.SetDissolveThreshold(gpuEmitterDissolveThreshold_);
    postProcess.SetDissolveEdgeWidth(gpuEmitterDissolveEdgeWidth_);
    postProcess.SetDissolveEdgeColor(gpuEmitterDissolveEdgeColor_);
    postProcess.SetRandomStrength(gpuEmitterRandomStrength_);
    postProcess.SetRandomScale(gpuEmitterRandomScale_);
    postProcess.SetRandomSpeed(gpuEmitterRandomSpeed_);
}

/// <summary>
/// GPU Emitterプリセット状態を現在の設定へ適用する。
/// </summary>
void ParticleManager::ApplyGpuEmitterPresetState(const PM_GpuEmitterSphere& presetState)
{
    gpuEmitterState_ = presetState;
    NormalizeGpuEmitterStateForRuntime();
    gpuEmitterState_.frequencyTime = gpuEmitterState_.frequency;
    gpuEmitterState_.emit = 0;
    ClearGpuEmitterRuntimeParticleState();
}

/// <summary>
/// GPU Emitterの基本プリセットを適用する。
/// </summary>
void ParticleManager::ApplyGpuEmitterBasicSettings()
{
    PM_GpuEmitterSphere presetState = gpuEmitterState_; // 適用する基本プリセット状態
    presetState.count = 24;
    presetState.frequency = 0.10f;
    presetState.radius = 1.6f;
    presetState.baseScale = { 0.22f, 0.22f, 0.22f };
    presetState.randomScale = 0.10f;
    presetState.velocityScale = { 0.35f, 0.35f, 0.35f };
    presetState.lifeTime = 1.2f;
    presetState.colorMin = { 0.85f, 0.85f, 0.95f, 1.0f };
    presetState.colorMax = { 1.0f, 1.0f, 1.0f, 1.0f };
    presetState.scaleOverLife = 1;
    presetState.endScale = { 0.02f, 0.02f, 0.02f };
    presetState.colorOverLife = 1;
    presetState.endColor = { 1.0f, 1.0f, 1.0f, 0.0f };
    presetState.gravity = { 0.0f, -0.3f, 0.0f };
    presetState.damping = 0.0f;
    ApplyGpuEmitterPresetState(presetState);
}

/// <summary>
/// GPU Emitterの密集バーストプリセットを適用する。
/// </summary>
void ParticleManager::ApplyGpuEmitterDenseBurstSettings()
{
    PM_GpuEmitterSphere presetState = gpuEmitterState_; // 適用する密集バーストプリセット状態
    presetState.count = 256;
    presetState.frequency = 0.01f;
    presetState.radius = 0.9f;
    presetState.baseScale = { 0.16f, 0.16f, 0.16f };
    presetState.randomScale = 0.10f;
    presetState.velocityScale = { 0.18f, 0.35f, 0.18f };
    presetState.lifeTime = 2.5f;
    presetState.colorMin = { 0.55f, 0.35f, 0.45f, 1.0f };
    presetState.colorMax = { 1.0f, 1.0f, 0.65f, 1.0f };
    presetState.scaleOverLife = 1;
    presetState.endScale = { 0.05f, 0.05f, 0.05f };
    presetState.colorOverLife = 1;
    presetState.endColor = { 1.0f, 0.35f, 0.15f, 0.0f };
    presetState.gravity = { 0.0f, -0.2f, 0.0f };
    presetState.damping = 0.2f;
    ApplyGpuEmitterPresetState(presetState);
}

/// <summary>
/// GPU Emitterのランダム拡散プリセットを適用する。
/// </summary>
void ParticleManager::ApplyGpuEmitterRandomSpreadSettings()
{
    PM_GpuEmitterSphere presetState = gpuEmitterState_; // 適用するランダム拡散プリセット状態
    presetState.count = 1024;
    presetState.frequency = 0.12f;
    presetState.radius = 1.3f;
    presetState.baseScale = { 0.08f, 0.08f, 0.08f };
    presetState.randomScale = 0.02f;
    presetState.velocityScale = { 0.08f, 0.10f, 0.08f };
    presetState.lifeTime = 5.0f;
    presetState.colorMin = { 0.35f, 0.55f, 0.9f, 1.0f };
    presetState.colorMax = { 1.0f, 1.0f, 1.0f, 1.0f };
    presetState.scaleOverLife = 0;
    presetState.endScale = { 0.02f, 0.02f, 0.02f };
    presetState.colorOverLife = 0;
    presetState.endColor = { 0.2f, 0.4f, 1.0f, 0.0f };
    presetState.gravity = { 0.0f, -0.05f, 0.0f };
    presetState.damping = 0.05f;
    ApplyGpuEmitterPresetState(presetState);
}

/// <summary>
/// GPU EmitterのGPU側パーティクル状態をリセットする。
/// </summary>
void ParticleManager::ResetGpuEmitterParticles()
{
    ClearGpuEmitterRuntimeParticleState();
    gpuEmitterState_.frequencyTime = 0.0f;
    gpuEmitterState_.emit = 0;
}

#ifdef USE_IMGUI
/// <summary>
/// ImGuiでGPU Emitterの基本情報を表示する。
/// </summary>
void ParticleManager::DrawGpuEmitterStatusImGui()
{
    UpdateGpuAliveCountEstimate();
    ImGui::Text("Ready: %s", gpuParticleReady_ ? "true" : "false");
    ImGui::Text("GPU Draw Request: %u / %u", gpuEmitterVisibleCount_, GetParticleLimit());
    ImGui::Text("GPU Alive Estimate: %u / %u", gpuAliveCountEstimate_, GetParticleLimit());
}

/// <summary>
/// ImGuiでGPU Emitterのeffect情報を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterEffectImGui()
{
    char effectNameBuffer[64] {}; // effect名入力用バッファ
    std::snprintf(effectNameBuffer, sizeof(effectNameBuffer), "%s", gpuEmitterEffectName_.c_str());
    if (ImGui::InputText("Effect Name", effectNameBuffer, sizeof(effectNameBuffer))) {
        gpuEmitterEffectName_ = effectNameBuffer;
    }

    char descriptionBuffer[160] {}; // 説明文入力用バッファ
    std::snprintf(descriptionBuffer, sizeof(descriptionBuffer), "%s", gpuEmitterDescription_.c_str());
    if (ImGui::InputTextMultiline("Description", descriptionBuffer, sizeof(descriptionBuffer), ImVec2(0.0f, 42.0f))) {
        gpuEmitterDescription_ = descriptionBuffer;
    }

    char texturePathBuffer[128] {}; // GPU描画に使うテクスチャパス入力用バッファ
    std::snprintf(texturePathBuffer, sizeof(texturePathBuffer), "%s", gpuEmitterTexturePath_.c_str());
    if (ImGui::InputText("GPU Texture", texturePathBuffer, sizeof(texturePathBuffer))) {
        gpuEmitterTexturePath_ = texturePathBuffer;
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply GPU Texture")) {
        ApplyGpuEmitterTextureToDrawGroup();
    }
}

/// <summary>
/// ImGuiでGPU Emitterに紐づくPostProcess設定を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterPostProcessImGui(PostProcess* postProcess)
{
    ImGui::Checkbox("Use Saved PostProcess", &gpuEmitterUsePostProcess_);
    ImGui::SameLine();
    if (postProcess && ImGui::Button("Capture PostProcess")) {
        CaptureGpuEmitterPostProcessSettings(*postProcess);
        gpuEmitterSettingsMessage_ = "Captured current PostProcess settings";
    }
    ImGui::SameLine();
    if (postProcess && ImGui::Button("Apply PostProcess")) {
        ApplyGpuEmitterPostProcessSettings(*postProcess);
        gpuEmitterSettingsMessage_ = "Applied saved PostProcess settings";
    }
}

/// <summary>
/// ImGuiでGPU Emitterの発生設定を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterStateImGui()
{
    DrawGpuEmitterPlaybackStateImGui();
    DrawGpuEmitterSpawnStateImGui();
    DrawGpuEmitterScaleLifeStateImGui();
    DrawGpuEmitterPhysicsStateImGui();
    DrawGpuEmitterColorStateImGui();
    NormalizeGpuEmitterStateForRuntime();
}

/// <summary>
/// ImGuiでGPU Emitterの再生フラグを編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterPlaybackStateImGui()
{
    ImGui::Checkbox("Auto Emit", &gpuEmitterAutoEmit_);
    ImGui::SameLine();
    ImGui::Checkbox("Update GPU Particles", &gpuParticleUpdateEnabled_);
    ImGui::SameLine();
    ImGui::Checkbox("Draw GPU Particles", &gpuParticleDrawEnabled_);
}

/// <summary>
/// ImGuiでGPU Emitterの発生範囲と発生数を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterSpawnStateImGui()
{
    ImGui::DragFloat3("Emitter Position", &gpuEmitterState_.translate.x, kImGuiFineStep, kImGuiSpawnPositionMin, kImGuiSpawnPositionMax);
    ImGui::DragFloat("Emitter Radius", &gpuEmitterState_.radius, kImGuiFineStep, 0.0f, kImGuiSpawnPositionMax);

    const char* spawnShapeLabels[] = { "Sphere", "Box", "Ring", "Cone" }; // ImGui表示用の発生形状名
    constexpr int spawnShapeCount = 4; // 選択できる発生形状数
    int spawnShapeIndex = static_cast<int>((std::min)(gpuEmitterState_.spawnShape, static_cast<uint32_t>(spawnShapeCount - 1))); // ImGui編集用の発生形状番号
    if (ImGui::Combo("Spawn Shape", &spawnShapeIndex, spawnShapeLabels, spawnShapeCount)) {
        gpuEmitterState_.spawnShape = static_cast<uint32_t>(spawnShapeIndex);
    }

    int gpuEmitCount = static_cast<int>(gpuEmitterState_.count); // ImGui編集用の射出数
    if (ImGui::SliderInt("Emit Count", &gpuEmitCount, 0, static_cast<int>(GetParticleLimit()))) {
        gpuEmitterState_.count = static_cast<uint32_t>((std::max)(gpuEmitCount, 0));
    }

    ImGui::DragFloat("Frequency", &gpuEmitterState_.frequency, kImGuiFineStep, 0.001f, 10.0f);
}

/// <summary>
/// ImGuiでGPU Emitterのスケールと寿命を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterScaleLifeStateImGui()
{
    ImGui::DragFloat3("Base Scale", &gpuEmitterState_.baseScale.x, kImGuiFineStep, kImGuiScaleMin, kImGuiScaleMax);
    ImGui::DragFloat("Random Scale", &gpuEmitterState_.randomScale, kImGuiFineStep, 0.0f, kImGuiScaleMax);

    ImGui::DragFloat3("Velocity Scale", &gpuEmitterState_.velocityScale.x, kImGuiFineStep, kImGuiPhysicsMin, kImGuiPhysicsMax);
    ImGui::DragFloat("Life Time", &gpuEmitterState_.lifeTime, kImGuiFineStep, kImGuiLifeMin, kImGuiLifeMax);

    bool scaleOverLife = gpuEmitterState_.scaleOverLife != 0; // 寿命に応じてスケールを変えるか
    if (ImGui::Checkbox("Scale Over Life", &scaleOverLife)) {
        gpuEmitterState_.scaleOverLife = scaleOverLife ? 1u : 0u;
    }
    ImGui::DragFloat3("End Scale", &gpuEmitterState_.endScale.x, kImGuiFineStep, 0.0f, kImGuiScaleMax);
}

/// <summary>
/// ImGuiでGPU Emitterの物理挙動を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterPhysicsStateImGui()
{
    ImGui::DragFloat3("Gravity", &gpuEmitterState_.gravity.x, kImGuiPhysicsStep, kImGuiPhysicsMin, kImGuiPhysicsMax);
    ImGui::DragFloat("Damping", &gpuEmitterState_.damping, kImGuiFineStep, kImGuiDampingMin, kImGuiDampingMax);
}

/// <summary>
/// ImGuiでGPU Emitterの色変化を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterColorStateImGui()
{
    ImGui::ColorEdit4("GPU Color Min", &gpuEmitterState_.colorMin.x);
    ImGui::ColorEdit4("GPU Color Max", &gpuEmitterState_.colorMax.x);
    bool colorOverLife = gpuEmitterState_.colorOverLife != 0; // 寿命に応じて色を変えるか
    if (ImGui::Checkbox("Color Over Life", &colorOverLife)) {
        gpuEmitterState_.colorOverLife = colorOverLife ? 1u : 0u;
    }
    ImGui::ColorEdit4("End Color", &gpuEmitterState_.endColor.x);
}
/// <summary>
/// ImGuiでGPU Emitterのプリセット適用ボタンを表示する。
/// </summary>
void ParticleManager::DrawGpuEmitterPresetImGui()
{
    if (ImGui::Button("Apply Basic Particle Settings")) {
        ApplyGpuEmitterBasicSettings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply Dense Burst Settings")) {
        ApplyGpuEmitterDenseBurstSettings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply Random Spread Settings")) {
        ApplyGpuEmitterRandomSpreadSettings();
    }
}

/// <summary>
/// ImGuiでGPU Emitter設定ファイルの保存と読み込みを操作する。
/// </summary>
void ParticleManager::DrawGpuEmitterSettingsFileImGui()
{
    DrawGpuEmitterSettingsNameImGui();

    const std::vector<std::string> settingsFiles = GpuEmitterSettingsUtility::CollectSettingsFiles(); // 読み込み候補のJSON一覧
    const std::string saveSettingsPath = GpuEmitterSettingsUtility::BuildSettingsPath(gpuEmitterSettingsName_); // 保存先JSONパス
    const std::string selectedSettingsPath = GpuEmitterSettingsUtility::ResolveSettingsPath(gpuEmitterSettingsName_, settingsFiles); // 読み込み対象JSONパス
    const std::string loadedPresetName = gpuEmitterLoadedSettingsName_.empty() ? "None" : gpuEmitterLoadedSettingsName_; // 表示用のロード済み設定名
    std::string settingsPreview = GpuEmitterSettingsUtility::SanitizeName(gpuEmitterSettingsName_); // コンボ表示用の設定名
    if (settingsPreview.empty()) {
        settingsPreview = "gpu_particle";
    }

    ImGui::Text("Loaded Preset: %s", loadedPresetName.c_str());
    ImGui::Text("Save Path: %s", saveSettingsPath.c_str());
    ImGui::Text("Selected File: %s", selectedSettingsPath.c_str());
    ImGui::Text("Load Files: %zu", settingsFiles.size());

    DrawGpuEmitterSettingsFileComboImGui(settingsFiles, settingsPreview);
    DrawGpuEmitterSettingsFileButtonsImGui(saveSettingsPath, selectedSettingsPath);

    if (!gpuEmitterSettingsMessage_.empty()) {
        ImGui::TextWrapped("%s", gpuEmitterSettingsMessage_.c_str());
    }
}

/// <summary>
/// ImGuiでGPU Emitter設定名を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterSettingsNameImGui()
{
    char settingsNameBuffer[64] {}; // 設定名入力用バッファ
    std::snprintf(settingsNameBuffer, sizeof(settingsNameBuffer), "%s", gpuEmitterSettingsName_.c_str());
    if (ImGui::InputText("Settings Name", settingsNameBuffer, sizeof(settingsNameBuffer))) {
        gpuEmitterSettingsName_ = settingsNameBuffer;
    }
}

/// <summary>
/// ImGuiでGPU Emitter設定ファイルの選択欄を表示する。
/// </summary>
void ParticleManager::DrawGpuEmitterSettingsFileComboImGui(const std::vector<std::string>& settingsFiles, const std::string& settingsPreview)
{
    if (ImGui::BeginCombo("Load File", settingsPreview.c_str())) {
        if (settingsFiles.empty()) {
            ImGui::TextDisabled("No json files in resources/effects");
        }
        for (const std::string& filePath : settingsFiles) {
            const std::string stemName = FileUtility::GetStem(filePath); // 選択表示用のファイル名
            const bool isSelected = stemName == settingsPreview; // 現在選択中か
            const std::string selectableLabel = stemName + "##" + filePath; // 表示名とImGui内部IDを分けるラベル
            if (ImGui::Selectable(selectableLabel.c_str(), isSelected)) {
                gpuEmitterSettingsName_ = stemName;
                gpuEmitterSettingsMessage_ = "Selected: " + filePath;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

/// <summary>
/// ImGuiでGPU Emitter設定ファイルの操作ボタンを表示する。
/// </summary>
void ParticleManager::DrawGpuEmitterSettingsFileButtonsImGui(const std::string& saveSettingsPath, const std::string& selectedSettingsPath)
{
    if (ImGui::Button("Save GPU Settings")) {
        SaveGpuEmitterSettingsFromImGui(saveSettingsPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load GPU Settings")) {
        LoadGpuEmitterSettingsFromImGui(selectedSettingsPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Selected")) {
        DeleteGpuEmitterSettingsFromImGui(selectedSettingsPath);
    }
}

/// <summary>
/// GPU Emitter設定を指定パスへ保存して結果メッセージを更新する。
/// </summary>
void ParticleManager::SaveGpuEmitterSettingsFromImGui(const std::string& saveSettingsPath)
{
    const bool willOverwrite = FileUtility::Exists(saveSettingsPath); // 既存ファイルを上書きするか
    if (SaveGpuEmitterSettings(saveSettingsPath)) {
        gpuEmitterLoadedSettingsName_ = FileUtility::GetStem(saveSettingsPath);
        gpuEmitterSettingsMessage_ = std::string(willOverwrite ? "Overwritten: " : "Saved: ") + saveSettingsPath;
    } else {
        gpuEmitterSettingsMessage_ = "Save failed: " + saveSettingsPath;
    }
}

/// <summary>
/// GPU Emitter設定を指定パスから読み込んで結果メッセージを更新する。
/// </summary>
void ParticleManager::LoadGpuEmitterSettingsFromImGui(const std::string& loadSettingsPath)
{
    if (LoadGpuEmitterSettings(loadSettingsPath)) {
        gpuEmitterLoadedSettingsName_ = FileUtility::GetStem(loadSettingsPath);
        gpuEmitterSettingsName_ = gpuEmitterLoadedSettingsName_;
        gpuEmitterSettingsMessage_ = "Loaded: " + loadSettingsPath;
    } else {
        gpuEmitterSettingsMessage_ = "Load failed: " + loadSettingsPath;
    }
}

/// <summary>
/// GPU Emitter設定ファイルを削除して結果メッセージを更新する。
/// </summary>
void ParticleManager::DeleteGpuEmitterSettingsFromImGui(const std::string& selectedSettingsPath)
{
    const bool removed = FileUtility::RemoveFile(selectedSettingsPath); // JSON削除結果
    if (removed) {
        const std::string deletedName = FileUtility::GetStem(selectedSettingsPath); // 削除した設定名
        if (gpuEmitterLoadedSettingsName_ == deletedName) {
            gpuEmitterLoadedSettingsName_.clear();
        }
        gpuEmitterSettingsMessage_ = "Deleted: " + selectedSettingsPath;
    } else {
        gpuEmitterSettingsMessage_ = "Delete failed: " + selectedSettingsPath;
    }
}
/// <summary>
/// ImGuiでGPU Emitterの実行操作を表示する。
/// </summary>
void ParticleManager::DrawGpuEmitterControlImGui()
{
    if (ImGui::Button("Reset GPU Particles")) {
        ResetGpuEmitterParticles();
    }
    ImGui::SameLine();
    if (ImGui::Button("Emit Once")) {
        gpuEmitterManualEmitRequested_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Emit Next Frame")) {
        gpuEmitterState_.frequencyTime = gpuEmitterState_.frequency;
    }
}

/// <summary>
/// ImGuiでGPU Emitter設定を編集する。
/// </summary>
void ParticleManager::DrawGpuEmitterImGui(PostProcess* postProcess)
{
    if (ImGui::CollapsingHeader("GPU Particle", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawGpuEmitterStatusImGui();
        DrawGpuEmitterEffectImGui();
        DrawGpuEmitterPostProcessImGui(postProcess);
        DrawGpuEmitterStateImGui();
        DrawGpuEmitterPresetImGui();
        DrawGpuEmitterSettingsFileImGui();
        DrawGpuEmitterControlImGui();
    }
}
#endif
/// <summary>
/// ImGuiでパーティクル設定を編集する
/// </summary>
void ParticleManager::DrawImGui(PostProcess* postProcess)
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

    DrawGpuEmitterImGui(postProcess);
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



















