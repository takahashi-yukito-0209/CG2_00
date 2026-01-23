#pragma once
#include "DirectXCommon.h"
#include <d3d12.h>
#include <wrl.h>
#include "Object3d.h"
#include "../RenderState.h"

namespace MyEngine {

class Object3dCommon {
public: // メンバ関数
    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // サポートする最大の点光源数
    // ライティング設定を簡素化するために単一の点光源のみをサポートする
    static const uint32_t kMaxPointLights = 1;

    // 編集用にマップされた平行光源データへのポインタを取得
    Object3d::DirectionalLight* GetDirectionalLightData() { return directionalLightData_; }

    // 平行光源CBVのGPU仮想アドレスを取得
    D3D12_GPU_VIRTUAL_ADDRESS GetDirectionalLightGPUAddress() const { return directionalLightResource_ ? directionalLightResource_->GetGPUVirtualAddress() : 0; }

    //  共通描画設定
    void SetCommonDrawSetting();

    // インスタンシング描画設定（外部コードからインスタンシング／パーティクルPSOを選択できるよう公開）
    void SetInstancingDrawSetting();
    // ビルボード用カメラベクトルの更新
    void SetBillboardCamera(const Math::Vector3& right, const Math::Vector3& up, bool enable) {
        if (!cameraCBData_) return;
        cameraCBData_->right = right;
        cameraCBData_->up = up;
        cameraCBData_->enable = enable ? 1.0f : 0.0f;
    }
    void SetBillboardCameraWithVP(const Math::Vector3& right, const Math::Vector3& up, const Matrix4x4& viewProj, bool enable) {
        if (!cameraCBData_) return;
        cameraCBData_->right = right;
        cameraCBData_->up = up;
        cameraCBData_->enable = enable ? 1.0f : 0.0f;
        cameraCBData_->viewProj = viewProj;
    }

    // getter
    DirectXCommon* GetDxCommon() { return dxCommon_; }
    // カメラデータへのアクセス
    struct CameraForGPU { Math::Vector3 worldPosition; float pad; };
    CameraForGPU* GetCameraData() { return cameraData_; }
    D3D12_GPU_VIRTUAL_ADDRESS GetCameraGPUAddress() const { return cameraResource_ ? cameraResource_->GetGPUVirtualAddress() : 0; }

    // デフォルトカメラの管理
    void SetDefaultCamera(class Camera* camera) { defaultCamera_ = camera; }
    class Camera* GetDefaultCamera() const { return defaultCamera_; }

    // ブレンドモード制御
    void SetBlendMode(MyEngine::BlendMode mode);
    MyEngine::BlendMode GetBlendMode() const { return blendMode_; }
    // PSO等の再生成（明示的に呼び出して再構築する）
    void RecreatePipelines() { CreateGraphicsPipeline(); }

    // インスタンシング用ヘルパー
    // インスタンス毎の TransformationMatrix 配列へのCPUマップ済みポインタ（null許可）
    TransformationMatrix* GetInstancingData() const { return instancingData_; }
    // インスタンシング用構造化バッファのGPU可視SRVハンドル（null許可）
    D3D12_GPU_DESCRIPTOR_HANDLE GetInstancingSrvGPUHandle() const { return instancingSrvHandleGPU_; }
    // 割り当て済みインスタンススロット数
    uint32_t GetInstancingSlotCount() const { return kNumInstance_; }

private: // メンバ関数
    //  ルートシグネチャの作成
    void CreateRootSignature();
    // グラフィックスパイプラインの作成
    void CreateGraphicsPipeline();
    // インスタンシング／パーティクル用パイプラインの描画設定を作成・適用
    // （実装は .cpp 側）

private: // メンバ変数
    DirectXCommon* dxCommon_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
    // インスタンシング／パーティクル描画に使用するPSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancingPipelineState_;
    // すべての Object3d インスタンスで共有される平行光源リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
    Object3d::DirectionalLight* directionalLightData_ = nullptr;
    // 複数の点光源を管理するためのリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightsResource_;
    Object3d::PointLight* pointLightsData_ = nullptr;
    // スポットライト用リソース (単一スポットライトを想定)
    Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
    Object3d::SpotLight* spotLightData_ = nullptr;
    MyEngine::BlendMode blendMode_ = MyEngine::BlendMode::None;
    // カメラ定数バッファ（ワールド位置）
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
    CameraForGPU* cameraData_ = nullptr;
    
    // インスタンシング用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_ = nullptr; // 変換を格納するアップロードバッファ
    Object3d::TransformationMatrix* instancingData_ = nullptr; // マップ済みCPUポインタ
    D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU_ = {};
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_ = {};
    uint32_t kNumInstance_ = 0;

    // ビルボード用カメラベクトル（b2）
    struct CameraCB {
        Math::Vector3 right; float pad0;
        Math::Vector3 up;    float enable;
        Matrix4x4 viewProj;
    };
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraCBResource_ = nullptr;
    CameraCB* cameraCBData_ = nullptr;

    // 3Dオブジェクトが参照するデフォルトカメラ
    class Camera* defaultCamera_ = nullptr;

public:
    // 点光源用アクセサ
    Object3d::PointLight* GetPointLightsData() { return pointLightsData_; }
    D3D12_GPU_VIRTUAL_ADDRESS GetPointLightsGPUAddress() const { return pointLightsResource_ ? pointLightsResource_->GetGPUVirtualAddress() : 0; }

    // スポットライト用アクセサ
    Object3d::SpotLight* GetSpotLightData() { return spotLightData_; }
    D3D12_GPU_VIRTUAL_ADDRESS GetSpotLightGPUAddress() const { return spotLightResource_ ? spotLightResource_->GetGPUVirtualAddress() : 0; }

    // 点光源管理用API
    // 点光源を追加する。追加成功時にインデックスを返し、満杯/不可のときは -1 を返す
    int AddPointLight(const Object3d::PointLight& pl);
    // 指定インデックスの点光源を削除（無効化）する
    bool RemovePointLight(int index);
    // 指定インデックスの点光源を更新する
    bool UpdatePointLight(int index, const Object3d::PointLight& pl);
    // サポートする最大点光源数を取得
    uint32_t GetMaxPointLights() const { return kMaxPointLights; }

    // Point light manipulation helpers are provided via GetPointLightsData() access
};

} // namespace MyEngine