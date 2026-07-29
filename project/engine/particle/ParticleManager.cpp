#include "ParticleManager.h"
#include "engine/3d/Camera.h"
#include "engine/3d/Model.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/base/DirectXCommon.h"
#include "engine/utility/Logger.h"
#include "engine/utility/RandomUtility.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

using namespace Math;
using namespace MyEngine;

namespace {
constexpr uint32_t kFallbackParticleLimit = 1024; // 初期化前に参照された場合の最低限の保持上限
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
    if (gpuParticleReady_) {
        FinalizeGpuParticleResources();
    }

    dxCommon_ = dx;
    object3dCommon_ = objCommon;
    srvManager_ = srv;
    texManager_ = texMgr;
    imguiManager_ = imguiManager;
    ClearSceneParticles();
    InitializeGpuParticleResources();
}

/// <summary>
/// シーンが登録したパーティクル状態と描画参照をクリアする。
/// </summary>
void ParticleManager::ClearSceneParticles()
{
    particleGroups_.clear();
    instancingLimitWarnedGroups_.clear();
    totalParticleCount_ = 0;
    ClearGpuEmitterRuntimeParticleState();
    gpuEmitterManualEmitRequested_ = false;
    gpuEmitterState_.emit = 0;
    particlePlane_ = nullptr;
}

/// <summary>
/// パーティクルマネージャーを終了する
/// </summary>
void ParticleManager::Finalize()
{
    if (dxCommon_ && gpuParticleReady_) {
        dxCommon_->WaitForCommandExecution();
    }

    ClearSceneParticles();
    FinalizeGpuParticleResources();

    dxCommon_ = nullptr;
    object3dCommon_ = nullptr;
    srvManager_ = nullptr;
    texManager_ = nullptr;
    imguiManager_ = nullptr;
    particlePlane_ = nullptr;
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

    if (gpuEmitterState_.count == 0) {
        gpuEmitterManualEmitRequested_ = false;
        ClearGpuEmitterRuntimeParticleState();
        return;
    }

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
            const uint32_t particleLimit = GetParticleLimit(); // GPU Particleの上限数
            const uint32_t drawLimit = (std::min)(particleLimit, instancingSlots); // 実際に描画できる最大数
            const uint32_t drawCount = (std::min)(gpuEmitterVisibleCount_, drawLimit); // GPU Emitterから描画する数
            if (drawCount == 0) {
                return;
            }
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
/// GPU EmitterのGPU側パーティクル状態をリセットする。
/// </summary>
void ParticleManager::ResetGpuEmitterParticles()
{
    ClearGpuEmitterRuntimeParticleState();
    gpuEmitterState_.frequencyTime = 0.0f;
    gpuEmitterState_.emit = 0;
}
