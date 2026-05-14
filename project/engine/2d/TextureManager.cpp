#include "TextureManager.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#include "../utility/ResourceResolver.h"
#include "engine/base/SrvManager.h"
#include <algorithm>
#include <utility>

using namespace MyEngine;

//--- Static メンバ変数の定義 ---
TextureManager* TextureManager::instance_ = nullptr; // シングルトンインスタンスのポインタ
uint32_t TextureManager::kSRVIndexTop_ = 1; // SRVインデックスの開始位置（テクスチャ用のSRVはこのインデックスから割り当てる）

/// <summary>
/// シングルトンインスタンスの取得
/// </summary>
TextureManager* TextureManager::GetInstance()
{
    // インスタンスがまだ存在しない場合は生成する
    if (instance_ == nullptr) {
        instance_ = new TextureManager();
    }

    // 生成された（または既に存在する）インスタンスを返す
    return instance_;
}

/// <summary>
/// 終了処理
/// </summary>
void TextureManager::Finalize()
{
    // インスタンスが存在する場合は削除する
    if (instance_ != nullptr) {
        delete instance_;
        instance_ = nullptr;
    }
}

/// <summary>
/// 初期化処理
/// </summary>
void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    // 引数のDirectXCommonとSrvManagerの参照をメンバ変数に保存
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
}

/// <summary>
/// テクスチャのロードとSRVヒープへの登録
/// </summary>
void TextureManager::LoadTexture(const std::string& filePath)
{
    // ResourceResolverを使ってファイルパスを解決
    std::string key = ResourceResolver::Resolve(filePath, ResourceResolver::Type::Texture);
    // 解決できない場合は元のパスをキーとして使用
    if (key.empty()) {
        key = filePath;
    } 
    // すでにロードされているテクスチャか確認
    if (textureDatas.contains(key)) {
        const auto& td = textureDatas[key];
        // すでにロードされている場合はSRVインデックスをログに出力して終了
        char buf[256];
        sprintf_s(buf, "DEBUG LoadTexture: Already loaded texture: %s (SRV Index: %u)\n", filePath.c_str(), td.srvIndex);
        Logger::Log(buf);
        return;
    }

    // テクスチャ枚数の上限チェック（SRV確保可能か）
    if (srvManager_ ? !srvManager_->CanAllocate() : !CanAllocateMore()) {
        Logger::Log("ERROR LoadTexture: Exceeded maximum SRV count.\n");
        return;
    }

    // 保管するファイルパスを決定
    std::string storePath = key;
    // std::string を std::wstring に変換
    std::wstring wfilePath = StringUtility::ConvertString(storePath);
    {
        char buf[256];
        std::string tmp = StringUtility::ConvertString(wfilePath);
        sprintf_s(buf, "DEBUG wfilePath = %s\n", tmp.c_str());
        Logger::Log(buf);
    }

    // テクスチャファイルを読み込む (.dds は DirectX::LoadFromDDSFile を使う)
    DirectX::ScratchImage image {};
    HRESULT hr = S_OK;
    // 小文字化して拡張子判定
    std::string lowerPath = storePath;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c)); });
    bool isDDS = (lowerPath.size() >= 4 && lowerPath.substr(lowerPath.size() - 4) == ".dds");

    if (isDDS) {
        // DDSファイルはDirectXTexのDDSローダを使う（既にミップが含まれていることが多い）
        // LoadFromDDSFile の第2引数は DirectX::DDS_FLAGS 型なので明示的にキャストする
        hr = DirectX::LoadFromDDSFile(wfilePath.c_str(), static_cast<DirectX::DDS_FLAGS>(0), nullptr, image);
        if (FAILED(hr)) {
            char buf[256];
            sprintf_s(buf, "ERROR LoadTexture: Failed to load DDS texture: %s hr=0x%08X\n", filePath.c_str(), static_cast<unsigned int>(hr));
            Logger::Log(buf);
            return;
        }
    } else {
        // それ以外は既存のWICローダを使用
        hr = DirectX::LoadFromWICFile(wfilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
        if (FAILED(hr)) {
            char buf[256];
            sprintf_s(buf, "ERROR LoadTexture: Failed to load texture: %s hr=0x%08X\n", filePath.c_str(), static_cast<unsigned int>(hr));
            Logger::Log(buf);
            return;
        }
    }

    // ミップマップ生成／準備: DDSにミップが含まれていればそのまま、無ければ生成する
    DirectX::ScratchImage mipImages {};
    const DirectX::TexMetadata& srcMeta = image.GetMetadata();
    if (isDDS && srcMeta.mipLevels > 1) {
        // DDSにミップが含まれている
        // ScratchImage はコピー代入が削除されているのでムーブする
        mipImages = std::move(image);
    } else {
        // ミップを生成（WIC読み込み時やDDSでミップがない場合）
        hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
        if (FAILED(hr)) {
            char buf[256];
            sprintf_s(buf, "ERROR LoadTexture: GenerateMipMaps failed for %s hr=0x%08X\n", filePath.c_str(), static_cast<unsigned int>(hr));
            Logger::Log(buf);
            return;
        }
    }

    // SRV確保（インデックスを割り当て）
    uint32_t nextIndex = srvManager_ ? srvManager_->Allocate() : (static_cast<uint32_t>(textureDatas.size()) + kSRVIndexTop_);
    // テクスチャ枚数の上限チェック（確保可能か）
    if (nextIndex >= DirectXCommon::kMaxSRVCount) {
        Logger::Log("ERROR LoadTexture: SRV allocation exceeded heap size.\n");
        return;
    }

    // 追加したテクスチャデータの参照を取得（unordered_mapの要素作成）
    TextureData& textureData = textureDatas[storePath];
    textureData.srvIndex = nextIndex;
    textureData.metadata = mipImages.GetMetadata();
    // テクスチャリソースの生成
    textureData.Resource = DirectXCommon::GetInstance()->CreateTextureResource(textureData.metadata);

    // SRVハンドルを計算
    if (srvManager_) {
        textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
        textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);
    } else {
        textureData.srvHandleCPU = DirectXCommon::GetInstance()->GetSRVCPUDescriptorHandle(textureData.srvIndex);
        textureData.srvHandleGPU = DirectXCommon::GetInstance()->GetSRVGPUDescriptorHandle(textureData.srvIndex);
    }

    // SRVの生成設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = textureData.metadata.format; // リソースのフォーマットに合わせる
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // 標準的なRGBAマッピング

    // リソースの種類に応じて ViewDimension を設定
    // キューブマップ判定: metadata.miscFlags に TEX_MISC_TEXTURECUBE が立っているか、arraySize==6 ならキューブとみなす
    bool isCube = false;
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
            Logger::Log(buf);
        }
    } else if (textureData.metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE2D) {
        // 2Dテクスチャの場合
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        // mipmap のすべてのレベルを使用するように設定
        srvDesc.Texture2D.MipLevels = static_cast<UINT>(textureData.metadata.mipLevels);
    } else {
        // その他のテクスチャ（必要に応じて処理を追加）
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = static_cast<UINT>(textureData.metadata.mipLevels);
    }

    // 中間リソースを生成し、転送コマンドを積む
    textureData.IntermediateResource = DirectXCommon::GetInstance()->UploadTextureData(
        textureData.Resource, // コピー先リソース（テクスチャリソース）
        mipImages // アップロードするデータ
    );

    // SRV をアップロード完了後に作成する（リソースの状態が確定してから描画で安全に使えるようにする）
    DirectXCommon::GetInstance()->GetDevice()->CreateShaderResourceView(
        textureData.Resource.Get(),
        &srvDesc,
        textureData.srvHandleCPU);

    // デバッグ用: SRVハンドルの情報をログに出力
    {
        char buf[256];
        sprintf_s(buf, "DEBUG LoadTexture: Created SRV CPU.ptr=0x%016llX GPU.ptr=0x%016llX index=%u\n",
            static_cast<unsigned long long>(textureData.srvHandleCPU.ptr),
            static_cast<unsigned long long>(textureData.srvHandleGPU.ptr),
            textureData.srvIndex);
        Logger::Log(buf);
    }

    // SRVハンドルの生成確認
    if (textureData.srvHandleGPU.ptr == 0) {
        char buf[256];
        sprintf_s(buf, "ERROR LoadTexture: SRV GPU handle is null for %s\n", filePath.c_str());
        Logger::Log(buf);
    }

    // ロード成功のログ出力
    {
        char buf[256];
        sprintf_s(buf, "DEBUG LoadTexture: Loaded new texture: %s (SRV Index: %u)\n", storePath.c_str(), textureData.srvIndex);
        Logger::Log(buf);
    }
}

/// <summary>
/// 転送コマンドを実行して、GPUにテクスチャデータを転送する
/// </summary>
void TextureManager::ExecuteResourceUpload()
{
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
    {
        char buf[512];
        sprintf_s(buf, "GetTextureIndexByFilePath: lookup failed for '%s'. Registered count=%zu\n", filePath.c_str(), textureDatas.size());
        Logger::Log(buf);
        uint32_t i = 0;
        for (const auto& kv : textureDatas) {
            char buf2[512];
            sprintf_s(buf2, "  idx=%u path=%s\n", i++, kv.first.c_str());
            Logger::Log(buf2);
        }
    }

    // 見つからない場合は UINT32_MAX を返す
    char buf[256];
    sprintf_s(buf, "ERROR GetTextureIndexByFilePath: Texture not found: %s\n", filePath.c_str());
    Logger::Log(buf);
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
                Logger::Log(buf);
            }
            return td.srvHandleGPU;
        }
    }
    // 見つからない場合は null ハンドルを返す
    char buf[128];
    sprintf_s(buf, "Warning: SRV index %u not found in textureDatas\n", textureIndex);
    Logger::Log(buf);
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
    Logger::Log(buf);
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
    if (key.empty()) key = filePath;
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
    if (key.empty()) key = filePath;
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
    if (key.empty()) key = filePath;
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
