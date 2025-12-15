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

    // 見つかった場合はログして終了
    if (it != textureDatas.end()) {
        uint32_t index = static_cast<uint32_t>(std::distance(textureDatas.begin(), it)) + kSRVIndexTop_;
        char buf[256];
        sprintf_s(buf, "DEBUG LoadTexture: Already loaded texture: %s (Index: %u)\n", filePath.c_str(), index);
        Logger::Log(buf);
        return;
    }

    // テクスチャ枚数上限チェック
    if (textureDatas.size() + kSRVIndexTop_ >= DirectXCommon::kMaxSRVCount) {
        Logger::Log("ERROR LoadTexture: Exceeded maximum SRV count.\n");
        return;
    }

    // std::string を std::wstring に変換
    std::wstring wfilePath = StringUtility::ConvertString(filePath);
    {
        char buf[256];
        std::string tmp = StringUtility::ConvertString(wfilePath);
        sprintf_s(buf, "DEBUG wfilePath = %s\n", tmp.c_str());
        Logger::Log(buf);
    }

    // テクスチャファイルの読んでプログラムで扱えるようにする
    DirectX::ScratchImage image {};
    HRESULT hr = DirectX::LoadFromWICFile(wfilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    {
        char buf[128];
        sprintf_s(buf, "DEBUG hr = 0x%08X\n", static_cast<unsigned int>(hr));
        Logger::Log(buf);
    }

    // ロード失敗時の処理 (エラーハンドリング) を追加することが望ましい
    if (FAILED(hr)) {
        char buf[256];
        sprintf_s(buf, "ERROR LoadTexture: Failed to load texture: %s\n", filePath.c_str());
        Logger::Log(buf);
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

    // SRVハンドルの生成確認
    if (textureData.srvHandleGPU.ptr == 0) {
        char buf[256];
        sprintf_s(buf, "ERROR LoadTexture: SRV GPU handle is null for %s\n", filePath.c_str());
        Logger::Log(buf);
    }

    // ロード成功のログ出力
    {
        char buf[256];
        sprintf_s(buf, "DEBUG LoadTexture: Loaded new texture: %s (Index: %u)\n", filePath.c_str(), srvIndex);
        Logger::Log(buf);
    }
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

    // 検索がヒットしない場合は、テクスチャが事前に読み込まれていないのでエラーをログして無効値を返す
    char buf[256];
    sprintf_s(buf, "ERROR GetTextureIndexByFilePath: Texture not found: %s\n", filePath.c_str());
    Logger::Log(buf);
    // 呼び出し元で範囲チェックできるように 無効値 UINT32_MAX を返す
    return UINT32_MAX;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureIndex)
{
    // 配列の範囲チェック
    D3D12_GPU_DESCRIPTOR_HANDLE nullHandle{};
    nullHandle.ptr = 0;
    if (textureIndex >= textureDatas.size()) {
        char buf[128];
        sprintf_s(buf, "Warning: Requested SRV index out of range: %u\n", textureIndex);
        Logger::Log(buf);
        return nullHandle;
    }

    TextureData& textureData = textureDatas[textureIndex];
    // ハンドルの有効性チェック
    if (textureData.srvHandleGPU.ptr == 0) {
        char buf[128];
        sprintf_s(buf, "Warning: Requested SRV GPU handle is null for index %u\n", textureIndex);
        Logger::Log(buf);
    }
    return textureData.srvHandleGPU;
}

const DirectX::TexMetadata& TextureManager::GetMetadata(uint32_t textureIndex)
{
    // 範囲外指定違反チェック -> 範囲外の場合はデフォルトの安全なメタデータを返す
    static DirectX::TexMetadata defaultMeta = [](){ DirectX::TexMetadata m{}; m.width = 1; m.height = 1; m.mipLevels = 1; m.format = DXGI_FORMAT_R8G8B8A8_UNORM; return m; }();
    if (textureIndex >= textureDatas.size()) {
        char buf[128];
        sprintf_s(buf, "Warning: GetMetadata called with out-of-range index %u, returning default metadata\n", textureIndex);
        Logger::Log(buf);
        return defaultMeta;
    }

    TextureData& textureData = textureDatas[textureIndex];
    return textureData.metadata;
}
