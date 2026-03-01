#pragma once
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include <d3d12.h>
#include <string>
#include <wrl/client.h>
#include "engine/base/DirectXCommon.h"

// 前方宣言: MyEngine 名前空間内の DirectXCommon と SrvManager を宣言
namespace MyEngine { class SrvManager; }
#include <unordered_map>

namespace MyEngine {

/// <summary>
/// テクスチャマネージャクラス
/// </summary>
class TextureManager {
public: // メンバ関数

    /// <summary>
    /// シングルトンインスタンスの取得
    /// </summary>
    static TextureManager* GetInstance();

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    /// <summary>
    /// テクスチャのロードとSRVヒープへの登録
    /// </summary>
    void LoadTexture(const std::string& filePath);

    /// <summary>
    /// ロードしたテクスチャデータをGPUに転送
    /// </summary>
    void ExecuteResourceUpload();

    /// <summary>
    /// 指定されたファイルパスのテクスチャがすでにロードされているか確認し、ロードされていればSRVインデックスを返す。ロードされていなければ UINT32_MAX を返す。
    /// </summary>
    uint32_t GetTextureIndexByFilePath(const std::string& filePath);

    /// <summary>
    /// メタデータの取得
    /// </summary>
    const DirectX::TexMetadata& GetMetaData(const std::string& filePath);

    /// <summary>
    /// SRVインデックスの取得（filePathキー）
    /// </summary>
    uint32_t GetSrvIndex(const std::string& filePath);

    /// <summary>
    /// SRVハンドルの取得（filePathキー）
    /// </summary>
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

    /// <summary>
    /// ロード済みテクスチャのファイルパス一覧を取得
    /// </summary>
    std::vector<std::string> GetLoadedTextureFilePaths() const;

    /// <summary>
    /// // SRVヒープにさらにSRVを割り当て可能か（上限に達していないか）を確認
    /// </summary>
    bool CanAllocateMore() const {
        return (static_cast<uint32_t>(textureDatas.size()) + kSRVIndexTop_) < DirectXCommon::kMaxSRVCount;
    }

    /// <summary>
    /// SRVインデックスの取得（テクスチャインデックスキー）
    /// </summary>
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureIndex);

    /// <summary>
    /// ロード済みテクスチャの数を取得
    /// </summary>
    uint32_t GetLoadedTextureCount() const { return static_cast<uint32_t>(textureDatas.size()); }

    /// <summary>
    /// テクスチャインデックスからメタデータを取得
    /// </summary>
    const DirectX::TexMetadata& GetMetadata(uint32_t textureIndex);

private: // メンバ変数

    static TextureManager* instance_; // シングルトンインスタンスのポインタ
    static uint32_t kSRVIndexTop_; // SRVインデックスの開始位置（テクスチャ用のSRVはこのインデックスから割り当てる）

    // シングルトンパターンのため、コンストラクタとコピー/ムーブ関連の関数は削除
    TextureManager() = default; // デフォルトコンストラクタ
    ~TextureManager() = default; // デストラクタ
    TextureManager(TextureManager&) = delete; // コピーコンストラクタ
    TextureManager& operator=(TextureManager&) = delete; // コピー代入演算子

private: // 内部構造体: テクスチャデータ
    
    // テクスチャ1枚分のデータ
    struct TextureData {
        uint32_t srvIndex = 0; // SRVインデックス
        DirectX::TexMetadata metadata; // メタデータ
        Microsoft::WRL::ComPtr<ID3D12Resource> Resource; // リソース
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU; // SRVハンドル(CPU)
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU; // SRVハンドル(GPU)
        Microsoft::WRL::ComPtr<ID3D12Resource> IntermediateResource; // 中間リソース
    };

    // 参照先
    DirectXCommon* dxCommon_ = nullptr; // DirectXCommonへの参照（リソース生成やコマンドリストへのアクセスに使用）
    SrvManager* srvManager_ = nullptr; // SrvManagerへの参照（SRVヒープへのSRV登録に使用）

    // テクスチャデータ（filePath をキーとして保持）
    std::unordered_map<std::string, TextureData> textureDatas;
};
} // namespace MyEngine
