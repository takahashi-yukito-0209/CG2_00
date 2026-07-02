#pragma once

#include <MathTypes.h>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

namespace MyEngine {

class DirectXCommon;
class RenderTarget;

/// <summary>
/// ポストエフェクトの種類
/// </summary>
enum class PostEffectType {
    Distortion, // 指定した中心から画面を円形に歪ませる
    Copy, // 元画像をそのまま描画する
    Grayscale, // 元画像をグレースケール化して描画する
    Vignette, // 画面周辺を暗くして描画する
    BoxFilter, // Box Filterによる平滑化処理を適用する
    GaussianFilter, // 分離型Gaussian Filterを適用する
    LuminanceOutline, // 輝度差から輪郭を検出する
    DepthOutline, // 深度差から輪郭を検出する
    RadialBlur, // 指定した中心から放射状にぼかす
    Dissolve, // ノイズマスクの閾値で画面を消去する
    Random, // GPUで生成した乱数を入力画像へ乗算する
};

/// <summary>
/// 全画面ポストエフェクトを管理するクラス
/// </summary>
class PostProcess {
public:
    /// <summary>
    /// デフォルトコンストラクタ
    /// </summary>
    PostProcess() = default;

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~PostProcess();

    /// <summary>
    /// ポストエフェクトに必要なリソースを初期化する
    /// </summary>
    bool Initialize(DirectXCommon* dxCommon);

    /// <summary>
    /// ポストエフェクトが保持するリソースを解放する
    /// </summary>
    void Finalize();

    /// <summary>
    /// 指定したポストエフェクトでテクスチャを描画する
    /// </summary>
    void DrawTexture(uint32_t srvIndex, PostEffectType effectType);

    /// <summary>
    /// RenderTargetのカラーSRVを指定したポストエフェクトで描画する
    /// </summary>
    void DrawTexture(const RenderTarget& sourceRenderTarget, PostEffectType effectType);

    /// <summary>
    /// 現在選択されているポストエフェクトでテクスチャを描画する
    /// </summary>
    void DrawTexture(uint32_t srvIndex);

    /// <summary>
    /// RenderTargetのカラーSRVを現在のポストエフェクトで描画する
    /// </summary>
    void DrawTexture(const RenderTarget& sourceRenderTarget);

    /// <summary>
    /// 描画に必要なリソースが揃っているか確認する
    /// </summary>
    bool IsReady() const;

    /// <summary>
    /// ポストエフェクトの有効状態を設定する
    /// </summary>
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    /// <summary>
    /// ポストエフェクトが有効か確認する
    /// </summary>
    bool IsEnabled() const { return enabled_; }

    /// <summary>
    /// 使用するポストエフェクトを設定する
    /// </summary>
    void SetEffectType(PostEffectType effectType) { effectType_ = effectType; }

    /// <summary>
    /// 現在選択されているポストエフェクトを取得する
    /// </summary>
    PostEffectType GetEffectType() const { return effectType_; }

    /// <summary>
    /// 通常コピー用PSOが生成済みか確認する
    /// </summary>
    bool IsCopyReady() const { return copyPipelineState_ != nullptr; }

    /// <summary>
    /// グレースケール用PSOが生成済みか確認する
    /// </summary>
    bool IsGrayscaleReady() const { return grayscalePipelineState_ != nullptr; }

    /// <summary>
    /// ビネット用PSOが生成済みか確認する
    /// </summary>
    bool IsVignetteReady() const { return vignettePipelineState_ != nullptr; }

    /// <summary>
    /// Box Filter用PSOが生成済みか確認する
    /// </summary>
    bool IsBoxFilterReady() const { return boxFilterPipelineState_ != nullptr; }

    /// <summary>
    /// Box Filterで使用するカーネルサイズを設定する
    /// </summary>
    void SetBoxFilterKernelSize(uint32_t kernelSize);

    /// <summary>
    /// Box Filterで使用するカーネルサイズを取得する
    /// </summary>
    uint32_t GetBoxFilterKernelSize() const { return boxFilterKernelSize_; }

    /// <summary>
    /// Gaussian Filter用PSOが生成済みか確認する
    /// </summary>
    bool IsGaussianFilterReady() const { return gaussianFilterPipelineState_ != nullptr; }

    /// <summary>
    /// Gaussian Filterで使用するカーネルサイズを設定する
    /// </summary>
    void SetGaussianKernelSize(uint32_t kernelSize);

    /// <summary>
    /// Gaussian Filterで使用するカーネルサイズを取得する
    /// </summary>
    uint32_t GetGaussianKernelSize() const { return gaussianKernelSize_; }

    /// <summary>
    /// Gaussian Filterで使用する標準偏差を設定する
    /// </summary>
    void SetGaussianSigma(float sigma);

    /// <summary>
    /// Gaussian Filterで使用する標準偏差を取得する
    /// </summary>
    float GetGaussianSigma() const { return gaussianSigma_; }

    /// <summary>
    /// Gaussian Filterの1方向分を現在の描画先へ描画する
    /// </summary>
    void DrawGaussianPass(uint32_t srvIndex, uint32_t direction);

    /// <summary>
    /// RenderTargetのカラーSRVからGaussian Filterの1方向分を描画する
    /// </summary>
    void DrawGaussianPass(const RenderTarget& sourceRenderTarget, uint32_t direction);

    /// <summary>
    /// 中間レンダーターゲットを使用して分離型Gaussian Filterを描画する
    /// </summary>
    void DrawGaussianTexture(
        uint32_t sourceSrvIndex,
        int intermediateRenderTargetHandle,
        uint32_t intermediateSrvIndex);

    /// <summary>
    /// RenderTargetを中間描画先として分離型Gaussian Filterを描画する
    /// </summary>
    void DrawGaussianTexture(uint32_t sourceSrvIndex, RenderTarget& intermediateRenderTarget);

    /// <summary>
    /// Outlineの強度を設定する
    /// </summary>
    void SetOutlineStrength(float strength);

    /// <summary>
    /// Outlineの強度を取得する
    /// </summary>
    float GetOutlineStrength() const { return outlineStrength_; }

    /// <summary>
    /// Depth Outlineで輪郭と判定する深度差の閾値を設定する
    /// </summary>
    void SetDepthOutlineThreshold(float threshold);

    /// <summary>
    /// Depth Outlineの深度差閾値を取得する
    /// </summary>
    float GetDepthOutlineThreshold() const { return depthOutlineThreshold_; }

    /// <summary>
    /// Depth Outlineの輪郭の立ち上がり幅を設定する
    /// </summary>
    void SetDepthOutlineSoftness(float softness);

    /// <summary>
    /// Depth Outlineの輪郭の立ち上がり幅を取得する
    /// </summary>
    float GetDepthOutlineSoftness() const { return depthOutlineSoftness_; }

    /// <summary>
    /// 輝度ベースOutline用PSOが生成済みか確認する
    /// </summary>
    bool IsLuminanceOutlineReady() const { return luminanceOutlinePipelineState_ != nullptr; }

    /// <summary>
    /// 深度ベースOutline用PSOが生成済みか確認する
    /// </summary>
    bool IsDepthOutlineReady() const { return depthOutlinePipelineState_ != nullptr; }

    /// <summary>
    /// 深度テクスチャを使用してOutlineを描画する
    /// </summary>
    void DrawDepthOutline(
        uint32_t colorSrvIndex,
        uint32_t depthSrvIndex,
        const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// RenderTargetの深度SRVを使用してOutlineを描画する
    /// </summary>
    void DrawDepthOutline(
        uint32_t colorSrvIndex,
        const RenderTarget& depthSourceRenderTarget,
        const Math::Matrix4x4& projectionMatrix);

    /// <summary>
    /// Radial Blur用PSOが生成済みか確認する
    /// </summary>
    bool IsRadialBlurReady() const { return radialBlurPipelineState_ != nullptr; }

    /// <summary>
    /// Radial Blurの中心UV座標を設定する
    /// </summary>
    void SetRadialBlurCenter(const Math::Vector2& center);

    /// <summary>
    /// Radial Blurの中心UV座標を取得する
    /// </summary>
    const Math::Vector2& GetRadialBlurCenter() const { return radialBlurCenter_; }

    /// <summary>
    /// Radial Blurの幅を設定する
    /// </summary>
    void SetRadialBlurWidth(float width);

    /// <summary>
    /// Radial Blurの幅を取得する
    /// </summary>
    float GetRadialBlurWidth() const { return radialBlurWidth_; }

    /// <summary>
    /// Radial Blurのサンプル数を設定する
    /// </summary>
    void SetRadialBlurSampleCount(uint32_t sampleCount);

    /// <summary>
    /// Radial Blurのサンプル数を取得する
    /// </summary>
    uint32_t GetRadialBlurSampleCount() const { return radialBlurSampleCount_; }

    /// <summary>
    /// 画面歪み用PSOが生成済みか確認する
    /// </summary>
    bool IsDistortionReady() const { return distortionPipelineState_ != nullptr; }

    /// <summary>
    /// 画面歪みの中心UV座標を設定する
    /// </summary>
    void SetDistortionCenter(const Math::Vector2& center);

    /// <summary>
    /// 画面歪みの中心UV座標を取得する
    /// </summary>
    const Math::Vector2& GetDistortionCenter() const { return distortionCenter_; }

    /// <summary>
    /// 画面歪みの強度を設定する
    /// </summary>
    void SetDistortionStrength(float strength);

    /// <summary>
    /// 画面歪みの強度を取得する
    /// </summary>
    float GetDistortionStrength() const { return distortionStrength_; }

    /// <summary>
    /// 画面歪みの半径を設定する
    /// </summary>
    void SetDistortionRadius(float radius);

    /// <summary>
    /// 画面歪みの半径を取得する
    /// </summary>
    float GetDistortionRadius() const { return distortionRadius_; }

    /// <summary>
    /// 画面歪みの波数を設定する
    /// </summary>
    void SetDistortionWaveCount(float waveCount);

    /// <summary>
    /// 画面歪みの波数を取得する
    /// </summary>
    float GetDistortionWaveCount() const { return distortionWaveCount_; }

    /// <summary>
    /// 画面歪みの進行率を設定する
    /// </summary>
    void SetDistortionProgress(float progress);

    /// <summary>
    /// 画面歪みの進行率を取得する
    /// </summary>
    float GetDistortionProgress() const { return distortionProgress_; }

    /// <summary>
    /// Dissolve用PSOが生成済みか確認する
    /// </summary>
    bool IsDissolveReady() const { return dissolvePipelineState_ != nullptr; }

    /// <summary>
    /// Dissolveの閾値を設定する
    /// </summary>
    void SetDissolveThreshold(float threshold);

    /// <summary>
    /// Dissolveの閾値を取得する
    /// </summary>
    float GetDissolveThreshold() const { return dissolveThreshold_; }

    /// <summary>
    /// Dissolve境界の幅を設定する
    /// </summary>
    void SetDissolveEdgeWidth(float edgeWidth);

    /// <summary>
    /// Dissolve境界の幅を取得する
    /// </summary>
    float GetDissolveEdgeWidth() const { return dissolveEdgeWidth_; }

    /// <summary>
    /// Dissolve境界の色を設定する
    /// </summary>
    void SetDissolveEdgeColor(const Math::Vector3& color);

    /// <summary>
    /// Dissolve境界の色を取得する
    /// </summary>
    const Math::Vector3& GetDissolveEdgeColor() const { return dissolveEdgeColor_; }

    /// <summary>
    /// ノイズマスクを使用してDissolveを描画する
    /// </summary>
    void DrawDissolveTexture(
        uint32_t sourceSrvIndex,
        uint32_t maskSrvIndex);

    /// <summary>
    /// RenderTargetのカラーSRVとノイズマスクを使用してDissolveを描画する
    /// </summary>
    void DrawDissolveTexture(
        const RenderTarget& sourceRenderTarget,
        uint32_t maskSrvIndex);

    /// <summary>
    /// 時間経過を使用するポストエフェクトを更新する
    /// </summary>
    void Update(float deltaTime);

    /// <summary>
    /// Random用PSOが生成済みか確認する
    /// </summary>
    bool IsRandomReady() const { return randomPipelineState_ != nullptr; }

    /// <summary>
    /// Randomのノイズ強度を設定する
    /// </summary>
    void SetRandomStrength(float strength);

    /// <summary>
    /// Randomのノイズ強度を取得する
    /// </summary>
    float GetRandomStrength() const { return randomStrength_; }

    /// <summary>
    /// Randomのノイズスケールを設定する
    /// </summary>
    void SetRandomScale(float scale);

    /// <summary>
    /// Randomのノイズスケールを取得する
    /// </summary>
    float GetRandomScale() const { return randomScale_; }

    /// <summary>
    /// Randomの時間変化速度を設定する
    /// </summary>
    void SetRandomSpeed(float speed);

    /// <summary>
    /// Randomの時間変化速度を取得する
    /// </summary>
    float GetRandomSpeed() const { return randomSpeed_; }

    /// <summary>
    /// 最後に描画へ使用したSRVインデックスを取得する
    /// </summary>
    uint32_t GetLastSrvIndex() const { return lastSrvIndex_; }

private:
    /// <summary>
    /// 全画面描画用のルートシグネチャを生成する
    /// </summary>
    void CreateRootSignature();

    /// <summary>
    /// 指定したピクセルシェーダーからPSOを生成する
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePipelineState(
        const wchar_t* pixelShaderPath);

    /// <summary>
    /// 指定したポストエフェクトに対応するPSOを取得する
    /// </summary>
    ID3D12PipelineState* GetPipelineState(PostEffectType effectType) const;

private:
    DirectXCommon* dxCommon_ = nullptr; // DirectX共通基盤
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_; // 全画面描画用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> copyPipelineState_; // 通常コピー用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> grayscalePipelineState_; // グレースケール用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> vignettePipelineState_; // ビネット用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> boxFilterPipelineState_; // Box Filter用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gaussianFilterPipelineState_; // Gaussian Filter用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> luminanceOutlinePipelineState_; // 輝度Outline用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> depthOutlinePipelineState_; // 深度Outline用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> radialBlurPipelineState_; // Radial Blur用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> dissolvePipelineState_; // Dissolve用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> randomPipelineState_; // Random用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> distortionPipelineState_; // 画面歪み用PSO
    PostEffectType effectType_ = PostEffectType::Grayscale; // 現在選択中のエフェクト
    bool enabled_ = true; // ポストエフェクトの有効状態
    uint32_t lastSrvIndex_ = UINT32_MAX; // 最後に描画へ使用したSRVインデックス
    uint32_t boxFilterKernelSize_ = 3; // Box Filterに使用するカーネルサイズ
    uint32_t gaussianKernelSize_ = 7; // Gaussian Filterに使用するカーネルサイズ
    float gaussianSigma_ = 2.0f; // Gaussian Filterの標準偏差
    float outlineStrength_ = 6.0f; // Outlineの輪郭強度
    float depthOutlineThreshold_ = 0.02f; // 輪郭として扱う相対深度差
    float depthOutlineSoftness_ = 0.03f; // 輪郭の滑らかな立ち上がり幅
    Math::Vector2 radialBlurCenter_ = { 0.5f, 0.5f }; // 放射状ブラーの中心
    float radialBlurWidth_ = 0.01f; // 1サンプルごとのUV移動量
    uint32_t radialBlurSampleCount_ = 10; // 放射状ブラーのサンプル数
    float dissolveThreshold_ = 0.0f; // マスクを破棄する閾値
    float dissolveEdgeWidth_ = 0.03f; // Dissolve境界の幅
    Math::Vector3 dissolveEdgeColor_ = { 1.0f, 0.4f, 0.3f }; // Dissolve境界の色
    float randomTime_ = 0.0f; // RandomのSeedに使用する経過時間
    float randomStrength_ = 1.0f; // 入力画像へ乗算するノイズ強度
    float randomScale_ = 600.0f; // UVに掛けるノイズの細かさ
    float randomSpeed_ = 1.0f; // ノイズの時間変化速度
    Math::Vector2 distortionCenter_ = { 0.5f, 0.5f }; // 画面歪みの中心UV座標
    float distortionStrength_ = 0.02f; // 画面歪みの強度
    float distortionRadius_ = 0.35f; // 画面歪みの影響半径
    float distortionWaveCount_ = 3.0f; // 画面歪みの波数
    float distortionProgress_ = 0.0f; // 画面歪みの進行率
};

} // namespace MyEngine