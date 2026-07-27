#include "PostProcess.h"
#include "PostProcessPipeline.h"

#include "RenderTarget.h"

#include "../utility/mathUtility.h"
#include "DirectXCommon.h"

#include <cstring>

using namespace MyEngine;

namespace {
/// <summary>
/// float値を32bitルート定数へ渡すビット表現へ変換する。
/// </summary>
uint32_t ConvertFloatToRootConstant(float value)
{
    uint32_t bits = 0; // float値のビット表現
    std::memcpy(&bits, &value, sizeof(float));
    return bits;
}

/// <summary>
/// float値を指定した位置のルート定数へ書き込む。
/// </summary>
void WriteFloatRootConstant(uint32_t* constants, uint32_t index, float value)
{
    constants[index] = ConvertFloatToRootConstant(value);
}

} // namespace

/// <summary>
/// ポストエフェクト種類の表示名を取得する。
/// </summary>
const char* MyEngine::GetPostEffectTypeName(PostEffectType effectType)
{
    switch (effectType) {
    case PostEffectType::Distortion:
        return "Distortion";
    case PostEffectType::Copy:
        return "Copy";
    case PostEffectType::Grayscale:
        return "Grayscale";
    case PostEffectType::Vignette:
        return "Vignette";
    case PostEffectType::BoxFilter:
        return "Box Filter";
    case PostEffectType::GaussianFilter:
        return "Gaussian Filter";
    case PostEffectType::LuminanceOutline:
        return "Luminance Outline";
    case PostEffectType::DepthOutline:
        return "Depth Outline";
    case PostEffectType::RadialBlur:
        return "Radial Blur";
    case PostEffectType::Dissolve:
        return "Dissolve";
    case PostEffectType::Random:
        return "Random";
    case PostEffectType::Count:
    default:
        return "Unknown";
    }
}

/// <summary>
/// デフォルトコンストラクタ。
/// </summary>
PostProcess::PostProcess() = default;
/// <summary>
/// デストラクタ
/// </summary>
PostProcess::~PostProcess()
{
    Finalize();
}

/// <summary>
/// ポストエフェクトに必要なリソースを初期化する
/// </summary>
bool PostProcess::Initialize(DirectXCommon* dxCommon)
{
    if (!dxCommon) {
        return false;
    }

    Finalize();
    dxCommon_ = dxCommon; // 描画時に使用するDirectX共通基盤
    pipeline_ = std::make_unique<PostProcessPipeline>(); // RootSignatureとPSOの管理先

    return pipeline_->Initialize(dxCommon_);
}

/// <summary>
/// ポストエフェクトが保持するリソースを解放する
/// </summary>
void PostProcess::Finalize()
{
    if (pipeline_) {
        pipeline_->Finalize();
        pipeline_.reset();
    }
    dxCommon_ = nullptr;
    lastSrvIndex_ = UINT32_MAX;
}

/// <summary>
/// 描画に必要なリソースが揃っているか確認する
/// </summary>
bool PostProcess::IsReady() const
{
    return dxCommon_ && pipeline_ && pipeline_->IsReady();
}

/// <summary>
/// 通常コピー用PSOが生成済みか確認する。
/// </summary>
bool PostProcess::IsCopyReady() const
{
    return pipeline_ && pipeline_->IsEffectReady(PostEffectType::Copy);
}

/// <summary>
/// グレースケール用PSOが生成済みか確認する。
/// </summary>
bool PostProcess::IsGrayscaleReady() const
{
    return pipeline_ && pipeline_->IsEffectReady(PostEffectType::Grayscale);
}

/// <summary>
/// ビネット用PSOが生成済みか確認する。
/// </summary>
bool PostProcess::IsVignetteReady() const
{
    return pipeline_ && pipeline_->IsEffectReady(PostEffectType::Vignette);
}

/// <summary>
/// Box Filter用PSOが生成済みか確認する。
/// </summary>
bool PostProcess::IsBoxFilterReady() const
{
    return pipeline_ && pipeline_->IsEffectReady(PostEffectType::BoxFilter);
}

/// <summary>
/// Gaussian Filter用PSOが生成済みか確認する。
/// </summary>
bool PostProcess::IsGaussianFilterReady() const
{
    return pipeline_ && pipeline_->IsEffectReady(PostEffectType::GaussianFilter);
}

/// <summary>
/// 輝度ベースOutline用PSOが生成済みか確認する。
/// </summary>
bool PostProcess::IsLuminanceOutlineReady() const
{
    return pipeline_ && pipeline_->IsEffectReady(PostEffectType::LuminanceOutline);
}

/// <summary>
/// 深度ベースOutline用PSOが生成済みか確認する。
/// </summary>
bool PostProcess::IsDepthOutlineReady() const
{
    return pipeline_ && pipeline_->IsEffectReady(PostEffectType::DepthOutline);
}

/// <summary>
/// Radial Blur用PSOが生成済みか確認する。
/// </summary>
bool PostProcess::IsRadialBlurReady() const
{
    return pipeline_ && pipeline_->IsEffectReady(PostEffectType::RadialBlur);
}

/// <summary>
/// 画面歪み用PSOが生成済みか確認する。
/// </summary>
bool PostProcess::IsDistortionReady() const
{
    return pipeline_ && pipeline_->IsEffectReady(PostEffectType::Distortion);
}

/// <summary>
/// Dissolve用PSOが生成済みか確認する。
/// </summary>
bool PostProcess::IsDissolveReady() const
{
    return pipeline_ && pipeline_->IsEffectReady(PostEffectType::Dissolve);
}

/// <summary>
/// Random用PSOが生成済みか確認する。
/// </summary>
bool PostProcess::IsRandomReady() const
{
    return pipeline_ && pipeline_->IsEffectReady(PostEffectType::Random);
}
/// <summary>
/// 通常の1passポストエフェクト用ルート定数を作成する。
/// </summary>
PostProcess::RootConstantData PostProcess::BuildTextureRootConstants(PostEffectType effectType) const
{
    RootConstantData filterSettings {}; // 通常の1passエフェクト用設定
    filterSettings[0] = settings_.boxFilterKernelSize;
    filterSettings[2] = ConvertFloatToRootConstant(settings_.gaussianSigma);
    WriteFloatRootConstant(filterSettings.data(), 3, settings_.outlineStrength);
    std::memcpy(&filterSettings[4], &settings_.radialBlurCenter, sizeof(Math::Vector2));
    WriteFloatRootConstant(filterSettings.data(), 6, settings_.radialBlurWidth);
    filterSettings[7] = settings_.radialBlurSampleCount;
    WriteFloatRootConstant(filterSettings.data(), 8, settings_.dissolveThreshold);
    WriteFloatRootConstant(filterSettings.data(), 9, settings_.dissolveEdgeWidth);
    std::memcpy(&filterSettings[12], &settings_.dissolveEdgeColor, sizeof(Math::Vector3));
    WriteFloatRootConstant(filterSettings.data(), 16, settings_.randomTime);
    WriteFloatRootConstant(filterSettings.data(), 17, settings_.randomStrength);
    WriteFloatRootConstant(filterSettings.data(), 18, settings_.randomScale);

    if (effectType == PostEffectType::Distortion) {
        std::memcpy(&filterSettings[4], &settings_.distortionCenter, sizeof(Math::Vector2));
        WriteFloatRootConstant(filterSettings.data(), 6, settings_.distortionStrength);
        WriteFloatRootConstant(filterSettings.data(), 8, settings_.distortionRadius);
        WriteFloatRootConstant(filterSettings.data(), 9, settings_.distortionWaveCount);
        WriteFloatRootConstant(filterSettings.data(), 10, settings_.distortionProgress);
    }

    return filterSettings;
}

/// <summary>
/// Gaussian Filter用ルート定数を作成する。
/// </summary>
PostProcess::RootConstantData PostProcess::BuildGaussianRootConstants(uint32_t direction) const
{
    RootConstantData filterSettings {}; // Gaussian Filter用設定
    filterSettings[0] = settings_.gaussianKernelSize;
    filterSettings[1] = direction;
    filterSettings[2] = ConvertFloatToRootConstant(settings_.gaussianSigma);
    return filterSettings;
}

/// <summary>
/// Dissolve用ルート定数を作成する。
/// </summary>
PostProcess::RootConstantData PostProcess::BuildDissolveRootConstants() const
{
    RootConstantData dissolveSettings {}; // Dissolve用ルート定数
    WriteFloatRootConstant(dissolveSettings.data(), 8, settings_.dissolveThreshold);
    WriteFloatRootConstant(dissolveSettings.data(), 9, settings_.dissolveEdgeWidth);
    std::memcpy(&dissolveSettings[12], &settings_.dissolveEdgeColor, sizeof(Math::Vector3));
    return dissolveSettings;
}

/// <summary>
/// Depth Outline用ルート定数を作成する。
/// </summary>
PostProcess::RootConstantData PostProcess::BuildDepthOutlineRootConstants(const Math::Matrix4x4& projectionMatrix) const
{
    RootConstantData outlineSettings {}; // Outline用ルート定数
    Math::Matrix4x4 projectionInverse = MathUtil::Inverse(projectionMatrix); // View空間復元用の逆射影行列
    WriteFloatRootConstant(outlineSettings.data(), 3, settings_.outlineStrength);
    WriteFloatRootConstant(outlineSettings.data(), 0, settings_.depthOutlineThreshold);
    WriteFloatRootConstant(outlineSettings.data(), 1, settings_.depthOutlineSoftness);
    std::memcpy(&outlineSettings[4], &projectionInverse, sizeof(Math::Matrix4x4));
    return outlineSettings;
}

/// <summary>
/// 指定したポストエフェクトでテクスチャを描画する。
/// </summary>
void PostProcess::DrawTexture(
    uint32_t srvIndex,
    PostEffectType effectType)
{
    if (!IsReady()) {
        return;
    }

    ID3D12PipelineState* pipelineState = pipeline_->GetPipelineState(effectType); // 描画に使用するPSO
    if (!pipelineState) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList(); // 描画命令を記録するコマンドリスト
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = dxCommon_->GetSRVGPUDescriptorHandle(srvIndex); // 入力テクスチャのSRV
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        dxCommon_->GetSrvDescriptorHeap()
    }; // 描画で使用するSRVヒープ

    commandList->SetGraphicsRootSignature(pipeline_->GetRootSignature());
    commandList->SetPipelineState(pipelineState);
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetGraphicsRootDescriptorTable(0, srvHandle);
    const RootConstantData filterSettings = BuildTextureRootConstants(effectType); // 通常の1passエフェクト用設定
    commandList->SetGraphicsRoot32BitConstants(
        2,
        static_cast<UINT>(filterSettings.size()),
        filterSettings.data(),
        0);
    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
    lastSrvIndex_ = srvIndex;
}

/// <summary>
/// Box Filterで使用するカーネルサイズを設定する
/// </summary>
void PostProcess::SetBoxFilterKernelSize(uint32_t kernelSize)
{
    settings_.SetBoxFilterKernelSize(kernelSize);
}

/// <summary>
/// Gaussian Filterで使用するカーネルサイズを設定する
/// </summary>
void PostProcess::SetGaussianKernelSize(uint32_t kernelSize)
{
    settings_.SetGaussianKernelSize(kernelSize);
}

/// <summary>
/// Gaussian Filterで使用する標準偏差を設定する
/// </summary>
void PostProcess::SetGaussianSigma(float sigma)
{
    settings_.SetGaussianSigma(sigma);
}

/// <summary>
/// Gaussian Filterの1方向分を描画する
/// </summary>
void PostProcess::DrawGaussianPass(uint32_t srvIndex, uint32_t direction)
{
    if (!IsReady() || !pipeline_->IsEffectReady(PostEffectType::GaussianFilter)) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList(); // 描画命令を記録するコマンドリスト
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = dxCommon_->GetSRVGPUDescriptorHandle(srvIndex); // 入力テクスチャのSRV
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        dxCommon_->GetSrvDescriptorHeap()
    }; // 描画で使用するSRVヒープ

    const RootConstantData filterSettings = BuildGaussianRootConstants(direction); // Gaussian Filter用設定

    commandList->SetGraphicsRootSignature(pipeline_->GetRootSignature());
    commandList->SetPipelineState(pipeline_->GetPipelineState(PostEffectType::GaussianFilter));
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetGraphicsRootDescriptorTable(0, srvHandle);
    commandList->SetGraphicsRoot32BitConstants(
        2,
        static_cast<UINT>(filterSettings.size()),
        filterSettings.data(),
        0);
    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
    lastSrvIndex_ = srvIndex;
}

/// <summary>
/// 中間レンダーターゲットを使用して分離型Gaussian Filterを描画する
/// </summary>
void PostProcess::DrawGaussianTexture(
    uint32_t sourceSrvIndex,
    int intermediateRenderTargetHandle,
    uint32_t intermediateSrvIndex)
{
    if (!enabled_) {
        DrawTexture(sourceSrvIndex, PostEffectType::Copy);
        return;
    }

    dxCommon_->BeginRenderTo(intermediateRenderTargetHandle, true);
    DrawGaussianPass(sourceSrvIndex, 0); // 横方向へぼかす
    dxCommon_->EndRenderTo(intermediateRenderTargetHandle);

    DrawGaussianPass(intermediateSrvIndex, 1); // 縦方向へぼかす
}

/// <summary>
/// Outlineの強度を設定する
/// </summary>
void PostProcess::SetOutlineStrength(float strength)
{
    settings_.SetOutlineStrength(strength);
}

/// <summary>
/// Depth Outlineで輪郭と判定する深度差の閾値を設定する
/// </summary>
void PostProcess::SetDepthOutlineThreshold(float threshold)
{
    settings_.SetDepthOutlineThreshold(threshold);
}

/// <summary>
/// Depth Outlineの輪郭の立ち上がり幅を設定する
/// </summary>
void PostProcess::SetDepthOutlineSoftness(float softness)
{
    settings_.SetDepthOutlineSoftness(softness);
}

/// <summary>
/// Radial Blurの中心座標を設定する
/// </summary>
void PostProcess::SetRadialBlurCenter(const Math::Vector2& center)
{
    settings_.SetRadialBlurCenter(center);
}

/// <summary>
/// Radial Blurの幅を設定する
/// </summary>
void PostProcess::SetRadialBlurWidth(float width)
{
    settings_.SetRadialBlurWidth(width);
}

void PostProcess::SetRadialBlurSampleCount(uint32_t sampleCount)
{
    settings_.SetRadialBlurSampleCount(sampleCount);
}

/// <summary>
/// 画面歪みの中心UV座標を設定する
/// </summary>
void PostProcess::SetDistortionCenter(const Math::Vector2& center)
{
    settings_.SetDistortionCenter(center);
}

/// <summary>
/// 画面歪みの強度を設定する
/// </summary>
void PostProcess::SetDistortionStrength(float strength)
{
    settings_.SetDistortionStrength(strength);
}

/// <summary>
/// 画面歪みの影響半径を設定する
/// </summary>
void PostProcess::SetDistortionRadius(float radius)
{
    settings_.SetDistortionRadius(radius);
}

/// <summary>
/// 画面歪みの波数を設定する
/// </summary>
void PostProcess::SetDistortionWaveCount(float waveCount)
{
    settings_.SetDistortionWaveCount(waveCount);
}

/// <summary>
/// 画面歪みの進行率を設定する
/// </summary>
void PostProcess::SetDistortionProgress(float progress)
{
    settings_.SetDistortionProgress(progress);
}

/// <summary>
/// Dissolveの閾値を設定する
/// </summary>
void PostProcess::SetDissolveThreshold(float threshold)
{
    settings_.SetDissolveThreshold(threshold);
}

/// <summary>
/// Dissolve境界の幅を設定する
/// </summary>
void PostProcess::SetDissolveEdgeWidth(float edgeWidth)
{
    settings_.SetDissolveEdgeWidth(edgeWidth);
}

/// <summary>
/// Dissolve境界の色を設定する
/// </summary>
void PostProcess::SetDissolveEdgeColor(const Math::Vector3& color)
{
    settings_.SetDissolveEdgeColor(color);
}

/// <summary>
/// ノイズマスクを使用してDissolveを描画する
/// </summary>
void PostProcess::DrawDissolveTexture(
    uint32_t sourceSrvIndex,
    uint32_t maskSrvIndex)
{
    if (!enabled_) {
        DrawTexture(sourceSrvIndex, PostEffectType::Copy);
        return;
    }

    if (!IsReady() || !pipeline_->IsEffectReady(PostEffectType::Dissolve)) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList(); // 描画命令を記録するコマンドリスト
    D3D12_GPU_DESCRIPTOR_HANDLE sourceSrvHandle = dxCommon_->GetSRVGPUDescriptorHandle(sourceSrvIndex); // 元画像のSRV
    D3D12_GPU_DESCRIPTOR_HANDLE maskSrvHandle = dxCommon_->GetSRVGPUDescriptorHandle(maskSrvIndex); // ノイズマスクのSRV
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        dxCommon_->GetSrvDescriptorHeap()
    }; // 描画で使用するSRVヒープ

    const RootConstantData dissolveSettings = BuildDissolveRootConstants(); // Dissolve用ルート定数

    commandList->SetGraphicsRootSignature(pipeline_->GetRootSignature());
    commandList->SetPipelineState(pipeline_->GetPipelineState(PostEffectType::Dissolve));
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetGraphicsRootDescriptorTable(0, sourceSrvHandle);
    commandList->SetGraphicsRootDescriptorTable(1, maskSrvHandle);
    commandList->SetGraphicsRoot32BitConstants(
        2,
        static_cast<UINT>(dissolveSettings.size()),
        dissolveSettings.data(),
        0);
    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
    lastSrvIndex_ = sourceSrvIndex;
}

/// <summary>
/// 時間経過を使用するポストエフェクトを更新する
/// </summary>
void PostProcess::Update(float deltaTime)
{
    settings_.AddRandomTime(deltaTime);
}

/// <summary>
/// Randomのノイズ強度を設定する
/// </summary>
void PostProcess::SetRandomStrength(float strength)
{
    settings_.SetRandomStrength(strength);
}

/// <summary>
/// Randomのノイズの細かさを設定する
/// </summary>
void PostProcess::SetRandomScale(float scale)
{
    settings_.SetRandomScale(scale);
}

/// <summary>
/// Randomの時間変化速度を設定する
/// </summary>
void PostProcess::SetRandomSpeed(float speed)
{
    settings_.SetRandomSpeed(speed);
}

/// <summary>
/// 深度テクスチャを使用してOutlineを描画する
/// </summary>
void PostProcess::DrawDepthOutline(
    uint32_t colorSrvIndex,
    uint32_t depthSrvIndex,
    const Math::Matrix4x4& projectionMatrix)
{
    if (!enabled_) {
        DrawTexture(colorSrvIndex, PostEffectType::Copy);
        return;
    }

    if (!IsReady() || !pipeline_->IsEffectReady(PostEffectType::DepthOutline)) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList(); // 描画命令を記録するコマンドリスト
    D3D12_GPU_DESCRIPTOR_HANDLE colorSrvHandle = dxCommon_->GetSRVGPUDescriptorHandle(colorSrvIndex); // カラーSRV
    D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle = dxCommon_->GetSRVGPUDescriptorHandle(depthSrvIndex); // 深度SRV
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        dxCommon_->GetSrvDescriptorHeap()
    }; // 描画で使用するSRVヒープ

    const RootConstantData outlineSettings = BuildDepthOutlineRootConstants(projectionMatrix); // Outline用ルート定数

    commandList->SetGraphicsRootSignature(pipeline_->GetRootSignature());
    commandList->SetPipelineState(pipeline_->GetPipelineState(PostEffectType::DepthOutline));
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetGraphicsRootDescriptorTable(0, colorSrvHandle);
    commandList->SetGraphicsRootDescriptorTable(1, depthSrvHandle);
    commandList->SetGraphicsRoot32BitConstants(
        2,
        static_cast<UINT>(outlineSettings.size()),
        outlineSettings.data(),
        0);
    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
    lastSrvIndex_ = colorSrvIndex;
}

/// <summary>
/// 現在選択されているポストエフェクトでテクスチャを描画する
/// </summary>
void PostProcess::DrawTexture(uint32_t srvIndex)
{
    PostEffectType drawEffectType = enabled_ ? effectType_ : PostEffectType::Copy; // 無効時は元画像を表示する
    DrawTexture(srvIndex, drawEffectType);
}

/// <summary>
/// RenderTargetのカラーSRVを指定したポストエフェクトで描画する。
/// </summary>
void PostProcess::DrawTexture(const RenderTarget& sourceRenderTarget, PostEffectType effectType)
{
    DrawTexture(sourceRenderTarget.GetColorSrvIndex(), effectType);
}

/// <summary>
/// RenderTargetのカラーSRVを現在のポストエフェクトで描画する。
/// </summary>
void PostProcess::DrawTexture(const RenderTarget& sourceRenderTarget)
{
    DrawTexture(sourceRenderTarget.GetColorSrvIndex());
}

/// <summary>
/// RenderTargetのカラーSRVからGaussian Filterの1方向分を描画する。
/// </summary>
void PostProcess::DrawGaussianPass(const RenderTarget& sourceRenderTarget, uint32_t direction)
{
    DrawGaussianPass(sourceRenderTarget.GetColorSrvIndex(), direction);
}

/// <summary>
/// RenderTargetを中間描画先として分離型Gaussian Filterを描画する。
/// </summary>
void PostProcess::DrawGaussianTexture(uint32_t sourceSrvIndex, RenderTarget& intermediateRenderTarget)
{
    if (!enabled_) {
        DrawTexture(sourceSrvIndex, PostEffectType::Copy);
        return;
    }

    intermediateRenderTarget.Begin(true);
    DrawGaussianPass(sourceSrvIndex, 0);
    intermediateRenderTarget.End();

    DrawGaussianPass(intermediateRenderTarget.GetColorSrvIndex(), 1);
}

/// <summary>
/// RenderTargetの深度SRVを使用してOutlineを描画する。
/// </summary>
void PostProcess::DrawDepthOutline(
    uint32_t colorSrvIndex,
    const RenderTarget& depthSourceRenderTarget,
    const Math::Matrix4x4& projectionMatrix)
{
    DrawDepthOutline(colorSrvIndex, depthSourceRenderTarget.GetDepthSrvIndex(), projectionMatrix);
}

/// <summary>
/// RenderTargetのカラーSRVとノイズマスクを使用してDissolveを描画する。
/// </summary>
void PostProcess::DrawDissolveTexture(
    const RenderTarget& sourceRenderTarget,
    uint32_t maskSrvIndex)
{
    DrawDissolveTexture(sourceRenderTarget.GetColorSrvIndex(), maskSrvIndex);
}
