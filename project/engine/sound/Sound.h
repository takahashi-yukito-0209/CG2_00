// サウンド管理クラス群のヘッダ
#pragma once

#include <Windows.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <memory>
#include <xaudio2.h>
#include <atomic>
#include <mutex>
#include <mutex>


namespace MyEngine {

// 読み込んだ音声データを保持するクラス
class SoundClip {
public:
    // 波形フォーマット情報
    WAVEFORMATEX wfex{};
    // PCM 等の音声バイトバッファ
    std::vector<uint8_t> buffer;
    // バッファの生ポインタを取得
    const uint8_t* GetData() const { return buffer.empty() ? nullptr : buffer.data(); }
    // バッファサイズ取得
    uint32_t GetSize() const { return static_cast<uint32_t>(buffer.size()); }
    // 有効なクリップかどうかを判定
    bool IsValid() const { return !buffer.empty() && wfex.nChannels > 0 && wfex.nSamplesPerSec > 0; }
};

// サウンドシステム (読み込みと再生の責務を分離)
class SoundSystem {
public:
    // コンストラクタで Media Foundation と XAudio2 を初期化する
    SoundSystem();
    // デストラクタでリソースを解放する
    ~SoundSystem();

    // ファイルからサウンドを読み込む（拡張子で WAV は簡易パーサ、それ以外は Media Foundation を利用）
    std::shared_ptr<SoundClip> LoadFromFile(const std::string& path);

    // 読み込んだサウンドを再生する
    void Play(const std::shared_ptr<SoundClip>& clip);
    // 毎フレーム呼び出して、再生終了したボイスのクリーンナップを行う
    void Poll();

private:
    // WAV の簡易読み込み（既存実装との互換）
    std::shared_ptr<SoundClip> LoadWav(const std::string& path);
    // Media Foundation を利用した読み込み（MP3 等の圧縮フォーマット対応）
    std::shared_ptr<SoundClip> LoadViaMediaFoundation(const std::wstring& wpath);

    // XAudio2 エンジン
    Microsoft::WRL::ComPtr<IXAudio2> xaudio2_;
    IXAudio2MasteringVoice* masteringVoice_ = nullptr;
    // 再生中コンテキストの管理
    // 再生中コンテキスト: IXAudio2VoiceCallback を実装し、再生終了をフラグで通知する
    struct SourceVoiceContext : public IXAudio2VoiceCallback {
        IXAudio2SourceVoice* voice = nullptr;
        std::atomic<bool> finished{false};
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

    std::vector<SourceVoiceContext*> contexts_;
    std::mutex contextsMutex_;
};

} // namespace MyEngine
