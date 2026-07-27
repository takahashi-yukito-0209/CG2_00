#include "Object3dCommon.h"
#include "Camera.h"
#include "DebugCamera.h"
#include "Logger.h"
#include "StringUtility.h"
#include "engine/base/SrvManager.h"
#include "mathUtility.h"
#include <algorithm>
#include <cstring>
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

using namespace Math;
using namespace MyEngine;

namespace {
constexpr uint32_t kDefaultInstancingCount = 16384; // インスタンシング用の既定最大数
constexpr uint32_t kBillboardCameraCbCount = 256; // 1フレーム中に保持するビルボード用カメラCB数
constexpr size_t kConstantBufferAlignment = 0x100; // D3D12定数バッファのアラインメント
constexpr size_t kObjectLogBufferSize = 256; // Object3dCommonのログ用バッファサイズ
constexpr UINT kObjectRenderTargetCount = 1; // 3D描画で使用するRT数
constexpr UINT kObjectSampleCount = 1; // 3D描画のマルチサンプル数
constexpr UINT kLinearWrapSamplerRegister = 0; // 線形ラップサンプラーのレジスタ番号
constexpr UINT kPointClampSamplerRegister = 1; // ポイントクランプサンプラーのレジスタ番号
constexpr float kSamplerMipLodBias = 0.0f; // サンプラーのミップLODバイアス
constexpr float kSamplerMinLod = 0.0f; // サンプラーの最小LOD
constexpr float kDefaultDirectionalLightIntensity = 1.0f; // 平行光源の初期強度
constexpr float kDefaultPointLightRadius = 10.0f; // 点光源の初期半径
constexpr float kDefaultPointLightDecay = 2.0f; // 点光源の初期減衰率
constexpr float kDefaultSpotLightDistance = 10.0f; // スポットライトの初期距離
constexpr float kDefaultSpotLightDecay = 2.0f; // スポットライトの初期減衰率
constexpr float kDefaultSpotLightCosAngle = 1.0f; // スポットライト角度の初期cos値
constexpr float kDefaultSpotLightCosFalloffStart = 1.0f; // フォールオフ開始角度の初期cos値
constexpr int kLightDisabled = 0; // ライト無効状態
constexpr float kImGuiLightIntensityMin = 0.0f; // ライト強度の最小値
constexpr float kImGuiLightIntensityMax = 10.0f; // ライト強度の最大値
constexpr float kImGuiLightPositionStep = 0.1f; // ライト位置の調整幅
constexpr float kImGuiLightDirectionStep = 0.05f; // ライト方向の調整幅
constexpr float kImGuiLightRadiusMin = 0.0f; // 点光源半径の最小値
constexpr float kImGuiLightRadiusMax = 100.0f; // 点光源半径の最大値
constexpr float kImGuiLightDecayMin = 0.0f; // ライト減衰率の最小値
constexpr float kImGuiLightDecayMax = 10.0f; // ライト減衰率の最大値
constexpr float kRadiansToDegrees = 57.29577951308232f; // ラジアンから度数へ変換する倍率
constexpr float kDegreesToRadians = 0.017453292519943295f; // 度数からラジアンへ変換する倍率
constexpr float kCosClampMin = -1.0f; // acos前に使用するcos値の最小値
constexpr float kCosClampMax = 1.0f; // acos前に使用するcos値の最大値
constexpr float kImGuiConeAngleMin = 0.1f; // スポットライト角度の最小値
constexpr float kImGuiConeAngleMax = 179.0f; // スポットライト角度の最大値
constexpr float kImGuiFalloffAngleMin = 0.0f; // フォールオフ開始角度の最小値
constexpr float kDirectionNormalizeEpsilon = 1e-6f; // ライト方向を正規化する最小長さ
constexpr float kImGuiCameraTranslateStep = 0.1f; // カメラ位置の調整幅
constexpr float kImGuiCameraRotateStep = 0.01f; // カメラ回転の調整幅
/// <summary>
/// ブレンドモードがPSO配列の範囲内か確認する
/// </summary>
bool IsValidObjectBlendMode(BlendMode mode)
{
    const int modeIndex = static_cast<int>(mode); // 確認対象のブレンドモード番号
    return 0 <= modeIndex && modeIndex < static_cast<int>(BlendMode::Count);
}

/// <summary>
/// ブレンドモードをPSO配列の添字へ変換する
/// </summary>
size_t ToObjectBlendModeIndex(BlendMode mode)
{
    return static_cast<size_t>(mode);
}
} // namespace

/// <summary>
/// 初期化
/// </summary>
void Object3dCommon::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    // 引数で受け取ってメンバ変数に記録する
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    // 共有の平行光源用定数バッファを作成
    // これにより main/ImGui からすべての Object3d インスタンスで使用する単一のライトを編集できる
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        directionalLightResources_[frameIndex] = dxCommon_->CreateBufferResource(sizeof(Object3d::DirectionalLight));
        directionalLightResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedDirectionalLightData_[frameIndex]));
    }

    // バッファが作成できた場合はマッピングして初期値を設定する
    if (directionalLightData_) {
        // 既定値の設定
        directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
        directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
        directionalLightData_->intensity = kDefaultDirectionalLightIntensity;
        // デバッグログ
        {
            char buf[kObjectLogBufferSize];
            sprintf_s(buf, "DEBUG DirectionalLight default: color=(%f,%f,%f) dir=(%f,%f,%f) intensity=%f\n",
                directionalLightData_->color.x, directionalLightData_->color.y, directionalLightData_->color.z,
                directionalLightData_->direction.x, directionalLightData_->direction.y, directionalLightData_->direction.z,
                directionalLightData_->intensity);
            Logger::Log(buf);
        }
    }

    // 点光源用バッファを作成 (最大点光源数分の配列を想定)
    const uint32_t kMaxPointLights = Object3dCommon::kMaxPointLights;
    // CPU側の構造体レイアウトに合わせて、GPU側のバッファも同じレイアウトで作成する必要がある
    size_t pointLightsBufferSize = sizeof(Object3d::PointLight) * static_cast<size_t>(kMaxPointLights);
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        pointLightsResources_[frameIndex] = dxCommon_->CreateBufferResource(pointLightsBufferSize);
        pointLightsResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedPointLightsData_[frameIndex]));
    }

    // バッファが作成できた場合はマッピングして初期値を設定する
    if (pointLightsData_) {
        // デフォルトで無効化しておく
        for (uint32_t i = 0; i < kMaxPointLights; ++i) {
            // 無効化のため、位置を原点、色を白、半径と減衰を適当な値に設定し、enabled を 0 にする
            pointLightsData_[i].position = { 0.0f, 0.0f, 0.0f, 0.0f };
            pointLightsData_[i].color = { 1.0f, 1.0f, 1.0f, 1.0f };
            pointLightsData_[i].radius = kDefaultPointLightRadius;
            pointLightsData_[i].decay = kDefaultPointLightDecay;
            pointLightsData_[i].enabled = kLightDisabled;
            pointLightsData_[i].padding = 0.0f;
        }
    }

    // スポットライト用バッファを作成 (単一スポットライトを想定)
    size_t spotLightBufferSize = sizeof(Object3d::SpotLight);
    // CPU側の構造体レイアウトに合わせて、GPU側のバッファも同じレイアウトで作成する必要がある
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        spotLightResources_[frameIndex] = dxCommon_->CreateBufferResource(spotLightBufferSize);
        spotLightResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedSpotLightData_[frameIndex]));
    }
    // バッファが作成できた場合はマッピングして初期値を設定する
    if (spotLightData_) {
        // デフォルトで無効化しておく
        spotLightData_->position = { 0.0f, 0.0f, 0.0f, 0.0f };
        spotLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
        spotLightData_->direction = { 0.0f, -1.0f, 0.0f };
        spotLightData_->distance = kDefaultSpotLightDistance;
        spotLightData_->decay = kDefaultSpotLightDecay;
        spotLightData_->cosAngle = kDefaultSpotLightCosAngle;
        spotLightData_->cosFalloffStart = kDefaultSpotLightCosFalloffStart;
        spotLightData_->enabled = kLightDisabled;
        spotLightData_->padding = 0.0f;
    }

    // ビルボード用のカメラ定数バッファ（b2）を作成
    const size_t billboardCameraCbStride = (sizeof(CameraCB) + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1); // ビルボード用CBの1要素サイズ
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        cameraCBResources_[frameIndex] = dxCommon_->CreateBufferResource(billboardCameraCbStride * kBillboardCameraCbCount);
        cameraCBResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedCameraCBData_[frameIndex]));
    }
    // バッファが作成できた場合はマッピングして初期値を設定する
    if (cameraCBData_) {
        // CPU側のカメラ状態へ初期値を設定する
        cameraCBData_->right = { 1.0f, 0.0f, 0.0f };
        cameraCBData_->up = { 0.0f, 1.0f, 0.0f };
        cameraCBData_->enable = 0.0f;
        // 初期のViewProjは単位行列
        cameraCBData_->pad0 = 0.0f; // パディングを書き込むことを保証
        cameraCBData_->enable = 0.0f;
        cameraCBData_->viewProj = MathUtil::MakeIdentity4x4();
    }

    // カメラ用定数バッファ
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        cameraResources_[frameIndex] = dxCommon_->CreateBufferResource(sizeof(CameraForGPU));
        cameraResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedCameraData_[frameIndex]));
    }
    if (cameraData_) {
        cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
        cameraData_->exposure = 1.0f; // デフォルトの露出値
        cameraData_->toneMapOn = 1; // デフォルトでトーンマッピング有効
        cameraData_->hasEnvironmentMap = 0;
        cameraData_->pad1[0] = 0.0f;
        cameraData_->pad1[1] = 0.0f;
        cameraData_->view = MathUtil::MakeIdentity4x4();
    }

    // グラフィックスパイプラインの生成
    // ルートシグネチャはブレンドモードに依存しないため一度だけ生成する
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        if (!mappedCameraCBData_[frameIndex]) {
            continue;
        }
        std::memcpy(mappedCameraCBData_[frameIndex], cameraCBData_, sizeof(CameraCB));
        cameraCBWriteIndices_[frameIndex] = 1;
    }
    if (dxCommon_ && cameraCBResources_[dxCommon_->GetCurrentFrameIndex()]) {
        currentCameraCBGpuAddress_ = cameraCBResources_[dxCommon_->GetCurrentFrameIndex()]->GetGPUVirtualAddress();
    }

    CreateRootSignature();
    for (int modeIndex = 0; modeIndex < static_cast<int>(BlendMode::Count); ++modeIndex) {
        BlendMode mode = static_cast<BlendMode>(modeIndex); // 作成対象のブレンドモード
        CreateGraphicsPipeline(mode);
    }

    // インスタンシング用リソースの作成（構造化バッファ + SRV）
    kNumInstance_ = kDefaultInstancingCount;

    // TransformationMatrix[kDefaultInstancingCount] を格納する構造化バッファ向けのGPU可視SRVを作成
    D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc {};
    instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN; // 構造化バッファ
    instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER; // バッファビュー
    instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // バッファの先頭から使用
    instancingSrvDesc.Buffer.FirstElement = 0; // バッファの先頭から
    instancingSrvDesc.Buffer.NumElements = kDefaultInstancingCount; // kDefaultInstancingCount分の構造体を格納
    instancingSrvDesc.Buffer.StructureByteStride = sizeof(Object3d::TransformationMatrix); // 1インスタンス分のサイズ
    instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE; // フラグは特に必要ないため NONE

    // インスタンスデータを格納するため、UPLOADヒープに既定サイズのバッファリソースを作成
    size_t instancingBufferSize = sizeof(Object3d::TransformationMatrix) * kDefaultInstancingCount;
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        instancingResources_[frameIndex] = dxCommon_->CreateBufferResource(instancingBufferSize);
        instancingResources_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_[frameIndex]));
        for (uint32_t instanceIndex = 0; instanceIndex < kDefaultInstancingCount; ++instanceIndex) {
            instancingData_[frameIndex][instanceIndex].WVP = MathUtil::MakeIdentity4x4();
            instancingData_[frameIndex][instanceIndex].World = MathUtil::MakeIdentity4x4();
            instancingData_[frameIndex][instanceIndex].color = { 1.0f, 1.0f, 1.0f, 1.0f };
        }
    }
    // グローバルSRVヒープにSRVディスクリプタを作成
    // TextureManagerと競合しにくいディスクリプタスロットとして、ヒープの最後のスロットを使用
    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        if (!srvManager_ || !srvManager_->CanAllocate()) {
            Logger::Log("Object3dCommon::Initialize: failed to allocate instancing SRV.\n");
            kNumInstance_ = 0;
            return;
        }
        instancingSrvIndices_[frameIndex] = srvManager_->Allocate();
        instancingSrvHandlesCPU_[frameIndex] = srvManager_->GetCPUDescriptorHandle(instancingSrvIndices_[frameIndex]);
        instancingSrvHandlesGPU_[frameIndex] = srvManager_->GetGPUDescriptorHandle(instancingSrvIndices_[frameIndex]);
        dxCommon_->GetDevice()->CreateShaderResourceView(instancingResources_[frameIndex].Get(), &instancingSrvDesc, instancingSrvHandlesCPU_[frameIndex]);
    }
    // デバッグログ
    {
        char buf[kObjectLogBufferSize];
        sprintf_s(buf, "Object3dCommon::Initialize: created instancingResource size=%zu srvIndex=%u srvGPU=0x%016llX\n",
            instancingBufferSize, instancingSrvIndices_[0], static_cast<unsigned long long>(instancingSrvHandlesGPU_[0].ptr));
        Logger::Log(buf);
    }
}

/// <summary>
/// インスタンシング用SRVの割り当てを解放する
/// </summary>
void Object3dCommon::Finalize()
{
    auto releaseMappedResource = [this](Microsoft::WRL::ComPtr<ID3D12Resource>& resource, auto*& mappedData) {
        if (resource && mappedData) {
            resource->Unmap(0, nullptr);
        }
        mappedData = nullptr;
        if (dxCommon_) {
            dxCommon_->DeferReleaseResource(resource);
            return;
        }
        resource.Reset();
    }; // マップ済みGPUリソースを安全に解放予約する処理

    for (uint32_t frameIndex = 0; frameIndex < DirectXCommon::kFrameCount; ++frameIndex) {
        if (srvManager_ && instancingSrvIndices_[frameIndex] != UINT32_MAX) {
            srvManager_->Free(instancingSrvIndices_[frameIndex]);
        }
        instancingSrvIndices_[frameIndex] = UINT32_MAX;
        instancingSrvHandlesCPU_[frameIndex] = {};
        instancingSrvHandlesGPU_[frameIndex] = {};
        releaseMappedResource(directionalLightResources_[frameIndex], mappedDirectionalLightData_[frameIndex]);
        releaseMappedResource(pointLightsResources_[frameIndex], mappedPointLightsData_[frameIndex]);
        releaseMappedResource(spotLightResources_[frameIndex], mappedSpotLightData_[frameIndex]);
        releaseMappedResource(cameraResources_[frameIndex], mappedCameraData_[frameIndex]);
        releaseMappedResource(cameraCBResources_[frameIndex], mappedCameraCBData_[frameIndex]);
        releaseMappedResource(instancingResources_[frameIndex], instancingData_[frameIndex]);
        cameraCBWriteIndices_[frameIndex] = 0;
    }

    rootSignature_.Reset();
    for (auto& pipelineState : graphicsPipelineStates_) {
        pipelineState.Reset();
    }
    for (auto& pipelineState : skinningPipelineStates_) {
        pipelineState.Reset();
    }
    for (auto& pipelineState : skeletonDebugPipelineStates_) {
        pipelineState.Reset();
    }
    instancingPipelineState_.Reset();

    currentCameraCBGpuAddress_ = 0;
    environmentMapSrvHandleGPU_ = {};
    kNumInstance_ = 0;
    srvManager_ = nullptr;
    dxCommon_ = nullptr;
}

void Object3dCommon::SetEnvironmentMapSrvIndex(uint32_t srvIndex)
{
    environmentMapSrvHandleGPU_ = {};
    if (!dxCommon_ || srvIndex == UINT32_MAX) {
        return;
    }
    environmentMapSrvHandleGPU_ = dxCommon_->GetSRVGPUDescriptorHandle(srvIndex);
}

#ifdef USE_IMGUI
/// <summary>
/// Object3dCommon に関連する共通設定を編集する関数
/// </summary>
void Object3dCommon::DrawImGui()
{
    ImGui::PushID("Object3dCommon");

    // 平行光源の編集UI
    if (directionalLightData_) {
        if (ImGui::CollapsingHeader("Light")) {
            float color[3] = { directionalLightData_->color.x, directionalLightData_->color.y, directionalLightData_->color.z };
            if (ImGui::ColorEdit3("Color", color)) {
                directionalLightData_->color.x = color[0];
                directionalLightData_->color.y = color[1];
                directionalLightData_->color.z = color[2];
            }
            float intensity = directionalLightData_->intensity;
            if (ImGui::SliderFloat("Intensity", &intensity, kImGuiLightIntensityMin, kImGuiLightIntensityMax)) {
                directionalLightData_->intensity = intensity;
            }
            // Point lights UI
            if (ImGui::TreeNode("Point Lights")) {
                if (pointLightsData_) {
                    for (uint32_t i = 0; i < Object3dCommon::kMaxPointLights; ++i) {
                        ImGui::PushID((int)i);
                        char label[64];
                        sprintf_s(label, "Point %u", i);
                        if (ImGui::CollapsingHeader(label)) {
                            bool enabled = pointLightsData_[i].enabled != 0;
                            if (ImGui::Checkbox("Enabled", &enabled)) {
                                pointLightsData_[i].enabled = enabled ? 1 : 0;
                            }

                            float pos[3] = { pointLightsData_[i].position.x, pointLightsData_[i].position.y, pointLightsData_[i].position.z };
                            if (ImGui::DragFloat3("Position", pos, kImGuiLightPositionStep)) {
                                pointLightsData_[i].position.x = pos[0];
                                pointLightsData_[i].position.y = pos[1];
                                pointLightsData_[i].position.z = pos[2];
                            }

                            float col[3] = { pointLightsData_[i].color.x, pointLightsData_[i].color.y, pointLightsData_[i].color.z };
                            if (ImGui::ColorEdit3("Color", col)) {
                                pointLightsData_[i].color.x = col[0];
                                pointLightsData_[i].color.y = col[1];
                                pointLightsData_[i].color.z = col[2];
                            }

                            float pIntensity = pointLightsData_[i].color.w;
                            if (ImGui::SliderFloat("Intensity", &pIntensity, kImGuiLightIntensityMin, kImGuiLightIntensityMax)) {
                                pointLightsData_[i].color.w = pIntensity;
                            }

                            float radius = pointLightsData_[i].radius;
                            if (ImGui::SliderFloat("Radius", &radius, kImGuiLightRadiusMin, kImGuiLightRadiusMax)) {
                                pointLightsData_[i].radius = radius;
                            }

                            float decay = pointLightsData_[i].decay;
                            if (ImGui::SliderFloat("Decay", &decay, kImGuiLightDecayMin, kImGuiLightDecayMax)) {
                                pointLightsData_[i].decay = decay;
                            }
                        }
                        ImGui::PopID();
                    }
                } else {
                    ImGui::Text("Point lights not available");
                }
                ImGui::TreePop();
            }

            // Spot light UI
            if (ImGui::TreeNode("Spot Light")) {
                if (spotLightData_) {
                    bool enabled = spotLightData_->enabled != 0;
                    if (ImGui::Checkbox("Enabled", &enabled)) {
                        spotLightData_->enabled = enabled ? 1 : 0;
                    }

                    float spos[3] = { spotLightData_->position.x, spotLightData_->position.y, spotLightData_->position.z };
                    if (ImGui::DragFloat3("Position", spos, kImGuiLightPositionStep)) {
                        spotLightData_->position.x = spos[0];
                        spotLightData_->position.y = spos[1];
                        spotLightData_->position.z = spos[2];
                    }

                    float sdir[3] = { spotLightData_->direction.x, spotLightData_->direction.y, spotLightData_->direction.z };
                    if (ImGui::DragFloat3("Direction", sdir, kImGuiLightDirectionStep)) {
                        // normalize direction if possible
                        float len = sqrtf(sdir[0] * sdir[0] + sdir[1] * sdir[1] + sdir[2] * sdir[2]);
                        if (len > kDirectionNormalizeEpsilon) {
                            spotLightData_->direction.x = sdir[0] / len;
                            spotLightData_->direction.y = sdir[1] / len;
                            spotLightData_->direction.z = sdir[2] / len;
                        }
                    }

                    float scol[3] = { spotLightData_->color.x, spotLightData_->color.y, spotLightData_->color.z };
                    if (ImGui::ColorEdit3("Color", scol)) {
                        spotLightData_->color.x = scol[0];
                        spotLightData_->color.y = scol[1];
                        spotLightData_->color.z = scol[2];
                    }

                    float sIntensity = spotLightData_->color.w;
                    if (ImGui::SliderFloat("Intensity", &sIntensity, kImGuiLightIntensityMin, kImGuiLightIntensityMax)) {
                        spotLightData_->color.w = sIntensity;
                    }

                    float distance = spotLightData_->distance;
                    if (ImGui::SliderFloat("Distance", &distance, kImGuiLightRadiusMin, kImGuiLightRadiusMax)) {
                        spotLightData_->distance = distance;
                    }

                    float decay = spotLightData_->decay;
                    if (ImGui::SliderFloat("Decay", &decay, kImGuiLightDecayMin, kImGuiLightDecayMax)) {
                        spotLightData_->decay = decay;
                    }

                    // Cone angle editing (convert between cos and degrees for usability)
                    const float rad2deg = kRadiansToDegrees;
                    const float deg2rad = kDegreesToRadians;
                    float angleDeg = acosf(fmaxf(kCosClampMin, fminf(kCosClampMax, spotLightData_->cosAngle))) * rad2deg;
                    if (ImGui::SliderFloat("Cone Angle (deg)", &angleDeg, kImGuiConeAngleMin, kImGuiConeAngleMax)) {
                        spotLightData_->cosAngle = cosf(angleDeg * deg2rad);
                    }

                    float falloffDeg = acosf(fmaxf(kCosClampMin, fminf(kCosClampMax, spotLightData_->cosFalloffStart))) * rad2deg;
                    if (ImGui::SliderFloat("Falloff Start (deg)", &falloffDeg, kImGuiFalloffAngleMin, angleDeg)) {
                        spotLightData_->cosFalloffStart = cosf(falloffDeg * deg2rad);
                    }
                } else {
                    ImGui::Text("Spot light not available");
                }
                ImGui::TreePop();
            }
        }
    }

    ImGui::PopID();
}

/// <summary>
/// カメラ関連の設定を編集する関数
/// </summary>
void Object3dCommon::DrawCameraImGui()
{
    // デバッグカメラ切替・入力トグルおよびメイン/デバッグカメラのTransform編集UIを提供する
    // Use Debug Camera for Render
    if (ImGui::Checkbox("Use Debug Camera for Render", &useDebugCameraForRender_)) {
        // nothing else here, Game側のフラグと同期されている場合は呼び出し元で反映される
    }

    // Enable Debug Camera Input
    ImGui::Checkbox("Enable Debug Camera Input", &enableDebugCameraInput_);

    // メインカメラの編集
    if (ImGui::CollapsingHeader("Main Camera")) {
        if (defaultCamera_) {
            auto t = defaultCamera_->GetTranslate();
            if (ImGui::DragFloat3("Translate", &t.x, kImGuiCameraTranslateStep)) {
                defaultCamera_->SetTranslate(t);
            }
            auto r = defaultCamera_->GetRotate();
            if (ImGui::DragFloat3("Rotate", &r.x, kImGuiCameraRotateStep)) {
                defaultCamera_->SetRotate(r);
            }
        } else {
            ImGui::Text("Main Camera not available");
        }
    }

    // デバッグカメラの編集
    if (ImGui::CollapsingHeader("Debug Camera")) {
        if (debugCamera_) {
            // DebugCamera uses Math::Vector3 for translation/rotation
            Math::Vector3 dt = debugCamera_->GetTranslation();
            float dt_arr[3] = { dt.x, dt.y, dt.z };
            if (ImGui::DragFloat3("Debug Translate", dt_arr, kImGuiCameraTranslateStep)) {
                debugCamera_->SetTranslation({ dt_arr[0], dt_arr[1], dt_arr[2] });
            }
            Math::Vector3 dr = debugCamera_->GetRotation();
            float dr_arr[3] = { dr.x, dr.y, dr.z };
            if (ImGui::DragFloat3("Debug Rotate", dr_arr, kImGuiCameraRotateStep)) {
                debugCamera_->SetRotation({ dr_arr[0], dr_arr[1], dr_arr[2] });
            }
        } else {
            ImGui::Text("Debug Camera not available");
        }
    }
}
#else
void Object3dCommon::DrawImGui() { (void)0; }
void Object3dCommon::DrawCameraImGui() { (void)0; }
#endif

/// <summary>
/// ブレンドモードの設定
/// </summary>
void Object3dCommon::SetBlendMode(BlendMode mode)
{
    if (!IsValidObjectBlendMode(mode)) {
        Logger::Log("Object3dCommon::SetBlendMode: invalid blend mode\n");
        return;
    }

    const size_t modeIndex = ToObjectBlendModeIndex(mode); // 参照するPSO配列の添字
    if (!graphicsPipelineStates_[modeIndex]) {
        CreateGraphicsPipeline(mode);
    }

    if (!graphicsPipelineStates_[modeIndex]) {
        Logger::Log("Object3dCommon::SetBlendMode: pipeline state for blend mode is not ready\n");
        return;
    }

    blendMode_ = mode;
}



/// <summary>
/// インスタンシング／パーティクル用の描画設定をコマンドリストに設定
/// </summary>
void Object3dCommon::SetInstancingDrawSetting()
{
    const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex(); // 描画対象フレーム番号
    if (mappedDirectionalLightData_[frameIndex]) {
        *mappedDirectionalLightData_[frameIndex] = directionalLightState_;
    }
    if (mappedPointLightsData_[frameIndex]) {
        memcpy(mappedPointLightsData_[frameIndex], pointLightsState_.data(), sizeof(Object3d::PointLight) * pointLightsState_.size());
    }
    if (mappedSpotLightData_[frameIndex]) {
        *mappedSpotLightData_[frameIndex] = spotLightState_;
    }
    cameraState_.hasEnvironmentMap = environmentMapSrvHandleGPU_.ptr != 0 ? 1 : 0;
    if (mappedCameraData_[frameIndex]) {
        *mappedCameraData_[frameIndex] = cameraState_;
    }
    if (mappedCameraCBData_[frameIndex]) {
        StoreBillboardCameraData(cameraCBState_);
    }

    // 実行時のヌル参照を回避するための防御チェック
    auto cmdList = dxCommon_ ? dxCommon_->GetCommandList() : nullptr;

    // ここでcmdListがnullの場合は、GPUクラッシュを回避するために早期リターンする
    if (!cmdList) {
        Logger::Log("Object3dCommon::SetInstancingDrawSetting: command list is null\n");
        return;
    }

    // ルートシグネチャがない場合もGPUクラッシュを回避するために早期リターンする
    if (!rootSignature_) {
        Logger::Log("Object3dCommon::SetInstancingDrawSetting: rootSignature_ is null\n");
        return;
    }

    // フォールバック: インスタンシング用PSOが未準備なら標準のグラフィックスPSOを使用してGPUクラッシュを回避
    if (!instancingPipelineState_) {
        Logger::Log("Object3dCommon::SetInstancingDrawSetting: instancingPipelineState_ is null, falling back to current graphics PSO\n");
        if (!IsValidObjectBlendMode(blendMode_)) {
            Logger::Log("Object3dCommon::SetInstancingDrawSetting: invalid blendMode_\n");
            return;
        }
        auto& fallbackPipelineState = graphicsPipelineStates_[ToObjectBlendModeIndex(blendMode_)]; // フォールバック用PSO
        if (!fallbackPipelineState) {
            Logger::Log("Object3dCommon::SetInstancingDrawSetting: fallback graphics PSO is null\n");
            return;
        }
        cmdList->SetGraphicsRootSignature(rootSignature_.Get()); // ルートシグネチャは共通
        cmdList->SetPipelineState(fallbackPipelineState.Get()); // フォールバックして通常のグラフィックスPSOを使用
    } else {
        cmdList->SetGraphicsRootSignature(rootSignature_.Get()); // ルートシグネチャは共通
        cmdList->SetPipelineState(instancingPipelineState_.Get()); // インスタンシング用PSOを使用
    }

    // プリミティブトポロジーをセットするコマンド
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // 使用可能ならビルボード用カメラCB（VSのb2 -> ルートインデックス5）をバインド
    const D3D12_GPU_VIRTUAL_ADDRESS billboardCameraAddress = GetCurrentBillboardCameraGpuAddress(); // この描画で使うビルボード用CB
    if (billboardCameraAddress != 0) {
        cmdList->SetGraphicsRootConstantBufferView(5, billboardCameraAddress);
    }

    // スポットライトCBVをルートにバインド (ルートインデックス8 -> PS b5)
    if (spotLightResources_[frameIndex]) {
        cmdList->SetGraphicsRootConstantBufferView(8, spotLightResources_[frameIndex]->GetGPUVirtualAddress());
    }

    // 平行光源（ディレクショナルライト）CBVをルートにバインド (ルートインデックス3 -> PS b1)
    if (directionalLightResources_[frameIndex]) {
        cmdList->SetGraphicsRootConstantBufferView(3, directionalLightResources_[frameIndex]->GetGPUVirtualAddress());
    }

    // カメラCBVをピクセルシェーダー側にもバインドしておく (ルートインデックス6 -> PS b3)
    if (cameraResources_[frameIndex]) {
        cmdList->SetGraphicsRootConstantBufferView(6, cameraResources_[frameIndex]->GetGPUVirtualAddress());
    }

    if (environmentMapSrvHandleGPU_.ptr != 0) {
        cmdList->SetGraphicsRootDescriptorTable(9, environmentMapSrvHandleGPU_);
    }
}

/// <summary>
/// ビルボード描画用のカメラベクトルを設定する。
/// </summary>
void Object3dCommon::SetBillboardCamera(const Math::Vector3& right, const Math::Vector3& up, bool enable)
{
    CameraCB cameraCB = cameraCBState_; // 更新元にする現在のカメラCB
    cameraCB.right = right;
    cameraCB.up = up;
    cameraCB.enable = enable ? 1.0f : 0.0f;
    StoreBillboardCameraData(cameraCB);
}

/// <summary>
/// ビルボード描画用のカメラベクトルとビュー射影行列を設定する。
/// </summary>
void Object3dCommon::SetBillboardCameraWithVP(const Math::Vector3& right, const Math::Vector3& up, const Math::Matrix4x4& viewProj, bool enable)
{
    CameraCB cameraCB = cameraCBState_; // 更新元にする現在のカメラCB
    cameraCB.right = right;
    cameraCB.up = up;
    cameraCB.enable = enable ? 1.0f : 0.0f;
    cameraCB.viewProj = viewProj;
    StoreBillboardCameraData(cameraCB);
}

/// <summary>
/// ビルボード描画用のカメラ定数を描画単位のスロットへ保存する。
/// </summary>
void Object3dCommon::StoreBillboardCameraData(const CameraCB& cameraCB)
{
    cameraCBState_ = cameraCB;
    cameraCBData_ = &cameraCBState_;
    if (!dxCommon_) {
        currentCameraCBGpuAddress_ = 0;
        return;
    }

    const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex(); // 書き込み対象のフレーム番号
    if (!cameraCBResources_[frameIndex] || !mappedCameraCBData_[frameIndex]) {
        currentCameraCBGpuAddress_ = 0;
        return;
    }

    const uint32_t slotIndex = cameraCBWriteIndices_[frameIndex] % kBillboardCameraCbCount; // 今回使うCBスロット
    const size_t billboardCameraCbStride = (sizeof(CameraCB) + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1); // ビルボード用CBの1要素サイズ
    const size_t writeOffset = static_cast<size_t>(slotIndex) * billboardCameraCbStride; // 256byte境界の書き込み位置
    std::memcpy(mappedCameraCBData_[frameIndex] + writeOffset, &cameraCBState_, sizeof(CameraCB));
    currentCameraCBGpuAddress_ = cameraCBResources_[frameIndex]->GetGPUVirtualAddress() + writeOffset;
    ++cameraCBWriteIndices_[frameIndex];
}

/// <summary>
/// 現在のビルボード描画用カメラ定数バッファのGPUアドレスを取得する。
/// </summary>
D3D12_GPU_VIRTUAL_ADDRESS Object3dCommon::GetCurrentBillboardCameraGpuAddress() const
{
    if (currentCameraCBGpuAddress_ != 0) {
        return currentCameraCBGpuAddress_;
    }
    if (!dxCommon_) {
        return 0;
    }

    const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex(); // 参照対象のフレーム番号
    return cameraCBResources_[frameIndex] ? cameraCBResources_[frameIndex]->GetGPUVirtualAddress() : 0;
}

/// <summary>
/// Skinning用の描画設定をコマンドリストに設定
/// </summary>
void Object3dCommon::SetSkinningDrawSetting()
{
    const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex(); // 描画対象フレーム番号
    if (mappedDirectionalLightData_[frameIndex]) {
        *mappedDirectionalLightData_[frameIndex] = directionalLightState_;
    }
    if (mappedPointLightsData_[frameIndex]) {
        memcpy(mappedPointLightsData_[frameIndex], pointLightsState_.data(), sizeof(Object3d::PointLight) * pointLightsState_.size());
    }
    if (mappedSpotLightData_[frameIndex]) {
        *mappedSpotLightData_[frameIndex] = spotLightState_;
    }
    if (mappedCameraData_[frameIndex]) {
        *mappedCameraData_[frameIndex] = cameraState_;
    }
    if (mappedCameraCBData_[frameIndex]) {
        StoreBillboardCameraData(cameraCBState_);
    }

    auto cmdList = dxCommon_->GetCommandList(); // 描画コマンドリスト
    if (!cmdList || !rootSignature_ || !IsValidObjectBlendMode(blendMode_)) {
        return;
    }

    auto& pipelineState = skinningPipelineStates_[ToObjectBlendModeIndex(blendMode_)]; // Skinning用PSO
    if (!pipelineState) {
        return;
    }

    cmdList->SetGraphicsRootSignature(rootSignature_.Get());
    cmdList->SetPipelineState(pipelineState.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (spotLightResources_[frameIndex]) {
        cmdList->SetGraphicsRootConstantBufferView(8, spotLightResources_[frameIndex]->GetGPUVirtualAddress());
    }
    if (directionalLightResources_[frameIndex]) {
        cmdList->SetGraphicsRootConstantBufferView(3, directionalLightResources_[frameIndex]->GetGPUVirtualAddress());
    }
    if (cameraResources_[frameIndex]) {
        cmdList->SetGraphicsRootConstantBufferView(6, cameraResources_[frameIndex]->GetGPUVirtualAddress());
    }
    if (environmentMapSrvHandleGPU_.ptr != 0) {
        cmdList->SetGraphicsRootDescriptorTable(9, environmentMapSrvHandleGPU_);
    }
}
/// <summary>
/// Skeletonデバッグ用の描画設定をコマンドリストに設定
/// </summary>
void Object3dCommon::SetSkeletonDebugDrawSetting()
{
    const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex(); // 描画対象フレーム番号
    if (mappedDirectionalLightData_[frameIndex]) {
        *mappedDirectionalLightData_[frameIndex] = directionalLightState_;
    }
    if (mappedPointLightsData_[frameIndex]) {
        memcpy(mappedPointLightsData_[frameIndex], pointLightsState_.data(), sizeof(Object3d::PointLight) * pointLightsState_.size());
    }
    if (mappedSpotLightData_[frameIndex]) {
        *mappedSpotLightData_[frameIndex] = spotLightState_;
    }
    if (mappedCameraData_[frameIndex]) {
        *mappedCameraData_[frameIndex] = cameraState_;
    }
    if (mappedCameraCBData_[frameIndex]) {
        StoreBillboardCameraData(cameraCBState_);
    }

    auto cmdList = dxCommon_->GetCommandList(); // 描画コマンドリスト
    if (!cmdList || !rootSignature_ || !IsValidObjectBlendMode(blendMode_)) {
        return;
    }

    auto& pipelineState = skeletonDebugPipelineStates_[ToObjectBlendModeIndex(blendMode_)]; // Skeletonデバッグ用PSO
    if (!pipelineState) {
        return;
    }

    cmdList->SetGraphicsRootSignature(rootSignature_.Get());
    cmdList->SetPipelineState(pipelineState.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (spotLightResources_[frameIndex]) {
        cmdList->SetGraphicsRootConstantBufferView(8, spotLightResources_[frameIndex]->GetGPUVirtualAddress());
    }
    if (directionalLightResources_[frameIndex]) {
        cmdList->SetGraphicsRootConstantBufferView(3, directionalLightResources_[frameIndex]->GetGPUVirtualAddress());
    }
    if (cameraResources_[frameIndex]) {
        cmdList->SetGraphicsRootConstantBufferView(6, cameraResources_[frameIndex]->GetGPUVirtualAddress());
    }
    if (environmentMapSrvHandleGPU_.ptr != 0) {
        cmdList->SetGraphicsRootDescriptorTable(9, environmentMapSrvHandleGPU_);
    }
}
void Object3dCommon::SetCommonDrawSetting()
{
    const uint32_t frameIndex = dxCommon_->GetCurrentFrameIndex(); // 描画対象フレーム番号
    if (mappedDirectionalLightData_[frameIndex]) {
        *mappedDirectionalLightData_[frameIndex] = directionalLightState_;
    }
    if (mappedPointLightsData_[frameIndex]) {
        memcpy(mappedPointLightsData_[frameIndex], pointLightsState_.data(), sizeof(Object3d::PointLight) * pointLightsState_.size());
    }
    if (mappedSpotLightData_[frameIndex]) {
        *mappedSpotLightData_[frameIndex] = spotLightState_;
    }
    cameraState_.hasEnvironmentMap = environmentMapSrvHandleGPU_.ptr != 0 ? 1 : 0;
    if (mappedCameraData_[frameIndex]) {
        *mappedCameraData_[frameIndex] = cameraState_;
    }
    if (mappedCameraCBData_[frameIndex]) {
        StoreBillboardCameraData(cameraCBState_);
    }

    // 実行時のヌル参照を回避するための防御チェック
    auto cmdList = dxCommon_ ? dxCommon_->GetCommandList() : nullptr;

    // ここでcmdListがnullの場合は、GPUクラッシュを回避するために早期リターンする
    if (!cmdList) {
        Logger::Log("Object3dCommon::SetCommonDrawSetting: command list is null\n");
        return;
    }

    // ルートシグネチャやPSOがない場合もGPUクラッシュを回避するために早期リターンする
    if (!rootSignature_) {
        Logger::Log("Object3dCommon::SetCommonDrawSetting: rootSignature_ is null\n");
        return;
    }

    // ここでグラフィックスパイプラインステートがnullの場合もGPUクラッシュを回避するために早期リターンする
    if (!IsValidObjectBlendMode(blendMode_)) {
        Logger::Log("Object3dCommon::SetCommonDrawSetting: invalid blendMode_\n");
        return;
    }

    auto& pipelineState = graphicsPipelineStates_[ToObjectBlendModeIndex(blendMode_)]; // 現在のブレンドモード用PSO
    if (!pipelineState) {
        Logger::Log("Object3dCommon::SetCommonDrawSetting: graphicsPipelineState is null\n");
        return;
    }

    cmdList->SetGraphicsRootSignature(rootSignature_.Get());
    cmdList->SetPipelineState(pipelineState.Get()); // PSOを設定
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 形状を設定

    // Spot light CBV を全ての通常描画パスでもバインドする（シェーダー b5 / ルートパラメータ 8）
    if (spotLightResources_[frameIndex]) {
        cmdList->SetGraphicsRootConstantBufferView(8, spotLightResources_[frameIndex]->GetGPUVirtualAddress());
    }

    // 平行光源（ディレクショナルライト）CBVをルートにバインド (ルートインデックス3 -> PS b1)
    if (directionalLightResources_[frameIndex]) {
        cmdList->SetGraphicsRootConstantBufferView(3, directionalLightResources_[frameIndex]->GetGPUVirtualAddress());
    }

    // カメラCBVをピクセルシェーダー側にもバインドしておく (ルートインデックス6 -> PS b3)
    if (cameraResources_[frameIndex]) {
        cmdList->SetGraphicsRootConstantBufferView(6, cameraResources_[frameIndex]->GetGPUVirtualAddress());
    }

    if (environmentMapSrvHandleGPU_.ptr != 0) {
        cmdList->SetGraphicsRootDescriptorTable(9, environmentMapSrvHandleGPU_);
    }
}

/// <summary>
/// ルートシグネチャの作成
/// </summary>
void Object3dCommon::CreateRootSignature()
{

    HRESULT hr;

    // ディスクリプタレンジ作成 (pixel texture SRV)
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
    descriptorRange[0].NumDescriptors = 1; // 数は1つ
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算

    // ディスクリプタレンジ作成 (vertex instancing SRV)
    D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
    descriptorRangeForInstancing[0].BaseShaderRegister = 1; // t1 in VS
    descriptorRangeForInstancing[0].NumDescriptors = 1;
    descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE descriptorRangeForSkinning[1] = {};
    descriptorRangeForSkinning[0].BaseShaderRegister = 3; // t3 in VS
    descriptorRangeForSkinning[0].NumDescriptors = 1;
    descriptorRangeForSkinning[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeForSkinning[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE descriptorRangeForEnvironment[1] = {};
    descriptorRangeForEnvironment[0].BaseShaderRegister = 2; // t2 in PS
    descriptorRangeForEnvironment[0].NumDescriptors = 1;
    descriptorRangeForEnvironment[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeForEnvironment[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // ルートシグネチャ作成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature {};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // RootParameter作成。複数設定できるので配列。

    // インデックス割り当て:
    // 0 = マテリアルCBV（ピクセル, b0）
    // 1 = WVP CBV（頂点, b0）
    // 2 = テクスチャSRVテーブル（ピクセル, t0）
    // 3 = 光源CBV（ピクセル, b1）
    // 4 = インスタンシングSRVテーブル（頂点, t1）
    // 5 = カメラベクトルCBV（頂点, b2）  ← ビルボード用
    // 6 = カメラCBV（ピクセル, b3）     ← スペキュラ用

    // 注意: 互換性のため既存のインデックスを維持する: 0=Material CBV(Pixel), 1=WVP CBV(Vertex), 2=Texture SRV Table(Pixel), 3=Light CBV(Pixel)
    // 既存コードのインデックスを変更せずに済むよう、インスタンシングSRVテーブルはインデックス4（頂点）に追加する。
    D3D12_ROOT_PARAMETER rootParameters[11] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う (PixelShader, レジスタ0: マテリアルCBV)
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う (VertexShader, レジスタ0: WVP行列CBV)
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 0;
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う (PixelShader, レジスタ0: テクスチャSRV)
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う (PixelShader, レジスタ1: 光源CBV)
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    // インスタンシング用SRVテーブル (VertexShader, レジスタ1: インスタンシングSRV)
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[4].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
    rootParameters[4].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

    // カメラベクトルCBV (Vertex shader, b2) - ビルボード用
    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[5].Descriptor.ShaderRegister = 2; // b2

    // カメラCBV (Pixel shader, b3) - スペキュラ計算用
    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[6].Descriptor.ShaderRegister = 3; // b3

    // ポイントライトCBV (Pixel shader, b4)
    rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[7].Descriptor.ShaderRegister = 4; // b4

    // スポットライトCBV (Pixel shader, b5)
    rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[8].Descriptor.ShaderRegister = 5; // b5

    rootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[9].DescriptorTable.pDescriptorRanges = descriptorRangeForEnvironment;
    rootParameters[9].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForEnvironment);

    // Skinning用Palette SRV (Vertex shader, t3)
    rootParameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[10].DescriptorTable.pDescriptorRanges = descriptorRangeForSkinning;
    rootParameters[10].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForSkinning);

    // 注: 平行光源CBVは Object3dCommon に保存されたGPUアドレスを使ってオブジェクト毎にバインドされる

    /// ルートシグネチャの説明
    descriptionRootSignature.pParameters = rootParameters; // ルートパラメーター配列へのポインタ
    descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

    // スタティックサンプラーの設定
    // スタティックサンプラーを2つ用意する:
    //  - s0: 線形フィルタ、ラップアドレッシング（ほとんどのモデルの既定）
    //  - s1: ポイントフィルタ、クランプアドレッシング（フェンスのようなアルファカットアウト用テクスチャ向け）
    D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};

    // s0: 線形 + ラップ
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 線形フィルタ
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // U方向はラップ
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // V方向はラップ
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // W方向はラップ
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較は使わないのでNEVER
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ミップレベルの最大値は無限大（利用可能な最大ミップレベルまで使用）
    staticSamplers[0].MipLODBias = kSamplerMipLodBias; // ミップレベルのバイアスはなし
    staticSamplers[0].MinLOD = kSamplerMinLod; // ミップレベルの最小値は0（最も高解像度のミップレベルから使用）
    staticSamplers[0].ShaderRegister = kLinearWrapSamplerRegister; // シェーダーで s0 にバインドされる
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用

    // s1: ポイントフィルタ + クランプ（アルファカットアウトテクスチャのブリーディングを防ぐために有用）
    staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; // ポイントフィルタ
    staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // U方向はクランプ
    staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // V方向はクランプ
    staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // W方向はクランプ
    staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較は使わないのでNEVER
    staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX; // ミップレベルの最大値は無限大（利用可能な最大ミップレベルまで使用）
    staticSamplers[1].MipLODBias = kSamplerMipLodBias; // ミップレベルのバイアスはなし
    staticSamplers[1].MinLOD = kSamplerMinLod; // ミップレベルの最小値は0（最も高解像度のミップレベルから使用）
    staticSamplers[1].ShaderRegister = kPointClampSamplerRegister; // シェーダーで s1 にバインドされる
    staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用

    // ルートシグネチャの説明にスタティックサンプラーをセット
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    // スタティックサンプラーの数
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    // シリアライズしてバイナリにする
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    // ルートシグネチャの説明をシリアライズして、GPUに渡すためのバイナリを生成する
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        // エラー処理
        Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }

    // バイナリをもとに生成し、メンバ変数に保持する
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

/// <summary>
/// 点光源の追加
/// </summary>
int Object3dCommon::AddPointLight(const Object3d::PointLight& pl)
{

    // 点光源用の定数バッファがない場合は追加できないため、-1を返して失敗を示す
    if (!pointLightsData_) {
        return -1;
    }

    // 点光源の配列を走査して、enabled == 0 の最初のスロットに新しい点光源を追加する
    for (uint32_t i = 0; i < kMaxPointLights; ++i) {
        if (pointLightsData_[i].enabled == 0) {
            pointLightsData_[i] = pl;
            pointLightsData_[i].enabled = 1;
            return static_cast<int>(i);
        }
    }

    // 空きスロットがない場合は追加できないため、-1を返して失敗を示す
    return -1;
}

/// <summary>
/// 点光源の削除
/// </summary>
bool Object3dCommon::RemovePointLight(int index)
{
    // 点光源用の定数バッファがない場合は削除できないため、falseを返して失敗を示す
    if (!pointLightsData_) {
        return false;
    }

    // 指定されたインデックスが有効な範囲内にない場合も削除できないため、falseを返して失敗を示す
    if (index < 0 || static_cast<uint32_t>(index) >= kMaxPointLights) {
        return false;
    }

    // 指定されたインデックスの点光源を無効化するため、enabled を 0 に設定する
    pointLightsData_[index].enabled = 0;

    return true; // これで点光源は削除された（無効化された）とみなされる
}

/// <summary>
/// 点光源の更新
/// </summary>
bool Object3dCommon::UpdatePointLight(int index, const Object3d::PointLight& pl)
{
    // 点光源用の定数バッファがない場合は更新できないため、falseを返して失敗を示す
    if (!pointLightsData_) {
        return false;
    }

    // 指定されたインデックスが有効な範囲内にない場合も更新できないため、falseを返して失敗を示す
    if (index < 0 || static_cast<uint32_t>(index) >= kMaxPointLights) {
        return false;
    }

    // 指定されたインデックスの点光源を更新する。enabled フラグは引数の pl の値に従う。
    pointLightsData_[index] = pl; // pl の内容で上書きする
    pointLightsData_[index].enabled = pl.enabled ? 1 : 0; // enabled フラグは pl の値に従う（0以外は有効とみなす）

    return true; // これで点光源は更新された
}

/// <summary>
/// グラフィックスパイプラインの作成
/// </summary>
void Object3dCommon::CreateGraphicsPipeline()
{
    CreateGraphicsPipeline(blendMode_);
}

/// <summary>
/// 指定されたブレンドモード用のグラフィックスパイプラインを作成する
/// </summary>
void Object3dCommon::CreateGraphicsPipeline(BlendMode mode)
{

    HRESULT hr;


    // InputLayout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    // InputLayoutの説明
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc {};
    inputLayoutDesc.pInputElementDescs = inputElementDescs; // 入力要素の配列へのポインタ
    inputLayoutDesc.NumElements = _countof(inputElementDescs); // 入力要素の数

    D3D12_INPUT_ELEMENT_DESC skinningInputElementDescs[5] = {};
    skinningInputElementDescs[0] = inputElementDescs[0];
    skinningInputElementDescs[1] = inputElementDescs[1];
    skinningInputElementDescs[2] = inputElementDescs[2];
    skinningInputElementDescs[3].SemanticName = "BLENDWEIGHT";
    skinningInputElementDescs[3].SemanticIndex = 0;
    skinningInputElementDescs[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    skinningInputElementDescs[3].InputSlot = 1;
    skinningInputElementDescs[3].AlignedByteOffset = 0;
    skinningInputElementDescs[4].SemanticName = "BLENDINDICES";
    skinningInputElementDescs[4].SemanticIndex = 0;
    skinningInputElementDescs[4].Format = DXGI_FORMAT_R32G32B32A32_SINT;
    skinningInputElementDescs[4].InputSlot = 1;
    skinningInputElementDescs[4].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC skinningInputLayoutDesc {};
    skinningInputLayoutDesc.pInputElementDescs = skinningInputElementDescs;
    skinningInputLayoutDesc.NumElements = _countof(skinningInputElementDescs);

    // BlendState の設定を blendMode_ に応じて切り替える
    D3D12_BLEND_DESC blendDesc {};
    D3D12_RENDER_TARGET_BLEND_DESC& rtBlend = blendDesc.RenderTarget[0]; // 1つ目のレンダーターゲットのブレンド設定を取得
    rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // RGBA全てのチャンネルに書き込む
    rtBlend.LogicOpEnable = FALSE; // ロジックオペレーションは使わない
    rtBlend.BlendOp = D3D12_BLEND_OP_ADD; // ブレンドオペレーションは加算（src + dest）を基本とする
    rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD; // アルファブレンドオペレーションも加算を基本とする

    // ブレンドモードに応じて、ソースブレンドとデスティネーションブレンドを設定する
    switch (mode) {
    case BlendMode::None: // ブレンドなし（上書き）

        // ブレンドを無効にして、ソースがそのまま出力されるようにする
        rtBlend.BlendEnable = FALSE;
        rtBlend.SrcBlend = D3D12_BLEND_ONE;
        rtBlend.DestBlend = D3D12_BLEND_ZERO;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        break;

    case BlendMode::Alpha: // アルファブレンド（通常の半透明表現）

        // ブレンドを有効にして、ソースのアルファ値に基づいてブレンドする
        rtBlend.BlendEnable = TRUE;
        rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        break;

    case BlendMode::Add: // 加算ブレンド（発光表現などに有用）

        // ブレンドを有効にして、ソースの色をそのまま加算する
        rtBlend.BlendEnable = TRUE;
        rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rtBlend.DestBlend = D3D12_BLEND_ONE;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ONE;
        break;

    case BlendMode::Subtract: // 減算

        // ブレンドを有効にして、ソースの色を減算させる
        rtBlend.BlendEnable = TRUE;
        rtBlend.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
        rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rtBlend.DestBlend = D3D12_BLEND_ONE;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ONE;

        break;

    case BlendMode::Multiply: // 乗算ブレンド（影や暗い部分の表現に有用）

        // ブレンドを有効にして、ソースの色とデスティネーションの色を乗算する
        rtBlend.BlendEnable = TRUE;
        rtBlend.SrcBlend = D3D12_BLEND_DEST_COLOR;
        rtBlend.DestBlend = D3D12_BLEND_ZERO;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        break;

    case BlendMode::Screen: // スクリーンブレンド（明るい部分の表現に有用）

        // ブレンドを有効にして、ソースの色を反転してデスティネーションの色と乗算し、さらに反転する
        rtBlend.BlendEnable = TRUE;
        rtBlend.SrcBlend = D3D12_BLEND_ONE;
        rtBlend.DestBlend = D3D12_BLEND_INV_SRC_COLOR;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        break;

    default:

        // デフォルトはアルファブレンド
        rtBlend.BlendEnable = TRUE;
        rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
        break;
    }

    // RasterizerStateの設定
    D3D12_RASTERIZER_DESC rasterizerDesc {};
    // Effect用Primitiveの裏側も見えるようにカリングしない
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    // 三角形の中を塗りつぶす
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    // デフォルトのワインディング（時計回りを前面）を使用し、多くのOBJエクスポートと整合させる
    // もしモデルの一部が裏返しに見える場合は、ここでこのフラグを切り替えるよりも
    // カリングを無効にするか OBJ ローダー側でワインディングを修正することを検討してください。
    rasterizerDesc.FrontCounterClockwise = FALSE; // デフォルトのワインディングは時計回りが前面
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS; // 深度バイアスはデフォルト値を使用
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP; // 深度バイアスクランプはデフォルト値を使用
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS; // スロープスケーリング深度バイアスはデフォルト値を使用
    rasterizerDesc.DepthClipEnable = TRUE; // 深度クリッピングを有効にする
    rasterizerDesc.MultisampleEnable = FALSE; // マルチサンプルは使用しない

    // シェーダーをコンパイルする
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Object3D.VS.hlsl", L"vs_6_0");
    assert(vertexShaderBlob != nullptr); // PSOの生成に失敗する可能性があるため、シェーダーのコンパイルに失敗した場合はアサートで止める

    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Object3D.PS.hlsl", L"ps_6_0");
    assert(pixelShaderBlob != nullptr); // PSOの生成に失敗する可能性があるため、シェーダーのコンパイルに失敗した場合はアサートで止める

    // PSOを生成する
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc {};
    graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get(); // メンバ変数のルートシグネチャを使用
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() }; // VertexShader
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() }; // PixelShader
    graphicsPipelineStateDesc.BlendState = blendDesc; // BlendState
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState
    // 書き込むRTVの情報
    graphicsPipelineStateDesc.NumRenderTargets = kObjectRenderTargetCount;
    graphicsPipelineStateDesc.RTVFormats[0] = dxCommon_->GetSwapChainFormat();
    // 利用するトポロジ（形状）のタイプ。三角形
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // どのように画面に色を打ち込むかの設定（気にしなくて良い）
    graphicsPipelineStateDesc.SampleDesc.Count = kObjectSampleCount;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // DepthStencilStateの設定
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    // Depthの機能を有効化する
    depthStencilDesc.DepthEnable = true;
    // Depth書き込みマスク: 透明表現（例: Alpha）を使用するブレンドモードでは不正なオクルージョンを防ぐため深度書き込みを無効化。
    // 不透明（None）のレンダリングでは深度書き込みを有効化する。
    if (mode == BlendMode::Alpha || mode == BlendMode::Add || mode == BlendMode::Subtract || mode == BlendMode::Multiply || mode == BlendMode::Screen) {
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    } else {
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    }
    // 比較関数はLessEqual。つまり、近ければ描画される
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // DepthStencilの設定
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // 実際に生成し、メンバ変数に保持する
    const size_t modeIndex = ToObjectBlendModeIndex(mode); // 作成したPSOを格納する添字
    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStates_[modeIndex]));
    assert(SUCCEEDED(hr));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC skeletonDebugPipelineStateDesc = graphicsPipelineStateDesc;
    skeletonDebugPipelineStateDesc.DepthStencilState.DepthEnable = false;
    skeletonDebugPipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&skeletonDebugPipelineStateDesc, IID_PPV_ARGS(&skeletonDebugPipelineStates_[modeIndex]));
    assert(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IDxcBlob> skinningVertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/SkinningObject3D.VS.hlsl", L"vs_6_0");
    assert(skinningVertexShaderBlob != nullptr);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinningPipelineStateDesc = graphicsPipelineStateDesc;
    skinningPipelineStateDesc.InputLayout = skinningInputLayoutDesc;
    skinningPipelineStateDesc.VS = { skinningVertexShaderBlob->GetBufferPointer(), skinningVertexShaderBlob->GetBufferSize() };
    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&skinningPipelineStateDesc, IID_PPV_ARGS(&skinningPipelineStates_[modeIndex]));
    assert(SUCCEEDED(hr));

    if (instancingPipelineState_) {
        return;
    }

    // Particleシェーダーを用いて、インスタンシング／パーティクル描画用の別PSOを作成
    Microsoft::WRL::ComPtr<IDxcBlob> instVS = dxCommon_->CompileShader(L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> instPS = dxCommon_->CompileShader(L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");
    // インスタンシング用のシェーダーが両方ともコンパイルできた場合にのみ、インスタンシング用PSOを生成する
    if (instVS && instPS) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC instDesc = {};
        instDesc.pRootSignature = rootSignature_.Get(); // ルートシグネチャは共通
        instDesc.InputLayout = inputLayoutDesc; // InputLayoutも共通
        instDesc.VS = { instVS->GetBufferPointer(), instVS->GetBufferSize() }; // インスタンシング用VS
        instDesc.PS = { instPS->GetBufferPointer(), instPS->GetBufferSize() }; // インスタンシング用PS
        D3D12_BLEND_DESC particleBlendDesc {}; // パーティクル専用の加算ブレンド設定
        D3D12_RENDER_TARGET_BLEND_DESC& particleRenderTargetBlend =
            particleBlendDesc.RenderTarget[0]; // パーティクル用のレンダーターゲット設定
        particleRenderTargetBlend.BlendEnable = TRUE;
        particleRenderTargetBlend.LogicOpEnable = FALSE;
        particleRenderTargetBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        particleRenderTargetBlend.DestBlend = D3D12_BLEND_ONE;
        particleRenderTargetBlend.BlendOp = D3D12_BLEND_OP_ADD;
        particleRenderTargetBlend.SrcBlendAlpha = D3D12_BLEND_ZERO;
        particleRenderTargetBlend.DestBlendAlpha = D3D12_BLEND_ONE;
        particleRenderTargetBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        particleRenderTargetBlend.LogicOp = D3D12_LOGIC_OP_NOOP;
        particleRenderTargetBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        instDesc.BlendState = particleBlendDesc;
        instDesc.RasterizerState = rasterizerDesc; // ラスタライザーステートも共通
        instDesc.NumRenderTargets = kObjectRenderTargetCount; // 書き込むRTVの情報
        instDesc.RTVFormats[0] = dxCommon_->GetSwapChainFormat(); // 書き込むRTVの情報
        instDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // 利用するトポロジ（形状）のタイプ。三角形
        instDesc.SampleDesc.Count = kObjectSampleCount; // どのように画面に色を打ち込むかの設定（気にしなくて良い）
        instDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // ここまでは共通の設定
        D3D12_DEPTH_STENCIL_DESC particleDepthStencilDesc = depthStencilDesc; // パーティクル専用の深度設定
        particleDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        instDesc.DepthStencilState = particleDepthStencilDesc;
        instDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilのフォーマットも共通
        // インスタンシング用PSOを生成し、メンバ変数に保持する
        HRESULT r = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&instDesc, IID_PPV_ARGS(&instancingPipelineState_));
        // インスタンシング用PSOの生成に失敗した場合は、エラーログを出力して、インスタンシング用PSOをリセットする
        if (FAILED(r)) {
            char buf[kObjectLogBufferSize];
            sprintf_s(buf, "Object3dCommon::CreateGraphicsPipeline: failed to create instancing PSO hr=0x%08X\n", static_cast<unsigned int>(r));
            Logger::Log(buf);
            instancingPipelineState_.Reset();
        }
    } else {
        // インスタンシング用のシェーダーが見つからなかった場合は、エラーログを出力して、インスタンシング用PSOをリセットする
        Logger::Log("Object3dCommon::CreateGraphicsPipeline: Particleシェーダーが見つからなかったため、インスタンシング用PSOは作成されませんでした\n");
    }
}

/// <summary>
/// 現在のフレームで使用する平行光源のGPUアドレスを取得する
/// </summary>
D3D12_GPU_VIRTUAL_ADDRESS Object3dCommon::GetDirectionalLightGPUAddress() const
{
    const uint32_t frameIndex = dxCommon_ ? dxCommon_->GetCurrentFrameIndex() : 0;
    return directionalLightResources_[frameIndex] ? directionalLightResources_[frameIndex]->GetGPUVirtualAddress() : 0;
}

/// <summary>
/// 現在のフレームで使用する点光源のGPUアドレスを取得する
/// </summary>
D3D12_GPU_VIRTUAL_ADDRESS Object3dCommon::GetPointLightsGPUAddress() const
{
    const uint32_t frameIndex = dxCommon_ ? dxCommon_->GetCurrentFrameIndex() : 0;
    return pointLightsResources_[frameIndex] ? pointLightsResources_[frameIndex]->GetGPUVirtualAddress() : 0;
}

/// <summary>
/// 現在のフレームで使用するスポットライトのGPUアドレスを取得する
/// </summary>
D3D12_GPU_VIRTUAL_ADDRESS Object3dCommon::GetSpotLightGPUAddress() const
{
    const uint32_t frameIndex = dxCommon_ ? dxCommon_->GetCurrentFrameIndex() : 0;
    return spotLightResources_[frameIndex] ? spotLightResources_[frameIndex]->GetGPUVirtualAddress() : 0;
}

/// <summary>
/// 現在のフレームで使用するカメラのGPUアドレスを取得する
/// </summary>
D3D12_GPU_VIRTUAL_ADDRESS Object3dCommon::GetCameraGPUAddress() const
{
    const uint32_t frameIndex = dxCommon_ ? dxCommon_->GetCurrentFrameIndex() : 0;
    return cameraResources_[frameIndex] ? cameraResources_[frameIndex]->GetGPUVirtualAddress() : 0;
}

/// <summary>
/// 現在のフレームで使用するインスタンシングデータを取得する
/// </summary>
Object3d::TransformationMatrix* Object3dCommon::GetInstancingData() const
{
    const uint32_t frameIndex = dxCommon_ ? dxCommon_->GetCurrentFrameIndex() : 0;
    return instancingData_[frameIndex];
}

/// <summary>
/// 現在のフレームで使用するインスタンシングSRVを取得する
/// </summary>
D3D12_GPU_DESCRIPTOR_HANDLE Object3dCommon::GetInstancingSrvGPUHandle() const
{
    if (instancingSrvOverrideGPU_.ptr != 0) {
        return instancingSrvOverrideGPU_;
    }

    const uint32_t frameIndex = dxCommon_ ? dxCommon_->GetCurrentFrameIndex() : 0;
    return instancingSrvHandlesGPU_[frameIndex];
}

