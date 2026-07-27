#include "TextureManager.h"
#include "../utility/ResourceResolver.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#include "engine/base/SrvManager.h"
#include <utility>

using namespace MyEngine;

//--- Static メンバ変数の定義 ---
TextureManager* TextureManager::instance_ = nullptr; // シングルトンインスタンスのポインタ
uint32_t TextureManager::kSRVIndexTop_ = 1; // SRVインデックスの開始位置

/// <summary>
/// シングルトンインスタンスを取得する
/// </summary>
TextureManager* TextureManager::GetInstance()
{
    if (instance_ == nullptr) {
        instance_ = new TextureManager();
    }
    return instance_;
}

/// <summary>
/// テクスチャマネージャを初期化する
/// </summary>
void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
}

/// <summary>
/// 登録済みテクスチャとSRV割り当てを解放する
/// </summary>
void TextureManager::Finalize()
{
    if (srvManager_) {
        for (auto& kv : textureDatas) {
            TextureData& textureData = kv.second; // 解放対象のテクスチャデータ
            if (textureData.srvIndex != UINT32_MAX) {
                srvManager_->Free(textureData.srvIndex);
                textureData.srvIndex = UINT32_MAX;
            }
        }
    }

    if (dxCommon_) {
        for (auto& kv : textureDatas) {
            TextureData& textureData = kv.second; // 遅延解放へ渡すテクスチャデータ
            dxCommon_->DeferReleaseResource(textureData.Resource);
            dxCommon_->DeferReleaseResource(textureData.IntermediateResource);
        }
    }

    textureDatas.clear();
    dxCommon_ = nullptr;
    srvManager_ = nullptr;
}

/// <summary>
/// テクスチャを読み込み、SRVヒープへ登録する
/// </summary>
void TextureManager::LoadTexture(const std::string& filePath)
{
    std::string key = ResourceResolver::Resolve(filePath, ResourceResolver::Type::Texture); // Resolverで解決したテクスチャパス
    if (key.empty()) {
        key = filePath;
    }

    auto it = textureDatas.find(key);
    if (it != textureDatas.end()) {
        const TextureData& textureData = it->second; // 既にロード済みのテクスチャデータ
        char buf[256];
        sprintf_s(buf, "DEBUG LoadTexture: Already loaded texture: %s (SRV Index: %u)\n", filePath.c_str(), textureData.srvIndex);
        Logger::Debug(buf);
        return;
    }

    if (srvManager_ ? !srvManager_->CanAllocate() : !CanAllocateMore()) {
        Logger::Error("ERROR LoadTexture: Exceeded maximum SRV count.\n");
        return;
    }

    // 保管するファイルパスを決定
    std::string storePath = key; // 解決済みのテクスチャファイルパス
    std::wstring wfilePath; // DirectXTexへ渡すワイド文字パス
    if (!StringUtility::TryConvertString(storePath, wfilePath)) {
        Logger::Error("ERROR LoadTexture: Failed to convert texture path to wide string.\n");
        return;
    }
    {
        char buf[256];
        std::string tmp; // ログ出力用に戻したUTF-8パス
        if (!StringUtility::TryConvertString(wfilePath, tmp)) {
            tmp = storePath;
        }
        sprintf_s(buf, "DEBUG wfilePath = %s\n", tmp.c_str());
        Logger::Debug(buf);
    }

    // テクスチャファイルを読み込む
    DirectX::ScratchImage image {};
    HRESULT hr = S_OK;
    const std::string lowerPath = StringUtility::ToLower(storePath); // 拡張子判定用の小文字化パス
    const bool isDDS = StringUtility::EndsWith(lowerPath, ".dds"); // DDS形式かどうか

    if (isDDS) {
        hr = DirectX::LoadFromDDSFile(wfilePath.c_str(), static_cast<DirectX::DDS_FLAGS>(0), nullptr, image);
        if (FAILED(hr)) {
            char buf[256];
            sprintf_s(buf, "ERROR LoadTexture: Failed to load DDS texture: %s hr=0x%08X\n", filePath.c_str(), static_cast<unsigned int>(hr));
            Logger::Error(buf);
            return;
        }
    } else {
        hr = DirectX::LoadFromWICFile(wfilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
        if (FAILED(hr)) {
            char buf[256];
            sprintf_s(buf, "ERROR LoadTexture: Failed to load texture: %s hr=0x%08X\n", filePath.c_str(), static_cast<unsigned int>(hr));
            Logger::Error(buf);
            return;
        }
    }

    DirectX::ScratchImage mipImages {};
    const DirectX::TexMetadata& srcMeta = image.GetMetadata(); // 読み込んだ元画像のメタデータ
    if (isDDS && srcMeta.mipLevels > 1) {
        mipImages = std::move(image);
    } else {
        hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
        if (FAILED(hr)) {
            char buf[256];
            sprintf_s(buf, "ERROR LoadTexture: GenerateMipMaps failed for %s hr=0x%08X\n", filePath.c_str(), static_cast<unsigned int>(hr));
            Logger::Error(buf);
            return;
        }
    }

    uint32_t nextIndex = srvManager_ ? srvManager_->Allocate() : (static_cast<uint32_t>(textureDatas.size()) + kSRVIndexTop_); // 割り当てたSRVインデックス
    if (nextIndex >= DirectXCommon::kMaxSRVCount) {
        Logger::Error("ERROR LoadTexture: SRV allocation exceeded heap size.\n");
        return;
    }

    TextureData& textureData = textureDatas[storePath];
    textureData.srvIndex = nextIndex;
    textureData.metadata = mipImages.GetMetadata();
    textureData.Resource = DirectXCommon::GetInstance()->CreateTextureResource(textureData.metadata);

    if (srvManager_) {
        textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
        textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);
    } else {
        textureData.srvHandleCPU = DirectXCommon::GetInstance()->GetSRVCPUDescriptorHandle(textureData.srvIndex);
        textureData.srvHandleGPU = DirectXCommon::GetInstance()->GetSRVGPUDescriptorHandle(textureData.srvIndex);
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = textureData.metadata.format; // リソース形式に合わせたSRVフォーマット
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    bool isCube = false; // キューブマップとして扱うかどうか
    if ((textureData.metadata.miscFlags & DirectX::TEX_MISC_TEXTURECUBE) != 0) {
        isCube = true;
    }
    if (textureData.metadata.arraySize == 6) {
        isCube = true;
    }

    if (isCube) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = static_cast<UINT>(textureData.metadata.mipLevels);
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        {
            char buf[256];
            sprintf_s(buf, "DEBUG LoadTexture: Detected cubemap. arraySize=%u mipLevels=%u format=%u\n",
                static_cast<unsigned int>(textureData.metadata.arraySize),
                static_cast<unsigned int>(textureData.metadata.mipLevels),
                static_cast<unsigned int>(textureData.metadata.format));
            Logger::Debug(buf);
        }
    } else if (textureData.metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE2D) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = static_cast<UINT>(textureData.metadata.mipLevels);
    } else {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = static_cast<UINT>(textureData.metadata.mipLevels);
    }

    textureData.IntermediateResource = DirectXCommon::GetInstance()->UploadTextureData(
        textureData.Resource,
        mipImages);

    DirectXCommon::GetInstance()->GetDevice()->CreateShaderResourceView(
        textureData.Resource.Get(),
        &srvDesc,
        textureData.srvHandleCPU);

    {
        char buf[256];
        sprintf_s(buf, "DEBUG LoadTexture: Created SRV CPU.ptr=0x%016llX GPU.ptr=0x%016llX index=%u\n",
            static_cast<unsigned long long>(textureData.srvHandleCPU.ptr),
            static_cast<unsigned long long>(textureData.srvHandleGPU.ptr),
            textureData.srvIndex);
        Logger::Debug(buf);
    }

    if (textureData.srvHandleGPU.ptr == 0) {
        char buf[256];
        sprintf_s(buf, "ERROR LoadTexture: SRV GPU handle is null for %s\n", filePath.c_str());
        Logger::Error(buf);
    }

    {
        char buf[256];
        sprintf_s(buf, "DEBUG LoadTexture: Loaded new texture: %s (SRV Index: %u)\n", storePath.c_str(), textureData.srvIndex);
        Logger::Debug(buf);
    }
}

/// <summary>
/// アップロード後に不要になった中間リソースを解放する
/// </summary>
void TextureManager::ReleaseIntermediateResources()
{
    if (dxCommon_) {
        dxCommon_->FlushTextureUploads();
    }

    // すべての中間リソースを解放する
    for (auto& kv : textureDatas) {
        kv.second.IntermediateResource.Reset();
    }
}

/// <summary>
/// 指定されたファイルパスのテクスチャがすでにロードされているか確認し、ロードされていればSRVインデックスを返す。ロードされていなければ UINT32_MAX を返す。
/// </summary>
uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{

    // ResourceResolverを使ってファイルパスを解決
    std::string key = ResourceResolver::Resolve(filePath, ResourceResolver::Type::Texture);
    // 解決できない場合は元のパスをキーとして使用
    if (key.empty()) {
        key = filePath;
    }

    // filePathキーに対応するSRVインデックスを返す。見つからない場合は UINT32_MAX を返す
    auto it = textureDatas.find(key);
    if (it != textureDatas.end()) {
        return it->second.srvIndex;
    }

    // デバッグ用: 現在の登録一覧を出力
    char buf[256];
    sprintf_s(buf, "DEBUG GetTextureIndexByFilePath: texture is not loaded: %s\n", filePath.c_str());
    Logger::Debug(buf);

    // 見つからない場合は UINT32_MAX を返す
    return UINT32_MAX;
}

/// <summary>
/// SRVヒープの絶対インデックスを指定してGPUハンドルを取得する。見つからない場合は null ハンドルを返す。
/// </summary>
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureIndex)
{
    // 引数はSRVヒープの絶対インデックスとして扱う
    D3D12_GPU_DESCRIPTOR_HANDLE nullHandle {};
    nullHandle.ptr = 0;
    // textureDatas を走査して、srvIndex が textureIndex と一致するものを探す
    if (textureDatas.empty()) {
        return nullHandle;
    }
    // もし該当する srvIndex を持つテクスチャが見つかったら、その GPU ハンドルを返す
    for (auto& kv : textureDatas) {
        const TextureData& td = kv.second;
        // srvIndex が一致するか確認
        if (td.srvIndex == textureIndex) {
            if (td.srvHandleGPU.ptr == 0) {
                char buf[128];
                sprintf_s(buf, "Warning: SRV GPU handle is null for srvIndex %u\n", textureIndex);
                Logger::Warn(buf);
            }
            return td.srvHandleGPU;
        }
    }
    // 見つからない場合は null ハンドルを返す
    char buf[128];
    sprintf_s(buf, "Warning: SRV index %u not found in textureDatas\n", textureIndex);
    Logger::Warn(buf);
    return nullHandle;
}

/// <summary>
/// テクスチャインデックス（SRV絶対インデックス）からメタデータを取得する。見つからない場合はデフォルトのメタデータを返す。
/// </summary>
const DirectX::TexMetadata& TextureManager::GetMetadata(uint32_t textureIndex)
{
    // 引数はSRV絶対インデックス。該当が無ければデフォルトを返す
    static DirectX::TexMetadata defaultMeta = []() { DirectX::TexMetadata m{}; m.width = 1; m.height = 1; m.mipLevels = 1; m.format = DXGI_FORMAT_R8G8B8A8_UNORM; return m; }();
    // textureDatas を走査して、srvIndex が textureIndex と一致するものを探す
    if (textureDatas.empty()) {
        return defaultMeta;
    }
    // もし該当する srvIndex を持つテクスチャが見つかったら、そのメタデータを返す
    for (auto& kv : textureDatas) {
        const TextureData& td = kv.second;
        // srvIndex が一致するか確認
        if (td.srvIndex == textureIndex) {
            return td.metadata;
        }
    }
    // 見つからない場合はデフォルトのメタデータを返す
    char buf[128];
    sprintf_s(buf, "Warning: GetMetadata srvIndex %u not found, returning default\n", textureIndex);
    Logger::Warn(buf);
    return defaultMeta;
}

/// <summary>
/// filePathキーでメタデータを取得する。見つからない場合はデフォルトのメタデータを返す。
/// </summary>
const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath)
{
    // filePathキーに対応するメタデータを返す。見つからない場合はデフォルトのメタデータを返す
    static DirectX::TexMetadata defaultMeta = []() { DirectX::TexMetadata m{}; m.width = 1; m.height = 1; m.mipLevels = 1; m.format = DXGI_FORMAT_R8G8B8A8_UNORM; return m; }();
    // もし filePath キーが見つかればそのメタデータを返す
    std::string key = ResourceResolver::Resolve(filePath, ResourceResolver::Type::Texture);
    if (key.empty())
        key = filePath;
    auto it = textureDatas.find(key);
    // 見つからない場合はデフォルトのメタデータを返す
    if (it == textureDatas.end()) {
        return defaultMeta;
    }
    // 見つかった場合はそのメタデータを返す
    return it->second.metadata;
}

/// <summary>
/// filePathキーでSRVインデックスを取得する。見つからない場合は UINT32_MAX を返す。
/// </summary>
uint32_t TextureManager::GetSrvIndex(const std::string& filePath)
{
    // filePathキーに対応するSRVインデックスを返す
    std::string key = ResourceResolver::Resolve(filePath, ResourceResolver::Type::Texture);
    if (key.empty())
        key = filePath;
    auto it = textureDatas.find(key);
    // 見つからない場合は UINT32_MAX を返す
    if (it == textureDatas.end()) {
        return UINT32_MAX;
    }
    // 見つかった場合はそのSRVインデックスを返す
    return it->second.srvIndex;
}

/// <summary>
/// filePathキーでSRVハンドル（GPU）を取得する。見つからない場合は null ハンドルを返す。
/// </summary>
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath)
{
    // filePathキーに対応するSRVハンドル（GPU）を返す
    D3D12_GPU_DESCRIPTOR_HANDLE nullHandle {};
    nullHandle.ptr = 0;
    // 見つからない場合は null ハンドルを返す
    std::string key = ResourceResolver::Resolve(filePath, ResourceResolver::Type::Texture);
    if (key.empty())
        key = filePath;
    auto it = textureDatas.find(key);
    // もし見つからない場合は null ハンドルを返す
    if (it == textureDatas.end()) {
        return nullHandle;
    }
    // 見つかった場合はそのSRVハンドルを返す
    return it->second.srvHandleGPU;
}

/// <summary>
/// ロード済みテクスチャのファイルパス一覧を取得
/// </summary>
std::vector<std::string> TextureManager::GetLoadedTextureFilePaths() const
{
    // textureDatas のキー（ファイルパス）をすべて取得してベクターにして返す
    std::vector<std::string> paths;
    // 事前にサイズを予約しておくと効率的
    paths.reserve(textureDatas.size());
    // textureDatas を走査してキーを取得
    for (const auto& kv : textureDatas) {
        // キー（ファイルパス）をベクターに追加
        paths.push_back(kv.first);
    }

    return paths;
}
