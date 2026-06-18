#pragma once

#include <Windows.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <wrl.h>
#include <xaudio2.h>

namespace MyEngine {

/// <summary>
/// サウンドクリップクラス：波形フォーマットと音声バッファを保持
/// </summary>
class SoundClip {
public: // メンバ変数
    // 波形フォーマット情報
    WAVEFORMATEX wfex {};
    // PCM 等の音声バイトバッファ
    std::vector<uint8_t> buffer;

public: // メンバ関数
    /// <summary>
    /// 音声データの生ポインタを取得（バッファが空の場合は nullptr を返す）
    /// </summary>
    const uint8_t* GetData() const { return buffer.empty() ? nullptr : buffer.data(); }

    /// <summary>
    /// 音声データのサイズを取得
    /// </summary>
    uint32_t GetSize() const { return static_cast<uint32_t>(buffer.size()); }

    /// <summary>
    /// 波形フォーマットが有効かどうかを判定（バッファが空でないこと、チャンネル数とサンプルレートが正の値であること）
    /// </summary>
    bool IsValid() const { return !buffer.empty() && wfex.nChannels > 0 && wfex.nSamplesPerSec > 0; }
};

/// <summary>
/// サウンドシステムクラス：Media Foundation と XAudio2 を利用してサウンドの読み込みと再生を管理
/// </summary>
class SoundSystem {
public: // メンバ関数
    /// <summary>
    /// 未初期化状態のサウンドシステムを作成する
    /// </summary>
    SoundSystem() = default;

    /// <summary>
    /// 音声システムが終了済みであることを保証する
    /// </summary>
    ~SoundSystem();

    /// <summary>
    /// Media FoundationとXAudio2を初期化する
    /// </summary>
    bool Initialize();

    /// <summary>
    /// 再生中ボイスと音声システムを終了する
    /// </summary>
    void Finalize();

    /// <summary>
    /// 音声再生が利用可能か取得する
    /// </summary>
    bool IsReady() const { return xaudio2_.Get() != nullptr && masteringVoice_ != nullptr; }

    /// <summary>
    /// 指定されたファイルパスからサウンドを読み込む。WAV 形式は LoadWav、MP3 などの圧縮フォーマットは LoadViaMediaFoundation を内部で呼び分け
    /// </summary>
    std::shared_ptr<SoundClip> LoadFromFile(const std::string& path);

    /// <summary>
    /// 指定されたサウンドクリップを再生する。内部で IXAudio2SourceVoice を作成し、サウンドクリップのバッファを送信して再生開始する
    /// </summary>
    void Play(const std::shared_ptr<SoundClip>& clip);

    /// <summary>
    /// 再生中のサウンドの状態を更新する。再生終了したサウンドのコンテキストをクリーンアップするために、ゲームループの適切なタイミングで呼び出す必要がある
    /// </summary>
    void Poll();

private: // メンバ関数（内部実装用）
    /// <summary>
    /// WAV 形式のサウンドを読み込む。WAVEFORMATEX と PCM バッファを SoundClip に格納して返す
    /// </summary>
    std::shared_ptr<SoundClip> LoadWav(const std::string& path);

    /// <summary>
    /// Media Foundation を利用して MP3 などの圧縮フォーマットのサウンドを読み込む。WAVEFORMATEX と PCM バッファを SoundClip に格納して返す
    /// </summary>
    std::shared_ptr<SoundClip> LoadViaMediaFoundation(const std::wstring& wpath);

private: // メンバ変数
    // XAudio2 エンジン
    Microsoft::WRL::ComPtr<IXAudio2> xaudio2_;
    IXAudio2MasteringVoice* masteringVoice_ = nullptr;
    bool mediaFoundationStarted_ = false; // Media Foundationの開始状態

    // 再生中コンテキストの管理
    // 再生中コンテキスト: IXAudio2VoiceCallback を実装し、再生終了をフラグで通知する
    struct SourceVoiceContext : public IXAudio2VoiceCallback {
        IXAudio2SourceVoice* voice = nullptr;
        std::shared_ptr<SoundClip> clip; // 再生中の音声データ寿命を保持する
        std::atomic<bool> finished { false };
        // IUnknown
        STDMETHOD(QueryInterface)(REFIID, void**) { return E_NOINTERFACE; }
        STDMETHOD_(ULONG, AddRef)() { return 1; }
        STDMETHOD_(ULONG, Release)() { return 1; }
        // IXAudio2VoiceCallback
        STDMETHOD_(void, OnVoiceProcessingPassStart)(UINT32) { }
        STDMETHOD_(void, OnVoiceProcessingPassEnd)() { }
        STDMETHOD_(void, OnStreamEnd)() { }
        STDMETHOD_(void, OnBufferStart)(void*) { }
        STDMETHOD_(void, OnBufferEnd)(void*) { finished.store(true); }
        STDMETHOD_(void, OnLoopEnd)(void*) { }
        STDMETHOD_(void, OnVoiceError)(void*, HRESULT) { finished.store(true); }
    };

    std::vector<SourceVoiceContext*> contexts_; // 再生中のサウンドのコンテキストを保持
    std::mutex contextsMutex_; // contexts_ へのアクセスを保護するためのミューテックス
};

} // namespace MyEngine
