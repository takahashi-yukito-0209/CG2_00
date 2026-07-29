#pragma once

#include "PostProcess.h"

#include <array>
#include <cstddef>
#include <d3d12.h>
#include <wrl.h>

namespace MyEngine {

class DirectXCommon;

/// <summary>
/// ポストエフェクト用RootSignatureとPSOを管理するクラス
/// </summary>
class PostProcessPipeline {
public:
    /// <summary>
    /// ポストエフェクト用RootSignatureとPSOを生成する
    /// </summary>
    bool Initialize(DirectXCommon* dxCommon);

    /// <summary>
    /// 保持しているRootSignatureとPSOを解放する
    /// </summary>
    void Finalize();

    /// <summary>
    /// 描画に必要なRootSignatureとPSOが揃っているか確認する
    /// </summary>
    bool IsReady() const;

    /// <summary>
    /// 指定したポストエフェクトのPSOが生成済みか確認する
    /// </summary>
    bool IsEffectReady(PostEffectType effectType) const;

    /// <summary>
    /// 全画面描画用RootSignatureを取得する
    /// </summary>
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

    /// <summary>
    /// 指定したポストエフェクトに対応するPSOを取得する
    /// </summary>
    ID3D12PipelineState* GetPipelineState(PostEffectType effectType) const;

private:
    using PipelineState = Microsoft::WRL::ComPtr<ID3D12PipelineState>; // ポストエフェクト用PSO

    /// <summary>
    /// 全画面描画用RootSignatureを生成する
    /// </summary>
    void CreateRootSignature();

    /// <summary>
    /// 指定したピクセルシェーダーからPSOを生成する
    /// </summary>
    PipelineState CreatePipelineState(const wchar_t* pixelShaderPath);

    /// <summary>
    /// 指定したポストエフェクトに対応するPSO格納先を取得する
    /// </summary>
    PipelineState* GetPipelineStateStorage(PostEffectType effectType);

private:
    DirectXCommon* dxCommon_ = nullptr; // GPUリソース生成に使用するDirectX共通基盤
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_; // 全画面描画用RootSignature
    std::array<PipelineState, GetPostEffectTypeCount()> pipelineStates_; // エフェクト種別ごとのPSO
};

} // namespace MyEngine