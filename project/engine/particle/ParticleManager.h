#pragma once

#include "engine/2d/TextureManager.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/utility/mathUtility.h"
#include <cstddef>
#include <iosfwd>
#include <array>
#include <d3d12.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <wrl.h>

// CPU側のパーティクルデータ
struct PM_CpuParticle {
    Math::Transform transform;
    Math::Vector3 startScale;
    Math::Vector3 endScale;
    Math::Vector3 velocity;
    Math::Vector4 color;
    Math::Vector4 startColor;
    float lifeTime = 5.0f;
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
// GPUでインスタンシング用行列へ変換するための粒子データ
struct PM_GpuParticleSource {
    Math::Vector3 scale;
    float lifeTime = 0.0f;
    Math::Vector3 rotate;
    float currentTime = 0.0f;
    Math::Vector3 translate;
    float padding0 = 0.0f;
    Math::Vector3 velocity;
    float padding1 = 0.0f;
    Math::Vector4 color;
    Math::Vector3 startScale;
    float padding2 = 0.0f;
    Math::Vector4 startColor;
};

// ComputeShaderへ渡すパーティクル変換情報
struct PM_GpuParticleTransformInfo {
    uint32_t particleCount = 0;
    float padding[3] {};
    Math::Matrix4x4 view;
    Math::Matrix4x4 projection;
};

struct PM_GpuEmitterSphere {
    Math::Vector3 translate { 0.0f, 0.0f, 0.0f }; // 発生中心座標
    float radius = 1.3f; // 球状に散らす半径
    uint32_t count = 1024; // 1回の射出で発生させる数
    float frequency = 0.12f; // 射出間隔
    float frequencyTime = 0.0f; // 射出間隔の経過時間
    uint32_t emit = 0; // 射出許可フラグ
    Math::Vector3 baseScale { 0.08f, 0.08f, 0.08f }; // 最小スケール
    float randomScale = 0.03f; // ランダムに加算するスケール幅
    Math::Vector3 velocityScale { 0.08f, 0.10f, 0.08f }; // 速度倍率
    float lifeTime = 5.0f; // 寿命
    Math::Vector4 colorMin { 0.35f, 0.55f, 0.9f, 1.0f }; // ランダム色の最小値
    Math::Vector4 colorMax { 1.0f, 1.0f, 1.0f, 1.0f }; // ランダム色の最大値
    uint32_t spawnShape = 0; // 発生形状 0:球 1:箱 2:リング 3:コーン
    float padding[3] {}; // ConstantBufferの16byte境界調整
    Math::Vector3 endScale { 0.02f, 0.02f, 0.02f }; // 寿命終了時のスケール
    float damping = 0.0f; // 速度の減衰量
    Math::Vector3 gravity { 0.0f, -0.05f, 0.0f }; // GPU更新で加える重力
    uint32_t scaleOverLife = 0; // 寿命に応じてスケールを変えるか
    Math::Vector4 endColor { 1.0f, 1.0f, 1.0f, 0.0f }; // 寿命終了時の色
    uint32_t colorOverLife = 0; // 寿命に応じて色を変えるか
    float padding2[3] {}; // ConstantBufferの16byte境界調整
};

struct PM_GpuPerFrame {
    float time = 0.0f;
    float deltaTime = 0.0f;
    uint32_t scaleOverLife = 0;
    uint32_t colorOverLife = 0;
    Math::Vector3 gravity { 0.0f, 0.0f, 0.0f };
    float damping = 0.0f;
    Math::Vector3 endScale { 0.0f, 0.0f, 0.0f };
    float padding0 = 0.0f;
    Math::Vector4 endColor { 0.0f, 0.0f, 0.0f, 0.0f };
};
namespace MyEngine {
class PostProcess;

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
    /// シーンが登録したパーティクル状態と描画参照をクリアする。
    /// </summary>
    void ClearSceneParticles();

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
    /// GPU Emitterプリセットを名前から読み込む。
    /// </summary>
    bool LoadGpuEmitterPreset(const std::string& presetName);

    /// <summary>
    /// 現在のGPU Emitter設定で次フレームに1回だけ発生させる。
    /// </summary>
    void RequestGpuEmitterEmit();

    /// <summary>
    /// GPU Emitter設定を編集用に取得する。
    /// </summary>
    PM_GpuEmitterSphere* GetMutableGpuEmitterState();

    /// <summary>
    /// GPU Emitter設定を参照用に取得する。
    /// </summary>
    const PM_GpuEmitterSphere* GetGpuEmitterState() const;

    /// <summary>
    /// GPU Emitterプリセットを読み込み、設定内の位置で1回だけ発生させる。
    /// </summary>
    bool PlayGpuEmitterPreset(const std::string& presetName);

    /// <summary>
    /// GPU Emitterプリセットを読み込み、指定位置で1回だけ発生させる。
    /// </summary>
    bool PlayGpuEmitterPreset(const std::string& presetName, const Math::Vector3& position);

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
    void DrawImGui(PostProcess* postProcess = nullptr);


private:
    ParticleManager() = default;

    /// <summary>
    /// 現在のPostProcess設定をGPU Emitter設定へ取り込む。
    /// </summary>
    void CaptureGpuEmitterPostProcessSettings(const PostProcess& postProcess);

    /// <summary>
    /// GPU Emitterに保存しているPostProcess設定を反映する。
    /// </summary>
    void ApplyGpuEmitterPostProcessSettings(PostProcess& postProcess) const;

    /// <summary>
    /// GPU EmitterのGPU側パーティクル状態をリセットする。
    /// </summary>
    void ResetGpuEmitterParticles();

    /// <summary>
    /// GPU Emitterの実行時パーティクル状態をクリアする。
    /// </summary>
    void ClearGpuEmitterRuntimeParticleState();

    /// <summary>
    /// GPU Emitter用テクスチャを描画グループへ反映する。
    /// </summary>
    void ApplyGpuEmitterTextureToDrawGroup();

    /// <summary>
    /// GPU Emitterの経過時間と射出許可を更新する。
    /// </summary>
    void UpdateGpuEmitter(float dt);

    /// <summary>
    /// ImGuiでGPU Emitter設定を編集する。
    /// </summary>
    void DrawGpuEmitterImGui(PostProcess* postProcess);

    /// <summary>
    /// ImGuiでGPU Emitterの基本情報を表示する。
    /// </summary>
    void DrawGpuEmitterStatusImGui();

    /// <summary>
    /// ImGuiでGPU Emitterのeffect情報を編集する。
    /// </summary>
    void DrawGpuEmitterEffectImGui();

    /// <summary>
    /// ImGuiでGPU Emitterに紐づくPostProcess設定を編集する。
    /// </summary>
    void DrawGpuEmitterPostProcessImGui(PostProcess* postProcess);

    /// <summary>
    /// ImGuiでGPU Emitterの発生設定を編集する。
    /// </summary>
    void DrawGpuEmitterStateImGui();

    /// <summary>
    /// ImGuiでGPU Emitterの再生フラグを編集する。
    /// </summary>
    void DrawGpuEmitterPlaybackStateImGui();

    /// <summary>
    /// ImGuiでGPU Emitterの発生範囲と発生数を編集する。
    /// </summary>
    void DrawGpuEmitterSpawnStateImGui();

    /// <summary>
    /// ImGuiでGPU Emitterのスケールと寿命を編集する。
    /// </summary>
    void DrawGpuEmitterScaleLifeStateImGui();

    /// <summary>
    /// ImGuiでGPU Emitterの物理挙動を編集する。
    /// </summary>
    void DrawGpuEmitterPhysicsStateImGui();

    /// <summary>
    /// ImGuiでGPU Emitterの色変化を編集する。
    /// </summary>
    void DrawGpuEmitterColorStateImGui();

    /// <summary>
    /// ImGuiでGPU Emitter設定ファイルの保存と読み込みを操作する。
    /// </summary>
    void DrawGpuEmitterSettingsFileImGui();

    /// <summary>
    /// ImGuiでGPU Emitter設定名を編集する。
    /// </summary>
    void DrawGpuEmitterSettingsNameImGui();

    /// <summary>
    /// ImGuiでGPU Emitter設定ファイルの選択欄を表示する。
    /// </summary>
    void DrawGpuEmitterSettingsFileComboImGui(const std::vector<std::string>& settingsFiles, const std::string& settingsPreview);

    /// <summary>
    /// ImGuiでGPU Emitter設定ファイルの操作ボタンを表示する。
    /// </summary>
    void DrawGpuEmitterSettingsFileButtonsImGui(const std::string& saveSettingsPath, const std::string& selectedSettingsPath);

    /// <summary>
    /// GPU Emitter設定を指定パスへ保存して結果メッセージを更新する。
    /// </summary>
    void SaveGpuEmitterSettingsFromImGui(const std::string& saveSettingsPath);

    /// <summary>
    /// GPU Emitter設定を指定パスから読み込んで結果メッセージを更新する。
    /// </summary>
    void LoadGpuEmitterSettingsFromImGui(const std::string& loadSettingsPath);

    /// <summary>
    /// GPU Emitter設定ファイルを削除して結果メッセージを更新する。
    /// </summary>
    void DeleteGpuEmitterSettingsFromImGui(const std::string& selectedSettingsPath);

    /// <summary>
    /// ImGuiでGPU Emitterの実行操作を表示する。
    /// </summary>
    void DrawGpuEmitterControlImGui();
    /// <summary>
    /// GPUパーティクル変換に必要なリソースを作成する。
    /// </summary>
    void InitializeGpuParticleResources();

    /// <summary>
    /// GPUパーティクル変換に必要なリソースを解放する。
    /// </summary>
    void FinalizeGpuParticleResources();

    /// <summary>
    /// GPU参照が終わるまでD3D12リソースの解放を遅延する。
    /// </summary>
    void DeferReleaseResource(Microsoft::WRL::ComPtr<ID3D12Resource>& resource);

    /// <summary>
    /// 指定フレームのGPU Particle用リソースを解放予約する。
    /// </summary>
    void ReleaseGpuParticleFrameResources(uint32_t frameIndex);

    /// <summary>
    /// GPUへ渡すパーティクル入力データを現在のグループ内容から作成する。
    /// </summary>
    uint32_t UploadGpuParticleSource(const ParticleGroup& group, uint32_t count, const Math::Matrix4x4& view, const Math::Matrix4x4& projection);

    /// <summary>
    /// ComputeShaderでパーティクルをインスタンシング用行列へ変換する。
    /// </summary>
    bool DispatchGpuParticleTransform(uint32_t count);

    /// <summary>
    /// GPU上のParticle Resourceを初期化する。
    /// </summary>
    bool DispatchInitializeGpuParticles();

    /// <summary>
    /// GPU上でEmitterからParticleを発生させる。
    /// </summary>
    bool DispatchEmitGpuParticles();

    /// <summary>
    /// GPU上のParticleを経過時間で更新する。
    /// </summary>
    bool DispatchUpdateGpuParticles();

    /// <summary>
    /// GPU FreeListIndexをReadback用Resourceへコピーする。
    /// </summary>
    void CopyGpuFreeCounterToReadback(uint32_t frameIndex);

    /// <summary>
    /// Readback済みのFreeListIndexからGPU Particleの生存数推定値を更新する。
    /// </summary>
    void UpdateGpuAliveCountEstimate();

    /// <summary>
    /// GPU Emitter設定をJSONファイルへ保存する。
    /// </summary>
    bool SaveGpuEmitterSettings(const std::string& filePath) const;

    /// <summary>
    /// GPU Emitter設定をJSONファイルから読み込む。
    /// </summary>
    bool LoadGpuEmitterSettings(const std::string& filePath);
    /// <summary>
    /// GPU Emitter設定をJSON形式で書き出す。
    /// </summary>
    void WriteGpuEmitterSettingsJson(std::ostream& file) const;

    /// <summary>
    /// JSONのeffect/renderカテゴリからGPU Emitterの基本情報を読み込む。
    /// </summary>
    void LoadGpuEmitterEffectSettings(const std::string& effectSection, const std::string& renderSection);

    /// <summary>
    /// JSONのplayback/renderカテゴリからGPU Emitterの再生設定を読み込む。
    /// </summary>
    void LoadGpuEmitterPlaybackSettings(const std::string& playbackSection, const std::string& renderSection);

    /// <summary>
    /// JSONのpostProcessカテゴリからGPU EmitterのPostProcess設定を読み込む。
    /// </summary>
    void LoadGpuEmitterPostProcessSettings(const std::string& postProcessSection);

    /// <summary>
    /// JSONのemitterカテゴリからGPU Emitterの発生設定を読み込む。
    /// </summary>
    void LoadGpuEmitterStateSettings(const std::string& emitterSection);

    /// <summary>
    /// GPU Emitter設定を実行時に扱える範囲へ整える。
    /// </summary>
    void NormalizeGpuEmitterStateForRuntime();

    /// <summary>
    /// 読み込み後のGPU Emitter設定を実行時に扱える範囲へ整える。
    /// </summary>
    void NormalizeGpuEmitterStateAfterLoad();

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

    Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> initializeParticlePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> emitParticlePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> updateParticlePipelineState_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> gpuParticleSourceResources_;
    std::array<PM_GpuParticleSource*, DirectXCommon::kFrameCount> gpuParticleSourceData_ {};
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> gpuParticleInfoResources_;
    std::array<PM_GpuParticleTransformInfo*, DirectXCommon::kFrameCount> gpuParticleInfoData_ {};
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> gpuEmitterResources_;
    std::array<PM_GpuEmitterSphere*, DirectXCommon::kFrameCount> gpuEmitterData_ {};
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> gpuPerFrameResources_;
    std::array<PM_GpuPerFrame*, DirectXCommon::kFrameCount> gpuPerFrameData_ {};
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> gpuFreeCounterResources_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> gpuFreeCounterReadbackResources_;
    std::array<int32_t*, DirectXCommon::kFrameCount> gpuFreeCounterReadbackData_ {};
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> gpuFreeListResources_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> gpuParticleOutputResources_;
    std::array<uint32_t, DirectXCommon::kFrameCount> gpuParticleSourceSrvIndices_ { UINT32_MAX, UINT32_MAX };
    std::array<uint32_t, DirectXCommon::kFrameCount> gpuParticleOutputSrvIndices_ { UINT32_MAX, UINT32_MAX };
    std::array<uint32_t, DirectXCommon::kFrameCount> gpuParticleOutputUavIndices_ { UINT32_MAX, UINT32_MAX };
    std::array<uint32_t, DirectXCommon::kFrameCount> gpuFreeCounterUavIndices_ { UINT32_MAX, UINT32_MAX };
    std::array<uint32_t, DirectXCommon::kFrameCount> gpuFreeListUavIndices_ { UINT32_MAX, UINT32_MAX };
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, DirectXCommon::kFrameCount> gpuParticleOutputSrvHandlesGPU_ {};
    std::array<D3D12_RESOURCE_STATES, DirectXCommon::kFrameCount> gpuParticleOutputStates_ {};
    std::array<D3D12_RESOURCE_STATES, DirectXCommon::kFrameCount> gpuFreeCounterStates_ {};
    std::array<D3D12_RESOURCE_STATES, DirectXCommon::kFrameCount> gpuFreeListStates_ {};
    std::array<bool, DirectXCommon::kFrameCount> gpuParticleInitialized_ {};
    PM_GpuEmitterSphere gpuEmitterState_ {};
    PM_GpuPerFrame gpuPerFrameState_ {};
    uint32_t gpuEmitterVisibleCount_ = 0;
    uint32_t gpuAliveCountEstimate_ = 0; // GPU FreeListIndexから推定した生存Particle数
    std::string gpuEmitterSettingsName_ = "gpu_particle"; // JSON保存時の設定名
    std::string gpuEmitterLoadedSettingsName_; // 現在ロード済みの設定名
    std::string gpuEmitterEffectName_ = "GPU Particle"; // エフェクト表示名
    std::string gpuEmitterDescription_; // エフェクト説明文
    std::string gpuEmitterTexturePath_ = "resources/textures/circle.png"; // GPU Particle描画用テクスチャ
    bool gpuEmitterUsePostProcess_ = false; // JSON保存対象のPostProcessを使用するか
    bool gpuEmitterPostProcessEnabled_ = true; // 保存済みPostProcessの有効状態
    uint32_t gpuEmitterPostEffectType_ = 1; // 保存済みPostProcessの種類
    Math::Vector2 gpuEmitterRadialBlurCenter_ { 0.5f, 0.5f }; // 保存済みRadialBlur中心
    float gpuEmitterRadialBlurWidth_ = 0.01f; // 保存済みRadialBlur幅
    uint32_t gpuEmitterRadialBlurSampleCount_ = 10; // 保存済みRadialBlurサンプル数
    Math::Vector2 gpuEmitterDistortionCenter_ { 0.5f, 0.5f }; // 保存済みDistortion中心
    float gpuEmitterDistortionStrength_ = 0.02f; // 保存済みDistortion強度
    float gpuEmitterDistortionRadius_ = 0.35f; // 保存済みDistortion半径
    float gpuEmitterDistortionWaveCount_ = 3.0f; // 保存済みDistortion波数
    float gpuEmitterDistortionProgress_ = 0.0f; // 保存済みDistortion進行率
    float gpuEmitterDissolveThreshold_ = 0.0f; // 保存済みDissolve閾値
    float gpuEmitterDissolveEdgeWidth_ = 0.03f; // 保存済みDissolve境界幅
    Math::Vector3 gpuEmitterDissolveEdgeColor_ { 1.0f, 0.4f, 0.3f }; // 保存済みDissolve境界色
    float gpuEmitterRandomStrength_ = 1.0f; // 保存済みRandom強度
    float gpuEmitterRandomScale_ = 600.0f; // 保存済みRandomスケール
    float gpuEmitterRandomSpeed_ = 1.0f; // 保存済みRandom速度
    std::string gpuEmitterSettingsMessage_; // JSON保存と読み込みの結果表示
    bool gpuEmitterAutoEmit_ = true; // GPU Particleを自動発生させるか
    bool gpuEmitterManualEmitRequested_ = false; // 次の更新で1回だけ発生させるか
    bool gpuParticleUpdateEnabled_ = true; // GPU Particleの寿命と移動を更新するか
    bool gpuParticleDrawEnabled_ = true; // GPU Particleを描画するか
    bool gpuParticleReady_ = false;
};

} // namespace MyEngine