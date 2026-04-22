#include "SpriteCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

using namespace MyEngine;

/// <summary>
/// SpriteCommonの初期化と、Sprite描画に必要なリソースの生成
/// </summary>
void SpriteCommon::Initialize(DirectXCommon* dxCommon)
{
	//引数で受け取ってメンバ変数に記録する
    dxCommon_ = dxCommon;

	//グラフィクスパイプラインの生成
    CreateGraphicsPipeline();
}

#ifdef USE_IMGUI
void SpriteCommon::DrawImGui()
{
    // ブレンドモード設定
    const char* blendNames[] = { "None", "Alpha", "Add", "Multiply", "Screen" };
    int spriteBlendIdx = static_cast<int>(GetBlendMode());
    if (ImGui::Combo("Sprite Blend", &spriteBlendIdx, blendNames, IM_ARRAYSIZE(blendNames))) {
        SetBlendMode(static_cast<BlendMode>(spriteBlendIdx));
    }
}
#else
void SpriteCommon::DrawImGui() { (void)0; }
#endif

/// <summary>
/// ブレンドモードを設定
/// </summary>
void SpriteCommon::SetBlendMode(BlendMode mode)
{
    // ブレンドモードを更新
    blendMode_ = mode;
    // ブレンドモードの変更に伴い、グラフィクスパイプラインを再生成する
    CreateGraphicsPipeline();
}


/// <summary>
/// スプライト描画の共通設定をコマンドリストに設定
/// </summary>
void SpriteCommon::SetCommonDrawSetting()
{
    // 実行時のヌル参照を回避するための防御チェック
    // もしDirectXCommonへの参照がない場合は、ログに警告を出して処理を抜ける
    if (!dxCommon_) {
        Logger::Log("SpriteCommon::SetCommonDrawSetting: dxCommon_ is null\n");
        return;
    }

    // コマンドリストを取得
    auto cmdList = dxCommon_->GetCommandList();

    // コマンドリストがない場合警告を出して処理を抜ける
    if (!cmdList) {
        Logger::Log("SpriteCommon::SetCommonDrawSetting: command list is null\n");
        return;
    }

    // ルートシグネチャがない場合警告を出して処理を抜ける
    if (!rootSignature_) {
        Logger::Log("SpriteCommon::SetCommonDrawSetting: rootSignature_ is null\n");
        return;
    }

    // PSOがない場合警告を出して処理を抜ける
    if (!graphicsPipelineState_) {
        Logger::Log("SpriteCommon::SetCommonDrawSetting: graphicsPipelineState_ is null\n");
        return;
    }

    // ルートシグネチャとPSOをコマンドリストに設定
    cmdList->SetGraphicsRootSignature(rootSignature_.Get());
    cmdList->SetPipelineState(graphicsPipelineState_.Get()); // PSOを設定
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 形状を設定
}

/// <summary>
/// 描画用のルートシグネチャ/PSO が準備完了しているかを返す
/// </summary>
bool SpriteCommon::IsReady() const
{
    // ルートシグネチャとPSOが両方とも存在していれば描画可能とみなす
    return dxCommon_ && rootSignature_ && graphicsPipelineState_;
}

/// <summary>
/// ルートシグネチャを作成（内部処理）
/// </summary>
void SpriteCommon::CreateRootSignature()
{

    HRESULT hr; // HRESULT型の変数を用意して、以降のDirectX関数の戻り値を受け取るために使う

    // DescriptorRangeを作成。今回はPixelShaderでテクスチャSRVを1つ使う想定なので、1つだけ設定する。
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
    descriptorRange[0].NumDescriptors = 1; // 数は1つ
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算

    // RootSignature作成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature {};
    // 今回はInputLayoutを使う予定なので、IAステージでの使用を許可するフラグを指定する
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // RootParameter作成。複数設定できるので配列。
    D3D12_ROOT_PARAMETER rootParameters[3] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う (PixelShader, レジスタ0: マテリアルCBV)
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用することを明示
    rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ0にバインドする
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う (VertexShader, レジスタ0: WVP行列CBV)
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // バーテックスシェーダーで使用することを明示
    rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ0にバインドする
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う (PixelShader, レジスタ0: テクスチャSRV)
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用することを明示
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // ルートパラメーターのDescriptorTableに、先ほど作成したDescriptorRangeへのポインタを設定
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // ルートパラメーターのDescriptorTableに、DescriptorRange配列の長さを設定

    descriptionRootSignature.pParameters = rootParameters; // ルートパラメーター配列へのポインタ
    descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

    // Object3d シェーダーの期待に合わせたスタティックサンプラーを2つ用意する:
    // s0: 線形フィルタ + ラップ（既定）
    // s1: ポイントフィルタ + クランプ（アルファカットアウト用）
    D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};

    // s0: 線形フィルタ + ラップ
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 線形フィルタ
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // U座標はラップ
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // V座標はラップ
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // W座標はラップ
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較は使わない
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ミップマップは最大レベルまで使う
    staticSamplers[0].MipLODBias = 0.0f; // ミップマップレベルのバイアスはなし
    staticSamplers[0].MinLOD = 0.0f; // ミップマップレベルの最小値は0
    staticSamplers[0].ShaderRegister = 0; // シェーダーのレジスタ0にバインドする
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;// ピクセルシェーダーで使用することを明示

    // s1: ポイントフィルタ + クランプ
    staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; // ポイントフィルタ
    staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // U座標はクランプ
    staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // V座標はクランプ
    staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // W座標はクランプ
    staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較は使わない
    staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX; // ミップマップは最大レベルまで使う
    staticSamplers[1].MipLODBias = 0.0f; // ミップマップレベルのバイアスはなし
    staticSamplers[1].MinLOD = 0.0f; // ミップマップレベルの最小値は0
    staticSamplers[1].ShaderRegister = 1; // シェーダーのレジスタ1にバインドする
    staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使用することを明示

    descriptionRootSignature.pStaticSamplers = staticSamplers;// スタティックサンプラー配列へのポインタ
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers); // スタティックサンプラーの数

    // シリアライズしてバイナリにする
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    // ルートシグネチャの説明をもとに、D3D12SerializeRootSignature関数でシリアライズしてバイナリを生成する
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    // シリアライズに失敗していないか確認する。失敗していたらエラー内容をログに出す
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
/// グラフィクスパイプライン（PSO）を生成（内部処理）
/// </summary>
void SpriteCommon::CreateGraphicsPipeline()
{
    // HRESULT型の変数を用意して、以降のDirectX関数の戻り値を受け取るために使う
    HRESULT hr; 

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
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc {};
    inputLayoutDesc.pInputElementDescs = inputElementDescs; // InputElementDesc配列へのポインタ
    inputLayoutDesc.NumElements = _countof(inputElementDescs); // InputElementDesc配列の長さ

    // BlendState の設定を blendMode_ に応じて切り替える
    D3D12_BLEND_DESC blendDesc {};
    D3D12_RENDER_TARGET_BLEND_DESC& rtBlend = blendDesc.RenderTarget[0]; // 0番目のRenderTargetのブレンド設定を取得して、これを変更する
    rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // RGBA全てのチャンネルに書き込む
    rtBlend.LogicOpEnable = FALSE; // ロジックオペレーションは使わない
    rtBlend.BlendOp = D3D12_BLEND_OP_ADD; // ブレンドの演算は加算
    rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD; // アルファチャンネルのブレンドの演算も加算

    // ブレンドモードに応じて、BlendEnableやSrcBlend/DestBlendなどの設定を切り替える
    switch (blendMode_) {
    case BlendMode::None: // ブレンドなし

        rtBlend.BlendEnable = FALSE; // ブレンドを無効にする
        rtBlend.SrcBlend = D3D12_BLEND_ONE; // srcの色をそのまま使う
        rtBlend.DestBlend = D3D12_BLEND_ZERO; // destの色は使わない
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE; // srcのアルファをそのまま使う
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO; // destのアルファは使わない
        break;

    case BlendMode::Alpha: // アルファブレンド

        rtBlend.BlendEnable = TRUE; // ブレンドを有効にする
        rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA; // srcのアルファ値に応じて、srcの色を使う割合が変わる
        rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; // srcのアルファ値に応じて、destの色を使う割合が変わる
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE; // srcのアルファをそのまま使う
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO; // destのアルファは使わない
        break;

    case BlendMode::Add: // 加算

        rtBlend.BlendEnable = TRUE; // ブレンドを有効にする
        rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA; // srcのアルファ値に応じて、srcの色を使う割合が変わる
        rtBlend.DestBlend = D3D12_BLEND_ONE; // destの色を全て使う
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE; // srcのアルファをそのまま使う
        rtBlend.DestBlendAlpha = D3D12_BLEND_ONE; // destのアルファを全て使う
        break;

    case BlendMode::Multiply: // 乗算

        rtBlend.BlendEnable = TRUE; // ブレンドを有効にする
        rtBlend.SrcBlend = D3D12_BLEND_DEST_COLOR; // srcの色を、destの色に乗算したものを使う
        rtBlend.DestBlend = D3D12_BLEND_ZERO; // destの色は使わない
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE; // srcのアルファをそのまま使う
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO; // destのアルファは使わない
        break;

    case BlendMode::Screen: // スクリーン

        rtBlend.BlendEnable = TRUE; // ブレンドを有効にする
        rtBlend.SrcBlend = D3D12_BLEND_ONE; // srcの色を全て使う
        rtBlend.DestBlend = D3D12_BLEND_INV_SRC_COLOR; // srcの色を反転したものをdestの色に乗算して使う
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE; // srcのアルファをそのまま使う
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO; // destのアルファは使わない
        break;

    default: // その他のブレンドモードはアルファブレンドと同じにする

        rtBlend.BlendEnable = TRUE; // ブレンドを有効にする
        rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA; // srcのアルファ値に応じて、srcの色を使う割合が変わる
        rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; // srcのアルファ値に応じて、destの色を使う割合が変わる
        rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE; // srcのアルファをそのまま使う
        rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO; // destのアルファは使わない
        break;

    }

    // RasterizerStateの設定
    D3D12_RASTERIZER_DESC rasterizerDesc {};
    // カリングしない（裏面も表示させる）
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    // 三角形の中を塗りつぶす
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // Shaderをコンパイルする
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Sprite.VS.hlsl", L"vs_6_0");
    assert(vertexShaderBlob != nullptr);

    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Sprite.PS.hlsl", L"ps_6_0");
    assert(pixelShaderBlob != nullptr);

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
    graphicsPipelineStateDesc.RTVFormats[0] = dxCommon_->GetSwapChainFormat();
    // 利用するトロポジ（形状）のタイプ。三角形
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // どのように画面に色を打ち込むかの設定（気にしなくて良い）
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // DepthStencilStateの設定
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    // Depthの機能を有効化する
    depthStencilDesc.DepthEnable = true;
    // 書き込みします
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    // 比較関数はLessEqual。つまり、近ければ描画される
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // DepthStencilの設定
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // PSO生成前のデバッグ情報
    {
        char buf[512];
        sprintf_s(buf, "SpriteCommon::CreateGraphicsPipeline: creating PSO. device=%p rootSig=%p VS_sz=%zu PS_sz=%zu RTVFormat=%d DSVFormat=%d\n",
            dxCommon_->GetDevice(),
            rootSignature_.Get(),
            vertexShaderBlob ? vertexShaderBlob->GetBufferSize() : 0,
            pixelShaderBlob ? pixelShaderBlob->GetBufferSize() : 0,
            graphicsPipelineStateDesc.RTVFormats[0],
            graphicsPipelineStateDesc.DSVFormat);
        Logger::Log(buf);
    }

    // 実際に生成し、メンバ変数に保持する
    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
    // PSO生成後のデバッグ情報
    if (FAILED(hr) || !graphicsPipelineState_) {
        char buf[512];
        sprintf_s(buf, "SpriteCommon::CreateGraphicsPipeline: CreateGraphicsPipelineState failed. hr=0x%08X\n", static_cast<unsigned int>(hr));
        Logger::Log(buf);
        
        // デバイスが削除理由を返す場合はそれもログ出力
        HRESULT removedHr = dxCommon_->GetDevice()->GetDeviceRemovedReason();
        if (removedHr != S_OK) {
            char buf2[256]; sprintf_s(buf2, "SpriteCommon::CreateGraphicsPipeline: DeviceRemovedReason=0x%08X\n", static_cast<unsigned int>(removedHr)); Logger::Log(buf2);
        }

        // PSOの生成に失敗した場合は、ルートシグネチャとPSOを両方とも破棄して、描画できない状態にする
        graphicsPipelineState_.Reset();
        rootSignature_.Reset();
        return;
    }
}