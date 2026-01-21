#include "TextureManager.h"
#include "DirectXCommon.h"
#include "engine/base/SrvManager.h"
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

void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
}

void TextureManager::LoadTexture(const std::string& filePath)
{
    // 既読チェック（unordered_map の contains）
    std::string key = filePath;
    if (textureDatas.contains(key)) {
        const auto& td = textureDatas[key];
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

    // テクスチャファイルの読んでプログラムで扱えるようにする
    DirectX::ScratchImage image {};
    HRESULT hr = DirectX::LoadFromWICFile(wfilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    {
        char buf[128];
        sprintf_s(buf, "DEBUG hr = 0x%08X\n", static_cast<unsigned int>(hr));
        Logger::Log(buf);
    }

    // ロード失敗時の処理（エラーハンドリング）
    if (FAILED(hr)) {
        char buf[256];
        sprintf_s(buf, "ERROR LoadTexture: Failed to load texture: %s\n", filePath.c_str());
        Logger::Log(buf);
        return; // ロードに失敗した場合はここで終了
    }

    // ミップマップ生成
    DirectX::ScratchImage mipImages {};
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);

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
    if (textureData.metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE2D) {
        // 2Dテクスチャの場合
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        // mipmap のすべてのレベルを使用するように設定
        srvDesc.Texture2D.MipLevels = static_cast<UINT>(textureData.metadata.mipLevels);
    } else {
        // その他のテクスチャ（必要に応じて処理を追加）
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

    // SRV を CPU ヒープに作成した後、必要に応じてシェーダー可視ヒープへコピーして可視にすることを検討。
    // このアプリでは `DirectXCommon` で単一のシェーダー可視ヒープを使用しているため、ImGui 等からも利用可能。
    // ここではシェーダー可視ヒープの CPU ハンドルに直接 SRV を作成しているため追加処理は不要。

    // ロード成功のログ出力
    {
        char buf[256];
        sprintf_s(buf, "DEBUG LoadTexture: Loaded new texture: %s (SRV Index: %u)\n", storePath.c_str(), textureData.srvIndex);
        Logger::Log(buf);
    }
}

// すべてのテクスチャリソースの転送を実行し、中間リソースを解放
void TextureManager::ExecuteResourceUpload()
{
    // すべての中間リソースを解放する
    for (auto& kv : textureDatas) {
        kv.second.IntermediateResource.Reset();
    }
}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{
    // filePathキーに対応するSRVインデックス（絶対インデックス）を返す
    std::string key = filePath;
    auto it = textureDatas.find(key);
    if (it != textureDatas.end()) { return it->second.srvIndex; }

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

    char buf[256];
    sprintf_s(buf, "ERROR GetTextureIndexByFilePath: Texture not found: %s\n", filePath.c_str());
    Logger::Log(buf);
    return UINT32_MAX;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureIndex)
{
    // 引数はSRVヒープの絶対インデックスとして扱う
    D3D12_GPU_DESCRIPTOR_HANDLE nullHandle{};
    nullHandle.ptr = 0;
    if (textureDatas.empty()) { return nullHandle; }
    for (auto& kv : textureDatas) {
        const TextureData& td = kv.second;
        if (td.srvIndex == textureIndex) {
            if (td.srvHandleGPU.ptr == 0) {
                char buf[128]; sprintf_s(buf, "Warning: SRV GPU handle is null for srvIndex %u\n", textureIndex); Logger::Log(buf);
            }
            return td.srvHandleGPU;
        }
    }
    char buf[128]; sprintf_s(buf, "Warning: SRV index %u not found in textureDatas\n", textureIndex); Logger::Log(buf);
    return nullHandle;
}

const DirectX::TexMetadata& TextureManager::GetMetadata(uint32_t textureIndex)
{
    // 引数はSRV絶対インデックス。該当が無ければデフォルトを返す
    static DirectX::TexMetadata defaultMeta = [](){ DirectX::TexMetadata m{}; m.width = 1; m.height = 1; m.mipLevels = 1; m.format = DXGI_FORMAT_R8G8B8A8_UNORM; return m; }();
    if (textureDatas.empty()) { return defaultMeta; }
    for (auto& kv : textureDatas) {
        const TextureData& td = kv.second;
        if (td.srvIndex == textureIndex) { return td.metadata; }
    }
    char buf[128]; sprintf_s(buf, "Warning: GetMetadata srvIndex %u not found, returning default\n", textureIndex); Logger::Log(buf);
    return defaultMeta;
}

// 新API: filePath でメタデータ取得
const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath)
{
    static DirectX::TexMetadata defaultMeta = [](){ DirectX::TexMetadata m{}; m.width = 1; m.height = 1; m.mipLevels = 1; m.format = DXGI_FORMAT_R8G8B8A8_UNORM; return m; }();
    auto it = textureDatas.find(filePath);
    if (it == textureDatas.end()) { return defaultMeta; }
    return it->second.metadata;
}

// 新API: filePath で SRV インデックス取得
uint32_t TextureManager::GetSrvIndex(const std::string& filePath)
{
    auto it = textureDatas.find(filePath);
    if (it == textureDatas.end()) { return UINT32_MAX; }
    return it->second.srvIndex;
}

// 新API: filePath で GPU ハンドル取得
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath)
{
    D3D12_GPU_DESCRIPTOR_HANDLE nullHandle{}; nullHandle.ptr = 0;
    auto it = textureDatas.find(filePath);
    if (it == textureDatas.end()) { return nullHandle; }
    return it->second.srvHandleGPU;
}

std::vector<std::string> TextureManager::GetLoadedTextureFilePaths() const
{
    std::vector<std::string> paths;
    paths.reserve(textureDatas.size());
    for (const auto& kv : textureDatas) {
        paths.push_back(kv.first);
    }
    return paths;
}
