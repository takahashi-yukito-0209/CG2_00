#include "ParticleManager.h"
#include "GpuEmitterSettingsUtility.h"
#include "engine/base/PostProcess.h"
#include "engine/utility/FileUtility.h"
#include "engine/utility/JsonUtility.h"
#include "engine/utility/Logger.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>

using namespace Math;
using namespace MyEngine;

namespace {
constexpr float kGpuEmitterLifeMin = 0.1f; // GPU Emitter寿命の最小値
constexpr float kGpuEmitterLifeMax = 100.0f; // GPU Emitter寿命の最大値
constexpr float kGpuEmitterDampingMin = 0.0f; // GPU Emitter減衰率の最小値
constexpr float kGpuEmitterDampingMax = 100.0f; // GPU Emitter減衰率の最大値
} // namespace

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
    file << R"({)" << '\n';
    file << R"(  "version": 2,)" << '\n';

    file << R"(  "effect": {)" << '\n';
    file << R"(    "effectName": ")" << JsonUtility::EscapeString(gpuEmitterEffectName_) << R"(",)" << '\n';
    file << R"(    "description": ")" << JsonUtility::EscapeString(gpuEmitterDescription_) << R"(")" << '\n';
    file << R"(  },)" << '\n';

    file << R"(  "playback": {)" << '\n';
    file << R"(    "autoEmit": )" << (gpuEmitterAutoEmit_ ? 1 : 0) << R"(,)" << '\n';
    file << R"(    "updateParticles": )" << (gpuParticleUpdateEnabled_ ? 1 : 0) << R"(,)" << '\n';
    file << R"(    "drawParticles": )" << (gpuParticleDrawEnabled_ ? 1 : 0) << '\n';
    file << R"(  },)" << '\n';

    file << R"(  "render": {)" << '\n';
    file << R"(    "texture": ")" << JsonUtility::EscapeString(gpuEmitterTexturePath_) << R"(",)" << '\n';
    file << R"(    "usePostProcess": )" << (gpuEmitterUsePostProcess_ ? 1 : 0) << '\n';
    file << R"(  },)" << '\n';

    file << R"(  "emitter": {)" << '\n';
    file << R"(    "spawnShape": )" << gpuEmitterState_.spawnShape << R"(,)" << '\n';
    file << R"(    "translate": [)" << gpuEmitterState_.translate.x << ", " << gpuEmitterState_.translate.y << ", " << gpuEmitterState_.translate.z << R"(],)" << '\n';
    file << R"(    "radius": )" << gpuEmitterState_.radius << R"(,)" << '\n';
    file << R"(    "count": )" << gpuEmitterState_.count << R"(,)" << '\n';
    file << R"(    "frequency": )" << gpuEmitterState_.frequency << R"(,)" << '\n';
    file << R"(    "baseScale": [)" << gpuEmitterState_.baseScale.x << ", " << gpuEmitterState_.baseScale.y << ", " << gpuEmitterState_.baseScale.z << R"(],)" << '\n';
    file << R"(    "randomScale": )" << gpuEmitterState_.randomScale << R"(,)" << '\n';
    file << R"(    "velocityScale": [)" << gpuEmitterState_.velocityScale.x << ", " << gpuEmitterState_.velocityScale.y << ", " << gpuEmitterState_.velocityScale.z << R"(],)" << '\n';
    file << R"(    "lifeTime": )" << gpuEmitterState_.lifeTime << R"(,)" << '\n';
    file << R"(    "colorMin": [)" << gpuEmitterState_.colorMin.x << ", " << gpuEmitterState_.colorMin.y << ", " << gpuEmitterState_.colorMin.z << ", " << gpuEmitterState_.colorMin.w << R"(],)" << '\n';
    file << R"(    "colorMax": [)" << gpuEmitterState_.colorMax.x << ", " << gpuEmitterState_.colorMax.y << ", " << gpuEmitterState_.colorMax.z << ", " << gpuEmitterState_.colorMax.w << R"(],)" << '\n';
    file << R"(    "scaleOverLife": )" << gpuEmitterState_.scaleOverLife << R"(,)" << '\n';
    file << R"(    "endScale": [)" << gpuEmitterState_.endScale.x << ", " << gpuEmitterState_.endScale.y << ", " << gpuEmitterState_.endScale.z << R"(],)" << '\n';
    file << R"(    "gravity": [)" << gpuEmitterState_.gravity.x << ", " << gpuEmitterState_.gravity.y << ", " << gpuEmitterState_.gravity.z << R"(],)" << '\n';
    file << R"(    "damping": )" << gpuEmitterState_.damping << R"(,)" << '\n';
    file << R"(    "colorOverLife": )" << gpuEmitterState_.colorOverLife << R"(,)" << '\n';
    file << R"(    "endColor": [)" << gpuEmitterState_.endColor.x << ", " << gpuEmitterState_.endColor.y << ", " << gpuEmitterState_.endColor.z << ", " << gpuEmitterState_.endColor.w << R"(])" << '\n';
    file << R"(  },)" << '\n';

    file << R"(  "postProcess": {)" << '\n';
    file << R"(    "postProcessEnabled": )" << (gpuEmitterPostProcessEnabled_ ? 1 : 0) << R"(,)" << '\n';
    file << R"(    "postEffectType": )" << gpuEmitterPostEffectType_ << R"(,)" << '\n';
    file << R"(    "postRadialBlurCenter": [)" << postRadialBlurCenter.x << ", " << postRadialBlurCenter.y << ", " << postRadialBlurCenter.z << R"(],)" << '\n';
    file << R"(    "postRadialBlurWidth": )" << gpuEmitterRadialBlurWidth_ << R"(,)" << '\n';
    file << R"(    "postRadialBlurSampleCount": )" << gpuEmitterRadialBlurSampleCount_ << R"(,)" << '\n';
    file << R"(    "postDistortionCenter": [)" << postDistortionCenter.x << ", " << postDistortionCenter.y << ", " << postDistortionCenter.z << R"(],)" << '\n';
    file << R"(    "postDistortionStrength": )" << gpuEmitterDistortionStrength_ << R"(,)" << '\n';
    file << R"(    "postDistortionRadius": )" << gpuEmitterDistortionRadius_ << R"(,)" << '\n';
    file << R"(    "postDistortionWaveCount": )" << gpuEmitterDistortionWaveCount_ << R"(,)" << '\n';
    file << R"(    "postDistortionProgress": )" << gpuEmitterDistortionProgress_ << R"(,)" << '\n';
    file << R"(    "postDissolveThreshold": )" << gpuEmitterDissolveThreshold_ << R"(,)" << '\n';
    file << R"(    "postDissolveEdgeWidth": )" << gpuEmitterDissolveEdgeWidth_ << R"(,)" << '\n';
    file << R"(    "postDissolveEdgeColor": [)" << gpuEmitterDissolveEdgeColor_.x << ", " << gpuEmitterDissolveEdgeColor_.y << ", " << gpuEmitterDissolveEdgeColor_.z << R"(],)" << '\n';
    file << R"(    "postRandomStrength": )" << gpuEmitterRandomStrength_ << R"(,)" << '\n';
    file << R"(    "postRandomScale": )" << gpuEmitterRandomScale_ << R"(,)" << '\n';
    file << R"(    "postRandomSpeed": )" << gpuEmitterRandomSpeed_ << '\n';
    file << R"(  })" << '\n';
    file << R"(})" << '\n';
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
    gpuEmitterState_.lifeTime = std::clamp(gpuEmitterState_.lifeTime, kGpuEmitterLifeMin, kGpuEmitterLifeMax);
    gpuEmitterState_.damping = std::clamp(gpuEmitterState_.damping, kGpuEmitterDampingMin, kGpuEmitterDampingMax);
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
/// GPU Emitter設定を編集用に取得する。
/// </summary>
PM_GpuEmitterSphere* ParticleManager::GetMutableGpuEmitterState()
{
    return &gpuEmitterState_;
}

/// <summary>
/// GPU Emitter設定を参照用に取得する。
/// </summary>
const PM_GpuEmitterSphere* ParticleManager::GetGpuEmitterState() const
{
    return &gpuEmitterState_;
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

