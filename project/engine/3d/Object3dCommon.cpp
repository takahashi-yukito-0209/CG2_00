#include "Object3dCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#include "mathUtility.h"

using namespace MyEngine;

/// <summary>
/// 初期化
/// </summary>
void Object3dCommon::Initialize(DirectXCommon* dxCommon)
{
    // 引数で受け取ってメンバ変数に記録する
    dxCommon_ = dxCommon;
    // 共有の平行光源用定数バッファを作成
    // これにより main/ImGui からすべての Object3d インスタンスで使用する単一のライトを編集できる
    directionalLightResource_ = dxCommon_->GetDevice() ? dxCommon_->CreateBufferResource(sizeof(Object3d::DirectionalLight)) : nullptr;
    
    // バッファが作成できた場合はマッピングして初期値を設定する
    if (directionalLightResource_) {
        directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
        // 既定値の設定
        directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
        directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
        directionalLightData_->intensity = 1.0f;
    }

    // 点光源用バッファを作成（最大数は Object3dCommon::kMaxPointLights）
    const uint32_t kMaxPointLights = Object3dCommon::kMaxPointLights;
    // CPU側の構造体レイアウトに合わせて、GPU側のバッファも同じレイアウトで作成する必要がある
    size_t pointLightsBufferSize = sizeof(Object3d::PointLight) * static_cast<size_t>(kMaxPointLights);
    pointLightsResource_ = dxCommon_->CreateBufferResource(pointLightsBufferSize);

    // バッファが作成できた場合はマッピングして初期値を設定する
    if (pointLightsResource_) {
        pointLightsResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightsData_));
        // デフォルトで無効化しておく
        for (uint32_t i = 0; i < kMaxPointLights; ++i) {
            // 無効化のため、位置を原点、色を白、半径と減衰を適当な値に設定し、enabled を 0 にする
            pointLightsData_[i].position = {0.0f, 0.0f, 0.0f, 0.0f};
            pointLightsData_[i].color = {1.0f, 1.0f, 1.0f, 1.0f};
            pointLightsData_[i].radius = 10.0f;
            pointLightsData_[i].decay = 2.0f;
            pointLightsData_[i].enabled = 0;
            pointLightsData_[i].padding = 0.0f;
        }
    }

    // スポットライト用バッファを作成 (単一スポットライトを想定)
    size_t spotLightBufferSize = sizeof(Object3d::SpotLight);
    // CPU側の構造体レイアウトに合わせて、GPU側のバッファも同じレイアウトで作成する必要がある
    spotLightResource_ = dxCommon_->CreateBufferResource(spotLightBufferSize);
    // バッファが作成できた場合はマッピングして初期値を設定する
    if (spotLightResource_) {
        spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));
        // デフォルトで無効化しておく
        spotLightData_->position = {0.0f, 0.0f, 0.0f, 0.0f};
        spotLightData_->color = {1.0f, 1.0f, 1.0f, 1.0f};
        spotLightData_->direction = {0.0f, -1.0f, 0.0f};
        spotLightData_->distance = 10.0f;
        spotLightData_->decay = 2.0f;
        spotLightData_->cosAngle = 1.0f; 
        spotLightData_->cosFalloffStart = 1.0f; 
        spotLightData_->enabled = 0;
        spotLightData_->padding = 0.0f;
    }
  
    // ビルボード用のカメラ定数バッファ（b2）を作成
    cameraCBResource_ = dxCommon_->CreateBufferResource(sizeof(CameraCB));
    // バッファが作成できた場合はマッピングして初期値を設定する
    if (cameraCBResource_) {
        // カメラ定数バッファをマップして、初期値を設定する
        cameraCBResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraCBData_));
        cameraCBData_->right = {1.0f, 0.0f, 0.0f};
        cameraCBData_->up    = {0.0f, 1.0f, 0.0f};
        cameraCBData_->enable = 0.0f;
        // 初期のViewProjは単位行列
        cameraCBData_->pad0 = 0.0f; // パディングを書き込むことを保証
        cameraCBData_->enable = 0.0f;
    }

    // カメラ用定数バッファ
    cameraResource_ = dxCommon_->CreateBufferResource(sizeof(CameraForGPU));
    if (cameraResource_) {
        cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
        cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
        cameraData_->pad = 0.0f;
    }

    // グラフィックスパイプラインの生成
    CreateGraphicsPipeline();

    // インスタンシング用リソースの作成（構造化バッファ + SRV）
    const uint32_t kNumInstance = 100; // パーティクルデモ用に最大インスタンス数を増加
    kNumInstance_ = kNumInstance;

    // TransformationMatrix[kNumInstance] を格納する構造化バッファ向けのGPU可視SRVを作成
    D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
    instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN; // 構造化バッファ
    instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER; // バッファビュー
    instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // バッファの先頭から使用
    instancingSrvDesc.Buffer.FirstElement = 0; // バッファの先頭から
    instancingSrvDesc.Buffer.NumElements = kNumInstance; // kNumInstance分の構造体を格納
    instancingSrvDesc.Buffer.StructureByteStride = sizeof(Object3d::TransformationMatrix); // 1インスタンス分のサイズ
    instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE; // フラグは特に必要ないため NONE

    // インスタンスデータを格納するため、UPLOADヒープに既定サイズのバッファリソースを作成
    size_t instancingBufferSize = sizeof(Object3d::TransformationMatrix) * kNumInstance;
    instancingResource_ = dxCommon_->CreateBufferResource(instancingBufferSize);
    // CPUからの書き込みのためにマップ
    if (instancingResource_) {
        instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));
        // 単位行列で初期化
        MathUtility math;
        for (uint32_t i = 0; i < kNumInstance; ++i) {
            instancingData_[i].WVP = math.MakeIdentity4x4();
            instancingData_[i].World = math.MakeIdentity4x4();
            instancingData_[i].color = {1.0f, 1.0f, 1.0f, 1.0f};
        }
    }

    // グローバルSRVヒープにSRVディスクリプタを作成
    // TextureManagerと競合しにくいディスクリプタスロットとして、ヒープの最後のスロットを使用
    uint32_t srvIndex = DirectXCommon::kMaxSRVCount - 1;
    instancingSrvHandleCPU_ = dxCommon_->GetSRVCPUDescriptorHandle(srvIndex);
    instancingSrvHandleGPU_ = dxCommon_->GetSRVGPUDescriptorHandle(srvIndex);
    dxCommon_->GetDevice()->CreateShaderResourceView(instancingResource_.Get(), &instancingSrvDesc, instancingSrvHandleCPU_);

    // デバッグログ
    {
        char buf[256];
        sprintf_s(buf, "Object3dCommon::Initialize: created instancingResource size=%zu srvIndex=%u srvGPU=0x%016llX\n",
            instancingBufferSize, srvIndex, static_cast<unsigned long long>(instancingSrvHandleGPU_.ptr));
        Logger::Log(buf);
    }
}

/// <summary>
/// ブレンドモードの設定
/// </summary>
void Object3dCommon::SetBlendMode(BlendMode mode)
{
    // 選択されたブレンドモードを保存し、PSOを再生成する
    blendMode_ = mode;
    // 新しいブレンドステートを適用するためパイプラインを再生成
    CreateGraphicsPipeline();
}

/// <summary>
/// インスタンシング／パーティクル用の描画設定をコマンドリストに設定
/// </summary>
void Object3dCommon::SetInstancingDrawSetting()
{
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
        Logger::Log("Object3dCommon::SetInstancingDrawSetting: instancingPipelineState_ is null, falling back to graphicsPipelineState_\n");
        if (!graphicsPipelineState_) {
            Logger::Log("Object3dCommon::SetInstancingDrawSetting: graphicsPipelineState_ also null\n");
            return;
        }
        cmdList->SetGraphicsRootSignature(rootSignature_.Get()); // ルートシグネチャは共通
        cmdList->SetPipelineState(graphicsPipelineState_.Get()); // フォールバックして通常のグラフィックスPSOを使用
    } else {
        cmdList->SetGraphicsRootSignature(rootSignature_.Get()); // ルートシグネチャは共通
        cmdList->SetPipelineState(instancingPipelineState_.Get()); // インスタンシング用PSOを使用
    }

    // プリミティブトポロジーをセットするコマンド
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // 使用可能ならビルボード用カメラCB（VSのb2 -> ルートインデックス5）をバインド
    if (cameraCBResource_) {
        cmdList->SetGraphicsRootConstantBufferView(5, cameraCBResource_->GetGPUVirtualAddress());
    }
    // スポットライトCBVをルートにバインド (ルートインデックス8 -> PS b5)
    // シェーダー側では b5 にスポットライトが期待されるが、以前はここでバインドされていなかったため
    // スポットライトUIで編集しても描画に反映されていなかった。
    if (spotLightResource_) {
        cmdList->SetGraphicsRootConstantBufferView(8, spotLightResource_->GetGPUVirtualAddress());
    }
}

/// <summary>
/// 共通描画設定をコマンドリストに設定
/// </summary>
void Object3dCommon::SetCommonDrawSetting()
{
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
    if (!graphicsPipelineState_) {
        Logger::Log("Object3dCommon::SetCommonDrawSetting: graphicsPipelineState_ is null\n");
        return;
    }

    // ルートシグネチャをセットするコマンド
    cmdList->SetGraphicsRootSignature(rootSignature_.Get());
    // パイプラインステートをセットするコマンド
    cmdList->SetPipelineState(graphicsPipelineState_.Get()); // PSOを設定                                                                          
    // プリミティブトポロジーをセットするコマンド
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 形状を設定

    // Spot light CBV を全ての通常描画パスでもバインドする（シェーダー b5 / ルートパラメータ 8）
    if (spotLightResource_) {
        cmdList->SetGraphicsRootConstantBufferView(8, spotLightResource_->GetGPUVirtualAddress());
    }
}

/// <summary>
/// ルートシグネチャの作成
/// </summary>
void Object3dCommon::CreateRootSignature() {
    
    HRESULT hr; // HRESULTはDirectXの関数の成功/失敗を表す戻り値の型

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
    D3D12_ROOT_PARAMETER rootParameters[9] = {};
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
    staticSamplers[0].MipLODBias = 0.0f; // ミップレベルのバイアスはなし
    staticSamplers[0].MinLOD = 0.0f; // ミップレベルの最小値は0（最も高解像度のミップレベルから使用）
    staticSamplers[0].ShaderRegister = 0; // シェーダーで s0 にバインドされる
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用

    // s1: ポイントフィルタ + クランプ（アルファカットアウトテクスチャのブリーディングを防ぐために有用）
    staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; // ポイントフィルタ
    staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // U方向はクランプ
    staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // V方向はクランプ
    staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // W方向はクランプ
    staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較は使わないのでNEVER
    staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX; // ミップレベルの最大値は無限大（利用可能な最大ミップレベルまで使用）
    staticSamplers[1].MipLODBias = 0.0f; // ミップレベルのバイアスはなし
    staticSamplers[1].MinLOD = 0.0f; // ミップレベルの最小値は0（最も高解像度のミップレベルから使用）
    staticSamplers[1].ShaderRegister = 1; // シェーダーで s1 にバインドされる
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
void Object3dCommon::CreateGraphicsPipeline() {

    HRESULT hr; // HRESULTはDirectXの関数の成功/失敗を表す戻り値の型

    // ルートシグネチャの作成を先に実行する
    CreateRootSignature();

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

    // BlendState の設定を blendMode_ に応じて切り替える
    D3D12_BLEND_DESC blendDesc {};
    D3D12_RENDER_TARGET_BLEND_DESC& rtBlend = blendDesc.RenderTarget[0]; // 1つ目のレンダーターゲットのブレンド設定を取得
    rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // RGBA全てのチャンネルに書き込む
    rtBlend.LogicOpEnable = FALSE; // ロジックオペレーションは使わない
    rtBlend.BlendOp = D3D12_BLEND_OP_ADD; // ブレンドオペレーションは加算（src + dest）を基本とする
    rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD; // アルファブレンドオペレーションも加算を基本とする

    // ブレンドモードに応じて、ソースブレンドとデスティネーションブレンドを設定する
    switch (blendMode_) {
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
    // 裏面（時計回り）を表示しない
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
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
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    // 利用するトポロジ（形状）のタイプ。三角形
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // どのように画面に色を打ち込むかの設定（気にしなくて良い）
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // DepthStencilStateの設定
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    // Depthの機能を有効化する
    depthStencilDesc.DepthEnable = true;
    // Depth書き込みマスク: 透明表現（例: Alpha）を使用するブレンドモードでは不正なオクルージョンを防ぐため深度書き込みを無効化。
    // 不透明（None）のレンダリングでは深度書き込みを有効化する。
    if (blendMode_ == BlendMode::Alpha || blendMode_ == BlendMode::Add || blendMode_ == BlendMode::Multiply || blendMode_ == BlendMode::Screen) {
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
    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
    assert(SUCCEEDED(hr));

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
        instDesc.BlendState = blendDesc; // ブレンドステートは共通
        instDesc.RasterizerState = rasterizerDesc; // ラスタライザーステートも共通
        instDesc.NumRenderTargets = 1; // 書き込むRTVの情報
        instDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 書き込むRTVの情報
        instDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // 利用するトポロジ（形状）のタイプ。三角形
        instDesc.SampleDesc.Count = 1; // どのように画面に色を打ち込むかの設定（気にしなくて良い）
        instDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // ここまでは共通の設定
        instDesc.DepthStencilState = depthStencilDesc; // DepthStencilの設定も共通
        instDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilのフォーマットも共通
        // インスタンシング用PSOを生成し、メンバ変数に保持する
        HRESULT r = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&instDesc, IID_PPV_ARGS(&instancingPipelineState_));
        // インスタンシング用PSOの生成に失敗した場合は、エラーログを出力して、インスタンシング用PSOをリセットする
        if (FAILED(r)) {
            char buf[256]; sprintf_s(buf, "Object3dCommon::CreateGraphicsPipeline: failed to create instancing PSO hr=0x%08X\n", static_cast<unsigned int>(r)); Logger::Log(buf);
            instancingPipelineState_.Reset();
        }
    } else {
        // インスタンシング用のシェーダーが見つからなかった場合は、エラーログを出力して、インスタンシング用PSOをリセットする
        Logger::Log("Object3dCommon::CreateGraphicsPipeline: Particleシェーダーが見つからなかったため、インスタンシング用PSOは作成されませんでした\n");
    }
}