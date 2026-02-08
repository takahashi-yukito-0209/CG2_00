#include "Sound.h"
#include <Windows.h>
#include <fstream>
#include <cassert>
#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mftransform.h>
#include <initguid.h>
#include <wrl.h>
#include <comdef.h>
#include <atomic>

// XAudio2 と Media Foundation のライブラリ
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace MyEngine {

// コンストラクタ: Media Foundation と XAudio2 を初期化する
SoundSystem::SoundSystem() {
    HRESULT hr = S_OK;
    // Media Foundation を初期化（MP3 等の圧縮フォーマット対応のため）
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        // 初期化に失敗した場合はログ等で通知するがアプリ継続を優先する
    }

    // XAudio2 を初期化
    hr = XAudio2Create(&xaudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        xaudio2_.Reset();
        return;
    }

    hr = xaudio2_->CreateMasteringVoice(&masteringVoice_);
    if (FAILED(hr)) {
        masteringVoice_ = nullptr;
    }
}

// デストラクタ: 初期化したリソースを解放する
SoundSystem::~SoundSystem() {
    if (masteringVoice_) {
        masteringVoice_->DestroyVoice();
        masteringVoice_ = nullptr;
    }
    xaudio2_.Reset();
    MFShutdown();
}

std::shared_ptr<SoundClip> SoundSystem::LoadFromFile(const std::string& path) {
    // 拡張子判定
    auto pos = path.find_last_of('.') ;
    if (pos != std::string::npos) {
        std::string ext = path.substr(pos + 1);
        for (auto &c : ext) c = static_cast<char>(tolower(c));
        if (ext == "wav") {
            return LoadWav(path);
        }
    }
    // それ以外は Media Foundation を利用して読み込む
    std::wstring wpath(path.begin(), path.end());
    return LoadViaMediaFoundation(wpath);
}

// WAV の簡易読み込み
std::shared_ptr<SoundClip> SoundSystem::LoadWav(const std::string& path) {
    // 既存プロジェクトの WAV パーサを簡易的に移植した実装
    struct ChunkHeader { char id[4]; uint32_t size; };
    struct RiffHeader { ChunkHeader chunk; char type[4]; };
    struct FormatChunk { ChunkHeader chunk; WAVEFORMATEX fmt; };

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    RiffHeader riff{};
    file.read(reinterpret_cast<char*>(&riff), sizeof(riff));
    if (strncmp(riff.chunk.id, "RIFF", 4) != 0) return {};
    if (strncmp(riff.type, "WAVE", 4) != 0) return {};

    FormatChunk format{};
    file.read(reinterpret_cast<char*>(&format), sizeof(ChunkHeader));
    if (strncmp(format.chunk.id, "fmt ", 4) != 0) return {};
    if (format.chunk.size > sizeof(format.fmt)) return {};
    file.read(reinterpret_cast<char*>(&format.fmt), format.chunk.size);

    ChunkHeader data{};
    file.read(reinterpret_cast<char*>(&data), sizeof(data));
    if (strncmp(data.id, "JUNK", 4) == 0) {
        file.seekg(data.size, std::ios::cur);
        file.read(reinterpret_cast<char*>(&data), sizeof(data));
    }
    if (strncmp(data.id, "data", 4) != 0) return {};

    auto clip = std::make_shared<SoundClip>();
    clip->buffer.resize(data.size);
    file.read(reinterpret_cast<char*>(clip->buffer.data()), data.size);
    clip->wfex = format.fmt;

    return clip;
}

// Media Foundation を使って圧縮フォーマットに対応した読み込み
std::shared_ptr<SoundClip> SoundSystem::LoadViaMediaFoundation(const std::wstring& wpath) {
    HRESULT hr = S_OK;
    Microsoft::WRL::ComPtr<IMFSourceReader> reader;

    // ソースリーダーを作成する
    hr = MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &reader);
    if (FAILED(hr)) {
        return {};
    }

    // 出力メディアタイプを PCM に設定してリーダーに渡す
    Microsoft::WRL::ComPtr<IMFMediaType> pTypeOut;
    hr = MFCreateMediaType(&pTypeOut);
    if (FAILED(hr)) return {};

    hr = pTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(hr)) return {};
    hr = pTypeOut->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (FAILED(hr)) return {};

    hr = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pTypeOut.Get());
    if (FAILED(hr)) return {};

    // 現在のメディアタイプから WAVEFORMATEX を取得する
    Microsoft::WRL::ComPtr<IMFMediaType> mt;
    hr = reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &mt);
    if (FAILED(hr)) return {};

    WAVEFORMATEX* pwfx = nullptr;
    UINT32 cbwfx = 0;
    hr = MFCreateWaveFormatExFromMFMediaType(mt.Get(), &pwfx, &cbwfx);
    if (FAILED(hr)) return {};

    auto clip = std::make_shared<SoundClip>();
    // 取得したフォーマット情報をコピー
    clip->wfex = *pwfx;

    // サンプルを順次読み込み、バッファへ連結する
    while (true) {
        Microsoft::WRL::ComPtr<IMFSample> sample;
        DWORD flags = 0;
        hr = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample);
        if (FAILED(hr)) break;
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        if (!sample) continue;

        Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (FAILED(hr)) break;

        BYTE* pData = nullptr;
        DWORD maxLen = 0;
        DWORD curLen = 0;
        hr = buffer->Lock(&pData, &maxLen, &curLen);
        if (SUCCEEDED(hr)) {
            size_t old = clip->buffer.size();
            clip->buffer.resize(old + curLen);
            memcpy(clip->buffer.data() + old, pData, curLen);
            buffer->Unlock();
        }
    }

    if (pwfx) CoTaskMemFree(pwfx);

    return clip;
}

void SoundSystem::Play(const std::shared_ptr<SoundClip>& clip) {
    if (!clip || !clip->IsValid() || !xaudio2_) return;

    // 再生用のコンテキストを生成し、再生終了時に自身でボイスを破棄する仕組みを使う
    // IXAudio2VoiceCallback を実装した簡易コンテキストを動的確保し、
    // バッファ終端でボイスを DestroyVoice() して自分自身を delete する。
    // ヘッダで定義済みの SoundSystem::SourceVoiceContext を使う
    auto ctx = new SoundSystem::SourceVoiceContext();

    IXAudio2SourceVoice* pSourceVoice = nullptr;
    // コールバックとして ctx を渡すことで、再生完了時に自動で破棄される
    HRESULT hr = xaudio2_->CreateSourceVoice(&pSourceVoice, reinterpret_cast<WAVEFORMATEX*>(&clip->wfex), 0, XAUDIO2_DEFAULT_FREQ_RATIO, ctx);
    if (FAILED(hr) || !pSourceVoice) {
        delete ctx;
        return;
    }

    ctx->voice = pSourceVoice;

    // contexts_ に追加して Poll() でクリーンナップする
    {
        std::lock_guard<std::mutex> lk(contextsMutex_);
        contexts_.push_back(ctx);
    }

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = clip->GetData();
    buf.AudioBytes = clip->GetSize();
    buf.Flags = XAUDIO2_END_OF_STREAM;

    pSourceVoice->SubmitSourceBuffer(&buf);
    pSourceVoice->Start();
}

// 再生終了したボイスの破棄をポーリングで行う
void SoundSystem::Poll() {
    std::lock_guard<std::mutex> lk(contextsMutex_);
    for (auto it = contexts_.begin(); it != contexts_.end();) {
        auto ctx = *it;
        if (ctx->finished.load()) {
            if (ctx->voice) {
                ctx->voice->DestroyVoice();
                ctx->voice = nullptr;
            }
            delete ctx;
            it = contexts_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace MyEngine