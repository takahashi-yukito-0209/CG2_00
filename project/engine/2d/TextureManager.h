#pragma once
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include <d3d12.h>
#include <string>
#include <wrl/client.h>

namespace MyEngine {

class TextureManager {
public:
    // シングルトンインスタンス取得
    static TextureManager* GetInstance();

    // 終了
    void Finalize();

    // 初期化
    void Initialize();

    // テクスチャファイルの読み込み
    void LoadTexture(const std::string& filePath);

    // ロード済みのすべてのテクスチャリソースの転送をまとめて実行し、中間リソースを解放する
    void ExecuteResourceUpload();

    //SRVインデックスの開始番号
    uint32_t GetTextureIndexByFilePath(const std::string& filePath);

    //テクスチャ番号からGPUハンドルを取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureIndex);

    // 現在ロード済みのテクスチャ数を返す
    uint32_t GetLoadedTextureCount() const { return static_cast<uint32_t>(textureDatas.size()); }

    //メタデータを取得
    const DirectX::TexMetadata& GetMetadata(uint32_t textureIndex);

private:
    static TextureManager* instance_;
    static uint32_t kSRVIndexTop_;

    TextureManager() = default;
    ~TextureManager() = default;
    TextureManager(TextureManager&) = delete;
    TextureManager& operator=(TextureManager&) = delete;

private:
    // テクスチャ1枚分のデータ
    struct TextureData {
        std::string filePath; // ファイルパス
        DirectX::TexMetadata metadata; // メタデータ
        Microsoft::WRL::ComPtr<ID3D12Resource> Resource; // リソース
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU; // SRVハンドル(CPU)
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU; // SRVハンドル(GPU)
        Microsoft::WRL::ComPtr<ID3D12Resource> IntermediateResource; // 中間リソース
    };

    // テクスチャデータ
    std::vector<TextureData> textureDatas;
};
} // namespace MyEngine
