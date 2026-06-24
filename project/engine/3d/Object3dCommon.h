#pragma once
#include "../RenderState.h"
#include "DirectXCommon.h"
#include "Object3d.h"
#include <d3d12.h>
#include <wrl.h>

// 前方宣言
class DebugCamera;

namespace MyEngine {

class SrvManager;

/// <summary>
/// Object3d クラスで共有される DirectX リソースや描画設定を管理するクラス
/// </summary>
class Object3dCommon {
public: // メンバ関数
    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    /// <summary>
    /// インスタンシング用SRVの割り当てを解放する
    /// </summary>
    void Finalize();

    // 最大点光源数（スポットライトも点光源としてカウントする）
    static const uint32_t kMaxPointLights = 1;

    /// <summary>
    /// 平行光源のデータ構造体のポインタを取得
    /// </summary>
    Object3d::DirectionalLight* GetDirectionalLightData() { return directionalLightData_; }

    /// <summary>
    /// 点光源のデータ構造体のポインタを取得
    /// </summary>
    D3D12_GPU_VIRTUAL_ADDRESS GetDirectionalLightGPUAddress() const;

    /// <summary>
    /// 共通描画設定をコマンドリストに設定
    /// </summary>
    void SetCommonDrawSetting();

    /// <summary>
    /// オブジェクト反射に使う環境マップのSRVインデックスを設定
    /// </summary>
    void SetEnvironmentMapSrvIndex(uint32_t srvIndex);

    /// <summary>
    /// インスタンシング／パーティクル用の描画設定をコマンドリストに設定
    /// </summary>
    void SetInstancingDrawSetting();

    /// <summary>
    /// ビルボード描画用のカメラベクトルを設定
    /// </summary>
    void SetBillboardCamera(const Math::Vector3& right, const Math::Vector3& up, bool enable)
    {

        // カメラ定数バッファが存在しない場合は設定をスキップ
        if (!cameraCBData_) {
            return;
        }

        // カメラベクトルと有効フラグを定数バッファに書き込む
        cameraCBData_->right = right;
        cameraCBData_->up = up;
        cameraCBData_->enable = enable ? 1.0f : 0.0f;
    }

    /// <summary>
    /// ビルボード描画用のカメラベクトルとビュー射影行列を設定
    /// </summary>
    void SetBillboardCameraWithVP(const Math::Vector3& right, const Math::Vector3& up, const Math::Matrix4x4& viewProj, bool enable)
    {
        // カメラ定数バッファが存在しない場合は設定をスキップ
        if (!cameraCBData_) {
            return;
        }
        // カメラベクトル、有効フラグ、ビュー射影行列を定数バッファに書き込む
        cameraCBData_->right = right;
        cameraCBData_->up = up;
        cameraCBData_->enable = enable ? 1.0f : 0.0f;
        cameraCBData_->viewProj = viewProj;
    }

    /// <summary>
    /// DirectXCommon へのアクセサ
    /// </summary>
    DirectXCommon* GetDxCommon() { return dxCommon_; }

    // カメラのワールド位置を GPU 定数バッファに書き込むための構造体
    struct CameraForGPU {
        Math::Vector3 worldPosition;
        float exposure;
        int toneMapOn;
        int hasEnvironmentMap;
        float pad1[2];
        Math::Matrix4x4 view;
    };

    /// <summary>
    /// カメラのワールド位置を格納する構造体へのポインタを取得
    /// </summary>
    CameraForGPU* GetCameraData() { return cameraData_; }

    /// <summary>
    /// カメラのワールド位置を格納する定数バッファのGPU仮想アドレスを取得
    /// </summary>
    D3D12_GPU_VIRTUAL_ADDRESS GetCameraGPUAddress() const;

    /// <summary>
    /// デフォルトカメラのセット
    /// </summary>
    void SetDefaultCamera(class Camera* camera) { defaultCamera_ = camera; }

    /// <summary>
    /// デフォルトカメラのゲット
    /// </summary>
    class Camera* GetDefaultCamera() const { return defaultCamera_; }

    /// <summary>
    /// ブレンドモードの設定
    /// </summary>
    void SetBlendMode(BlendMode mode);

    /// <summary>
    /// Object3dCommon に関連する共通設定を編集する関数
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// カメラ関連の UI を描画する関数
    /// </summary>
    void DrawCameraImGui();

    /// <summary>
    /// デバッグカメラ用ポインタをセットする
    /// </summary>
    void SetDebugCamera(::DebugCamera* dbg) { debugCamera_ = dbg; }

    /// <summary>
    /// レンダリングにデバッグカメラを使うかどうかを取得/設定
    /// </summary>
    bool GetUseDebugCameraForRender() const { return useDebugCameraForRender_; } // No-op
    void SetUseDebugCameraForRender(bool v) { useDebugCameraForRender_ = v; } // No-op

    /// <summary>
    /// デバッグカメラの入力を有効にするかどうかを取得/設定
    /// </summary>
    bool GetEnableDebugCameraInput() const { return enableDebugCameraInput_; } // No-op
    void SetEnableDebugCameraInput(bool v) { enableDebugCameraInput_ = v; } // No-op

    /// <summary>
    /// ブレンドモードの取得
    /// </summary>
    BlendMode GetBlendMode() const { return blendMode_; }

    /// <summary>
    /// パイプラインステートの再作成（ブレンドモード変更時などに呼び出す）
    /// </summary>
    void RecreatePipelines() { CreateGraphicsPipeline(); }

    // インスタンシング用ヘルパー

    /// <summary>
    /// インスタンシング用構造化バッファのマップ済みCPUポインタを取得
    /// </summary>
    Object3d::TransformationMatrix* GetInstancingData() const;

    /// <summary>
    /// インスタンシング用SRVのCPUディスクリプタハンドルを取得
    /// </summary>
    D3D12_GPU_DESCRIPTOR_HANDLE GetInstancingSrvGPUHandle() const;

    /// <summary>
    /// スロット数（インスタンシングで同時に描画できる最大インスタンス数）を取得
    /// </summary>
    uint32_t GetInstancingSlotCount() const { return kNumInstance_; }

private: // メンバ関数
    /// <summary>
    /// ルートシグネチャの作成
    /// </summary>
    void CreateRootSignature();

    /// <summary>
    /// グラフィックスパイプラインの作成
    /// </summary>
    void CreateGraphicsPipeline();

private: // メンバ変数
    DirectXCommon* dxCommon_; // DirectXCommon へのポインタ（外部で管理される）
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_; // 3Dオブジェクトの描画に使用するルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_; // 3Dオブジェクトの描画に使用するグラフィックスパイプラインステート

    // インスタンシング／パーティクル描画に使用するPSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancingPipelineState_;

    // すべての Object3d インスタンスで共有される平行光源リソース
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> directionalLightResources_;
    std::array<Object3d::DirectionalLight*, DirectXCommon::kFrameCount> mappedDirectionalLightData_ {};
    Object3d::DirectionalLight directionalLightState_ {};
    Object3d::DirectionalLight* directionalLightData_ = &directionalLightState_;

    // 複数の点光源を管理するためのリソース
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> pointLightsResources_;
    std::array<Object3d::PointLight*, DirectXCommon::kFrameCount> mappedPointLightsData_ {};
    std::array<Object3d::PointLight, kMaxPointLights> pointLightsState_ {};
    Object3d::PointLight* pointLightsData_ = pointLightsState_.data();

    // スポットライト用リソース (単一スポットライトを想定)
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> spotLightResources_;
    std::array<Object3d::SpotLight*, DirectXCommon::kFrameCount> mappedSpotLightData_ {};
    Object3d::SpotLight spotLightState_ {};
    Object3d::SpotLight* spotLightData_ = &spotLightState_;
    BlendMode blendMode_ = BlendMode::None;

    // カメラ定数バッファ（ワールド位置）
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> cameraResources_;
    std::array<CameraForGPU*, DirectXCommon::kFrameCount> mappedCameraData_ {};
    CameraForGPU cameraState_ {};
    CameraForGPU* cameraData_ = &cameraState_;

    // インスタンシング用リソース
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> instancingResources_;  // 変換を格納するアップロードバッファ
    std::array<Object3d::TransformationMatrix*, DirectXCommon::kFrameCount> instancingData_ {}; // マップ済みCPUポインタ
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, DirectXCommon::kFrameCount> instancingSrvHandlesCPU_ {};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, DirectXCommon::kFrameCount> instancingSrvHandlesGPU_ {};
    SrvManager* srvManager_ = nullptr; // SRVの割り当てと解放を管理する
    std::array<uint32_t, DirectXCommon::kFrameCount> instancingSrvIndices_ { UINT32_MAX, UINT32_MAX }; // インスタンシング用SRVの割り当て位置
    uint32_t kNumInstance_ = 0;

    D3D12_GPU_DESCRIPTOR_HANDLE environmentMapSrvHandleGPU_ = {};

    // ビルボード用カメラベクトル（b2）
    struct CameraCB {
        Math::Vector3 right;
        float pad0;
        Math::Vector3 up;
        float enable;
        Math::Matrix4x4 viewProj;
    };

    // カメラ定数バッファ（ビルボード用）
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectXCommon::kFrameCount> cameraCBResources_;
    std::array<CameraCB*, DirectXCommon::kFrameCount> mappedCameraCBData_ {};
    CameraCB cameraCBState_ {};
    CameraCB* cameraCBData_ = &cameraCBState_;

    // 3Dオブジェクトが参照するデフォルトカメラ
    class Camera* defaultCamera_ = nullptr;

    // デバッグカメラへのポインタ（外部からセットされる）
    DebugCamera* debugCamera_ = nullptr;

    // UIで制御されるフラグ: レンダリングにデバッグカメラを使用するか
    bool useDebugCameraForRender_ = false;

    // UIで制御されるフラグ: デバッグカメラの入力を有効にするか
    bool enableDebugCameraInput_ = true;

public: // メンバ変数へのアクセサ
    /// <summary>
    /// ポイントライトのデータ構造体のポインタを取得
    /// </summary>
    Object3d::PointLight* GetPointLightsData() { return pointLightsData_; }

    /// <summary>
    /// ポイントライトのデータ構造体のGPU仮想アドレスを取得
    /// </summary>
    D3D12_GPU_VIRTUAL_ADDRESS GetPointLightsGPUAddress() const;

    /// <summary>
    /// スポットライトのデータ構造体のポインタを取得
    /// </summary>
    Object3d::SpotLight* GetSpotLightData() { return spotLightData_; }

    /// <summary>
    /// スポットライトのデータ構造体のGPU仮想アドレスを取得
    /// </summary>
    D3D12_GPU_VIRTUAL_ADDRESS GetSpotLightGPUAddress() const;

    /// <summary>
    /// 点光源を追加する。成功すれば追加した点光源のインデックスを返す。上限に達していれば -1 を返す。
    /// </summary>
    int AddPointLight(const Object3d::PointLight& pl);

    /// <summary>
    /// 指定インデックスの点光源を削除する。成功すればtrue、インデックスが無効ならfalseを返す。
    /// </summary>
    bool RemovePointLight(int index);

    /// <summary>
    /// 指定インデックスの点光源を更新する。成功すればtrue、インデックスが無効ならfalseを返す。
    /// </summary>
    bool UpdatePointLight(int index, const Object3d::PointLight& pl);

    /// <summary>
    /// 点光源の最大数を取得
    /// </summary>
    uint32_t GetMaxPointLights() const { return kMaxPointLights; }
};

} // namespace MyEngine
