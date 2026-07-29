#include "ParticleManager.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/utility/Logger.h"
#include "externals/DirectXTex/d3dx12.h"
#include <algorithm>

using namespace Math;
using namespace MyEngine;

namespace {
constexpr uint32_t kGpuParticleThreadCount = 1024; // ComputeShaderの1グループあたりのスレッド数
constexpr uint32_t kGpuEmitterThreadCount = 256; // GPU Emitter発生CSの1グループあたりのスレッド数
constexpr uint32_t kComputeRootParameterSourceSrv = 0; // Compute入力SRVのルート番号
constexpr uint32_t kComputeRootParameterOutputUav = 1; // Compute出力UAVのルート番号
constexpr uint32_t kComputeRootParameterInfoCbv = 2; // Compute定数バッファのルート番号
constexpr uint32_t kComputeRootParameterEmitterCbv = 3; // GPU Emitter用CBVのルート番号
constexpr uint32_t kComputeRootParameterPerFrameCbv = 4; // GPU Emitter用フレームCBVのルート番号
constexpr uint32_t kComputeRootParameterFreeCounterUav = 5; // GPU Emitter用FreeListIndex UAVのルート番号
constexpr uint32_t kComputeRootParameterFreeListUav = 6; // GPU Emitter用FreeList UAVのルート番号
} // namespace

/// <summary>
/// GPUパーティクル変換に必要なリソースを作成する。
/// </summary>
void ParticleManager::InitializeGpuParticleResources()
{
    gpuParticleReady_ = false;
    if (!dxCommon_ || !srvManager_) {
        return;
    }

    ID3D12Device* device = dxCommon_->GetDevice(); // GPUリソースを作成するD3D12デバイス
    if (!device) {
        return;
    }

    D3D12_DESCRIPTOR_RANGE sourceSrvRange {}; // CPU Particle入力SRVのDescriptorTable範囲
    sourceSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    sourceSrvRange.NumDescriptors = 1;
    sourceSrvRange.BaseShaderRegister = 0;
    sourceSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE outputUavRange {}; // GPU Particle出力UAVのDescriptorTable範囲
    outputUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    outputUavRange.NumDescriptors = 1;
    outputUavRange.BaseShaderRegister = 0;
    outputUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeCounterUavRange {}; // FreeListIndex UAVのDescriptorTable範囲
    freeCounterUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeCounterUavRange.NumDescriptors = 1;
    freeCounterUavRange.BaseShaderRegister = 1;
    freeCounterUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListUavRange {}; // FreeList UAVのDescriptorTable範囲
    freeListUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListUavRange.NumDescriptors = 1;
    freeListUavRange.BaseShaderRegister = 2;
    freeListUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[7] {}; // ComputeShaderへ渡すRootParameter一覧
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

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc {}; // ComputeShader用RootSignature設定
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob; // 生成したRootSignatureバイナリ
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob; // RootSignature生成エラー情報
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
    Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(L"resources/shaders/GPUParticleUpdate.CS.hlsl", L"cs_6_0"); // CPU Particle変換用CS
    if (!computeShaderBlob) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to compile GPUParticleUpdate.CS.hlsl.\n");
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc {}; // CPU Particle変換用PSO設定
    pipelineDesc.pRootSignature = computeRootSignature_.Get();
    pipelineDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };
    hr = device->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&computePipelineState_));
    if (FAILED(hr)) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to create compute pipeline state.\n");
        return;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> initializeShaderBlob = dxCommon_->CompileShader(L"resources/shaders/InitializeParticle.CS.hlsl", L"cs_6_0"); // GPU Particle初期化用CS
    if (!initializeShaderBlob) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to compile InitializeParticle.CS.hlsl.\n");
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC initializePipelineDesc {}; // GPU Particle初期化用PSO設定
    initializePipelineDesc.pRootSignature = computeRootSignature_.Get();
    initializePipelineDesc.CS = { initializeShaderBlob->GetBufferPointer(), initializeShaderBlob->GetBufferSize() };
    hr = device->CreateComputePipelineState(&initializePipelineDesc, IID_PPV_ARGS(&initializeParticlePipelineState_));
    if (FAILED(hr)) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to create initialize particle pipeline state.\n");
        return;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> emitShaderBlob = dxCommon_->CompileShader(L"resources/shaders/EmitParticle.CS.hlsl", L"cs_6_0"); // GPU Emitter発生用CS
    if (!emitShaderBlob) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to compile EmitParticle.CS.hlsl.\n");
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC emitPipelineDesc {}; // GPU Emitter発生用PSO設定
    emitPipelineDesc.pRootSignature = computeRootSignature_.Get();
    emitPipelineDesc.CS = { emitShaderBlob->GetBufferPointer(), emitShaderBlob->GetBufferSize() };
    hr = device->CreateComputePipelineState(&emitPipelineDesc, IID_PPV_ARGS(&emitParticlePipelineState_));
    if (FAILED(hr)) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to create emit particle pipeline state.\n");
        return;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> updateShaderBlob = dxCommon_->CompileShader(L"resources/shaders/UpdateParticle.CS.hlsl", L"cs_6_0"); // GPU Particle更新用CS
    if (!updateShaderBlob) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to compile UpdateParticle.CS.hlsl.\n");
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC updatePipelineDesc {}; // GPU Particle更新用PSO設定
    updatePipelineDesc.pRootSignature = computeRootSignature_.Get();
    updatePipelineDesc.CS = { updateShaderBlob->GetBufferPointer(), updateShaderBlob->GetBufferSize() };
    hr = device->CreateComputePipelineState(&updatePipelineDesc, IID_PPV_ARGS(&updateParticlePipelineState_));
    if (FAILED(hr)) {
        Logger::Warn("ParticleManager::InitializeGpuParticleResources: failed to create update particle pipeline state.\n");
        return;
    }

    const uint32_t particleLimit = GetParticleLimit(); // GPU Particleの最大数
    const size_t sourceBufferSize = sizeof(PM_GpuParticleSource) * particleLimit; // CPU入力バッファサイズ
    const size_t infoBufferSize = (sizeof(PM_GpuParticleTransformInfo) + 0xff) & ~static_cast<size_t>(0xff); // 変換情報CBVサイズ
    const size_t outputBufferSize = sizeof(PM_GpuParticleSource) * particleLimit; // GPU Particle出力バッファサイズ
    const size_t emitterBufferSize = (sizeof(PM_GpuEmitterSphere) + 0xff) & ~static_cast<size_t>(0xff); // Emitter CBVサイズ
    const size_t perFrameBufferSize = (sizeof(PM_GpuPerFrame) + 0xff) & ~static_cast<size_t>(0xff); // フレームCBVサイズ
    const size_t freeCounterBufferSize = sizeof(int32_t); // FreeListIndexバッファサイズ
    const size_t freeListBufferSize = sizeof(uint32_t) * particleLimit; // FreeListバッファサイズ

    D3D12_SHADER_RESOURCE_VIEW_DESC sourceSrvDesc {}; // CPU入力バッファSRV設定
    sourceSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    sourceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    sourceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sourceSrvDesc.Buffer.NumElements = particleLimit;
    sourceSrvDesc.Buffer.StructureByteStride = sizeof(PM_GpuParticleSource);
    sourceSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    D3D12_SHADER_RESOURCE_VIEW_DESC outputSrvDesc {}; // GPU Particle描画用SRV設定
    outputSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    outputSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    outputSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    outputSrvDesc.Buffer.NumElements = particleLimit;
    outputSrvDesc.Buffer.StructureByteStride = sizeof(PM_GpuParticleSource);
    outputSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    D3D12_UNORDERED_ACCESS_VIEW_DESC outputUavDesc {}; // GPU Particle更新用UAV設定
    outputUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    outputUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    outputUavDesc.Buffer.NumElements = particleLimit;
    outputUavDesc.Buffer.StructureByteStride = sizeof(PM_GpuParticleSource);
    outputUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

    D3D12_UNORDERED_ACCESS_VIEW_DESC freeCounterUavDesc {}; // FreeListIndex用UAV設定
    freeCounterUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    freeCounterUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    freeCounterUavDesc.Buffer.NumElements = 1;
    freeCounterUavDesc.Buffer.StructureByteStride = sizeof(int32_t);
    freeCounterUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

    D3D12_UNORDERED_ACCESS_VIEW_DESC freeListUavDesc {}; // FreeList用UAV設定
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

        D3D12_HEAP_PROPERTIES defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT); // Default Heap設定
        D3D12_RESOURCE_DESC outputResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(outputBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS); // GPU Particle出力バッファ設定
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

        D3D12_RESOURCE_DESC freeCounterResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(freeCounterBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS); // FreeListIndexバッファ設定
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

        D3D12_HEAP_PROPERTIES readbackHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK); // Readback Heap設定
        D3D12_RESOURCE_DESC freeCounterReadbackDesc = CD3DX12_RESOURCE_DESC::Buffer(freeCounterBufferSize); // FreeListIndex Readbackバッファ設定
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

        D3D12_RESOURCE_DESC freeListResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(freeListBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS); // FreeListバッファ設定
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
/// GPU参照が終わるまでD3D12リソースの解放を遅延する。
/// </summary>
void ParticleManager::DeferReleaseResource(Microsoft::WRL::ComPtr<ID3D12Resource>& resource)
{
    if (!resource) {
        return;
    }

    if (dxCommon_) {
        dxCommon_->DeferReleaseResource(resource);
        return;
    }

    resource.Reset();
}

/// <summary>
/// 指定フレームのGPU Particle用リソースを解放予約する。
/// </summary>
void ParticleManager::ReleaseGpuParticleFrameResources(uint32_t frameIndex)
{
    if (frameIndex >= DirectXCommon::kFrameCount) {
        return;
    }

    if (gpuParticleSourceResources_[frameIndex] && gpuParticleSourceData_[frameIndex]) {
        gpuParticleSourceResources_[frameIndex]->Unmap(0, nullptr);
    }
    gpuParticleSourceData_[frameIndex] = nullptr;
    DeferReleaseResource(gpuParticleSourceResources_[frameIndex]);

    if (gpuParticleInfoResources_[frameIndex] && gpuParticleInfoData_[frameIndex]) {
        gpuParticleInfoResources_[frameIndex]->Unmap(0, nullptr);
    }
    gpuParticleInfoData_[frameIndex] = nullptr;
    DeferReleaseResource(gpuParticleInfoResources_[frameIndex]);

    if (gpuEmitterResources_[frameIndex] && gpuEmitterData_[frameIndex]) {
        gpuEmitterResources_[frameIndex]->Unmap(0, nullptr);
    }
    gpuEmitterData_[frameIndex] = nullptr;
    DeferReleaseResource(gpuEmitterResources_[frameIndex]);

    if (gpuPerFrameResources_[frameIndex] && gpuPerFrameData_[frameIndex]) {
        gpuPerFrameResources_[frameIndex]->Unmap(0, nullptr);
    }
    gpuPerFrameData_[frameIndex] = nullptr;
    DeferReleaseResource(gpuPerFrameResources_[frameIndex]);

    DeferReleaseResource(gpuParticleOutputResources_[frameIndex]);
    DeferReleaseResource(gpuFreeCounterResources_[frameIndex]);

    if (gpuFreeCounterReadbackResources_[frameIndex] && gpuFreeCounterReadbackData_[frameIndex]) {
        gpuFreeCounterReadbackResources_[frameIndex]->Unmap(0, nullptr);
    }
    gpuFreeCounterReadbackData_[frameIndex] = nullptr;
    DeferReleaseResource(gpuFreeCounterReadbackResources_[frameIndex]);

    DeferReleaseResource(gpuFreeListResources_[frameIndex]);
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
        ReleaseGpuParticleFrameResources(frameIndex);
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

        const int32_t freeListIndex = *counterData; // GPU側の未使用末尾位置
        const int32_t estimatedAlive = static_cast<int32_t>(particleLimit) - 1 - freeListIndex; // FreeListIndexから推定した生存数
        if (estimatedAlive > 0) {
            aliveCount = (std::max)(aliveCount, static_cast<uint32_t>((std::min)(estimatedAlive, static_cast<int32_t>(particleLimit))));
        }
    }

    gpuAliveCountEstimate_ = aliveCount;
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

    const uint32_t particleLimit = GetParticleLimit(); // GPUへ転送できる最大数
    const uint32_t uploadCount = (std::min)(count, particleLimit); // 実際に転送するParticle数
    for (uint32_t particleIndex = 0; particleIndex < uploadCount; ++particleIndex) {
        const PM_CpuParticle& particle = group.particles[particleIndex]; // 転送元のCPU Particle
        PM_GpuParticleSource& gpuParticle = gpuParticleSourceData_[frameIndex][particleIndex]; // 転送先のGPU入力Particle
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

    PM_GpuParticleTransformInfo& info = *gpuParticleInfoData_[frameIndex]; // ComputeShaderへ渡す変換情報
    info.particleCount = uploadCount;
    info.view = view;
    info.projection = projection;

    return uploadCount;
}
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

    D3D12_RESOURCE_STATES& counterState = gpuFreeCounterStates_[frameIndex]; // FreeListIndex Resourceの現在状態
    if (counterState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER counterBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            gpuFreeCounterResources_[frameIndex].Get(),
            counterState,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &counterBarrier);
        counterState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    D3D12_RESOURCE_STATES& freeListState = gpuFreeListStates_[frameIndex]; // FreeList Resourceの現在状態
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
    }; // Dispatch後にUAV書き込みを確定するBarrier
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
    if (!gpuParticleReady_ || !dxCommon_ || !srvManager_ || gpuEmitterState_.emit == 0 || gpuEmitterState_.count == 0) {
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

    D3D12_RESOURCE_STATES& counterState = gpuFreeCounterStates_[frameIndex]; // FreeListIndex Resourceの現在状態
    if (counterState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER counterBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            gpuFreeCounterResources_[frameIndex].Get(),
            counterState,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &counterBarrier);
        counterState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    D3D12_RESOURCE_STATES& freeListState = gpuFreeListStates_[frameIndex]; // FreeList Resourceの現在状態
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
    }; // Dispatch後にUAV書き込みを確定するBarrier
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

    D3D12_RESOURCE_STATES& counterState = gpuFreeCounterStates_[frameIndex]; // FreeListIndex Resourceの現在状態
    if (counterState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER counterBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            gpuFreeCounterResources_[frameIndex].Get(),
            counterState,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &counterBarrier);
        counterState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    D3D12_RESOURCE_STATES& freeListState = gpuFreeListStates_[frameIndex]; // FreeList Resourceの現在状態
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
    }; // Dispatch後にUAV書き込みを確定するBarrier
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
/// ComputeShaderでパーティクルをインスタンシング用行列へ変換する。
/// </summary>
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

    const uint32_t dispatchGroupCount = (count + kGpuParticleThreadCount - 1) / kGpuParticleThreadCount; // 変換対象のDispatch数
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