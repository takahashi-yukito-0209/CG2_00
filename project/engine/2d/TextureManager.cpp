#include "TextureManager.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"

using namespace MyEngine;

TextureManager* TextureManager::instance_ = nullptr;
uint32_t TextureManager::kSRVIndexTop_ = 1;

TextureManager* TextureManager::GetInstance()
{
    if (instance_ == nullptr) {
        instance_ = new TextureManager();
    }
    return instance_;
}

void TextureManager::Finalize()
{
    if (instance_ != nullptr) {
        delete instance_;
        instance_ = nullptr;
    }
}

void TextureManager::Initialize()
{
    // SRVの数と同数
    textureDatas.reserve(DirectXCommon::kMaxSRVCount);
}

void TextureManager::LoadTexture(const std::string& filePath)
{
    // 読み込み済みテクスチャを検索
    auto it = std::find_if(
        textureDatas.begin(), textureDatas.end(),
        [&](const TextureData& textureData) {
            return textureData.filePath == filePath;
        });

    // 見つかった場合は、ログを出力して終了する (SRVインデックスを返さない)
    if (it != textureDatas.end()) {
        // ロード済みのインデックスを計算してログに出力
        uint32_t index = static_cast<uint32_t>(std::distance(textureDatas.begin(), it)) + kSRVIndexTop_;
        Logger::Log(std::format("DEBUG LoadTexture: Already loaded texture: {} (Index: {})\n", filePath, index));
        // void型なのでここでreturn
        return;
    }

    // テクスチャ枚数上限チェック
    assert(textureDatas.size() + kSRVIndexTop_ < DirectXCommon::kMaxSRVCount);

    // std::string を std::wstring に変換
    std::wstring wfilePath = StringUtility::ConvertString(filePath);
    Logger::Log(std::format("DEBUG wfilePath = {}\n", StringUtility::ConvertString(wfilePath)));

    // テクスチャファイルの読んでプログラムで扱えるようにする
    DirectX::ScratchImage image {};
    HRESULT hr = DirectX::LoadFromWICFile(wfilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    Logger::Log(std::format("DEBUG hr = 0x{:08X}\n", hr));

    // ロード失敗時の処理 (エラーハンドリング) を追加することが望ましい
    if (FAILED(hr)) {
        Logger::Log(std::format("ERROR LoadTexture: Failed to load texture: {}\n", filePath));
        return; // ロードに失敗した場合はここで終了
    }

    // ミップマップ生成
    DirectX::ScratchImage mipImages {};
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);

    // テクスチャデータを追加
    textureDatas.resize(textureDatas.size() + 1);
    // 追加したテクスチャデータの参照を取得
    TextureData& textureData = textureDatas.back();

    textureData.filePath = filePath;
    textureData.metadata = mipImages.GetMetadata();
    // テクスチャリソースの生成
    textureData.Resource = DirectXCommon::GetInstance()->CreateTextureResource(textureData.metadata);

    // テクスチャデータの要素数番号をSRVのインデックスとする
    uint32_t srvIndex = static_cast<uint32_t>(textureDatas.size() - 1) + kSRVIndexTop_;

    // テクスチャデータのアップロード先ハンドルを取得
    textureData.srvHandleCPU = DirectXCommon::GetInstance()->GetSRVCPUDescriptorHandle(srvIndex);
    textureData.srvHandleGPU = DirectXCommon::GetInstance()->GetSRVGPUDescriptorHandle(srvIndex);

    // SRVの生成設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = textureData.metadata.format; // リソースのフォーマットに合わせる
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // 標準的なRGBAマッピング

    // リソースの種類に応じてViewDimensionを設定
    if (textureData.metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE2D) {
        // 2Dテクスチャの場合
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        // mipmapのすべてのレベルを使用するように設定
        srvDesc.Texture2D.MipLevels = static_cast<UINT>(textureData.metadata.mipLevels);
    } else {
        // その他のテクスチャ (必要に応じて処理を追加)
    }

    // 設定をもとにSRVの生成
    DirectXCommon::GetInstance()->GetDevice()->CreateShaderResourceView(
        textureData.Resource.Get(),
        &srvDesc,
        textureData.srvHandleCPU);

    // 中間リソースを生成し、転送コマンドを積む
    textureData.IntermediateResource = DirectXCommon::GetInstance()->UploadTextureData(
        textureData.Resource, // コピー先リソース（テクスチャリソース）
        mipImages // アップロードするデータ
    );

    // void型なので、ここでreturn srvIndex; は削除
    Logger::Log(std::format("DEBUG LoadTexture: Loaded new texture: {} (Index: {})\n", filePath, srvIndex));
}

// すべてのテクスチャリソースの転送を実行し、中間リソースを解放
void TextureManager::ExecuteResourceUpload()
{
    //// DirectXCommonの転送コマンド実行関数を呼び出す
    //DirectXCommon::GetInstance()->ExecuteCommandList();
     
    //// 実行完了を待機
    //DirectXCommon::GetInstance()->WaitForCommandExecution();
    
    //// 実行が完了したので、allocatorとcommandListをResetする
    //DirectXCommon::GetInstance()->ResetCommandList();

    // すべての中間リソースを解放する
    for (TextureData& data : textureDatas) {
        // ComPtr::Reset() で内部のID3D12Resource::Release() が呼ばれる
        data.IntermediateResource.Reset();
    }
}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{
    // 読み込み済みテクスチャを検索
    auto it = std::find_if(
        textureDatas.begin(), textureDatas.end(),
        [&](const TextureData& textureData) {
            return textureData.filePath == filePath;
        });

    if (it != textureDatas.end()) {
        // 見つかった場合はSRVインデックスを返す
        uint32_t textureIndex = static_cast<uint32_t>(std::distance(textureDatas.begin(), it));
        return textureIndex;
    }

    // 検索がヒットしない場合は、テクスチャが事前に読み込まれていないので assert で停止させる
    Logger::Log(std::format("ERROR GetTextureIndexByFilePath: Texture not found: {}\n", filePath));
    assert(false);
    return 0;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureIndex)
{
    // 配列の範囲チェック
    assert(textureIndex < textureDatas.size()); 

    TextureData& textureData = textureDatas[textureIndex];

    return textureData.srvHandleGPU;
}

const DirectX::TexMetadata& TextureManager::GetMetadata(uint32_t textureIndex)
{
    //範囲外指定違反チェック
    assert(textureIndex < textureDatas.size());

    TextureData& textureData = textureDatas[textureIndex];
    return textureData.metadata;
}
