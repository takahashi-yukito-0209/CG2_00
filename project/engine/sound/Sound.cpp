#include "Sound.h"
#include <Windows.h>
#include <atomic>
#include <cassert>
#include <comdef.h>
#include <fstream>
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <mftransform.h>
#include <wrl.h>

// XAudio2 と Media Foundation のライブラリ
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace MyEngine {

/// <summary>
/// コンストラクタ: Media Foundation と XAudio2 エンジンを初期化する
/// </summary>
SoundSystem::SoundSystem()
{
    // エラーコードを格納する変数
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

    // マスタリングボイスを作成
    hr = xaudio2_->CreateMasteringVoice(&masteringVoice_);
    if (FAILED(hr)) {
        masteringVoice_ = nullptr;
    }
}

/// <summary>
/// デストラクタ: XAudio2 エンジンと Media Foundation をクリーンアップする
/// </summary>
SoundSystem::~SoundSystem()
{

    if (masteringVoice_) {
        masteringVoice_->DestroyVoice();
        masteringVoice_ = nullptr;
    }
    xaudio2_.Reset();
    MFShutdown();
}

/// <summary>
/// 指定されたファイルパスからサウンドを読み込む。WAV 形式は LoadWav、MP3 などの圧縮フォーマットは LoadViaMediaFoundation を内部で呼び分け
/// </summary>
std::shared_ptr<SoundClip> SoundSystem::LoadFromFile(const std::string& path)
{
    // 拡張子判定
    auto pos = path.find_last_of('.');
    // 拡張子が .wav の場合は LoadWav を利用して読み込む
    if (pos != std::string::npos) {
        std::string ext = path.substr(pos + 1);
        for (auto& c : ext)
            c = static_cast<char>(tolower(c));
        // WAV 形式は LoadWav を利用して読み込む
        if (ext == "wav") {
            return LoadWav(path);
        }
    }
    // それ以外は Media Foundation を利用して読み込む
    std::wstring wpath(path.begin(), path.end());

    // Media Foundation を利用して読み込む
    return LoadViaMediaFoundation(wpath);
}

/// <summary>
/// 既存プロジェクトの WAV パーサを簡易的に移植した実装。RIFF ヘッダと fmt チャンク、data チャンクを読み取って SoundClip に格納して返す
/// </summary>
std::shared_ptr<SoundClip> SoundSystem::LoadWav(const std::string& path)
{
    // 既存プロジェクトの WAV パーサを簡易的に移植した実装
    struct ChunkHeader {
        char id[4];
        uint32_t size;
    };
    struct RiffHeader {
        ChunkHeader chunk;
        char type[4];
    };
    struct FormatChunk {
        ChunkHeader chunk;
        WAVEFORMATEX fmt;
    };

    std::ifstream file(path, std::ios::binary); // ファイルをバイナリモードで開く
    // ファイルが開けない場合は空のクリップを返す
    if (!file.is_open()) {
        return {};
    }

    // RIFF ヘッダを読み取る
    RiffHeader riff {};
    file.read(reinterpret_cast<char*>(&riff), sizeof(riff));
    // RIFF ヘッダの識別子とタイプを確認する。RIFF でない、WAVE タイプでない場合は対応していないフォーマットとみなして空のクリップを返す
    if (strncmp(riff.chunk.id, "RIFF", 4) != 0)
        return {};
    if (strncmp(riff.type, "WAVE", 4) != 0)
        return {};

    // fmt チャンクを読み取る
    FormatChunk format {};
    file.read(reinterpret_cast<char*>(&format), sizeof(ChunkHeader));
    // fmt チャンクの識別子を確認する。fmt チャンクでない場合は対応していないフォーマットとみなして空のクリップを返す
    if (strncmp(format.chunk.id, "fmt ", 4) != 0)
        return {};
    if (format.chunk.size > sizeof(format.fmt))
        return {};
    // fmt チャンクのサイズを確認する。WAVEFORMATEX より大きい場合は対応していないフォーマットとみなして空のクリップを返す
    file.read(reinterpret_cast<char*>(&format.fmt), format.chunk.size);

    // data チャンクを読み取る
    ChunkHeader data {};
    // fmt チャンクの後に data チャンクが来るとは限らないため、data チャンクが見つかるまでチャンクヘッダを読み飛ばす
    file.read(reinterpret_cast<char*>(&data), sizeof(data));
    if (strncmp(data.id, "JUNK", 4) == 0) {
        file.seekg(data.size, std::ios::cur);
        file.read(reinterpret_cast<char*>(&data), sizeof(data));
    }
    // data チャンクでない場合は対応していないフォーマットとみなして空のクリップを返す
    if (strncmp(data.id, "data", 4) != 0)
        return {};

    auto clip = std::make_shared<SoundClip>();
    // data チャンクのサイズ分だけバッファを確保して読み取る
    clip->buffer.resize(data.size);
    file.read(reinterpret_cast<char*>(clip->buffer.data()), data.size);
    clip->wfex = format.fmt;

    // 読み取りに失敗している場合は空のクリップを返す
    return clip;
}

/// <summary>
/// Media Foundation を利用して MP3 などの圧縮フォーマットのサウンドを読み込む。WAVEFORMATEX と PCM バッファを SoundClip に格納して返す
/// </summary>
std::shared_ptr<SoundClip> SoundSystem::LoadViaMediaFoundation(const std::wstring& wpath)
{

    // 変数の宣言と初期化
    HRESULT hr = S_OK;
    Microsoft::WRL::ComPtr<IMFSourceReader> reader;

    // ソースリーダーを作成する
    hr = MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &reader);
    // ソースリーダーの作成に失敗した場合は空のクリップを返す
    if (FAILED(hr)) {
        return {};
    }

    // 出力メディアタイプを PCM に設定してリーダーに渡す
    Microsoft::WRL::ComPtr<IMFMediaType> pTypeOut;
    hr = MFCreateMediaType(&pTypeOut);
    // メディアタイプの作成に失敗した場合は空のクリップを返す
    if (FAILED(hr))
        return {};

    // メディアタイプにオーディオの主要タイプと PCM サブタイプを設定する。
    // これにより、リーダーは内部で必要なデコード処理を行い、PCM フォーマットでサンプルを提供するようになる
    hr = pTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(hr))
        return {};
    hr = pTypeOut->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (FAILED(hr))
        return {};

    // 設定したメディアタイプをリーダーに渡す
    hr = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pTypeOut.Get());
    if (FAILED(hr))
        return {};

    // 現在のメディアタイプから WAVEFORMATEX を取得する
    Microsoft::WRL::ComPtr<IMFMediaType> mt;
    hr = reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &mt);
    if (FAILED(hr))
        return {};

    // WAVEFORMATEX を取得するためのバッファを確保して、Media Type からフォーマット情報を抽出する
    WAVEFORMATEX* pwfx = nullptr;
    UINT32 cbwfx = 0;
    hr = MFCreateWaveFormatExFromMFMediaType(mt.Get(), &pwfx, &cbwfx);
    if (FAILED(hr))
        return {};

    auto clip = std::make_shared<SoundClip>();
    // 取得したフォーマット情報をコピー
    clip->wfex = *pwfx;

    // サンプルを順次読み込み、バッファへ連結する
    while (true) {
        Microsoft::WRL::ComPtr<IMFSample> sample;
        DWORD flags = 0;
        hr = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample);
        // サンプルの読み込みに失敗した場合はループを抜ける
        if (FAILED(hr)) {
            break;
        }

        // ストリームの終端に達した場合はループを抜ける
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }

        // サンプルが空の場合は次のサンプルの読み込みに進む
        if (!sample) {
            continue;
        }

        // サンプルからバッファを取得する
        // サンプルは複数のバッファを持つことができるが、ConvertToContiguousBuffer を呼び出すことで、
        // すべてのバッファを連結した単一のバッファを取得する
        Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (FAILED(hr)) {
            break;
        }

        // バッファをロックして PCM データを取得する
        BYTE* pData = nullptr;
        DWORD maxLen = 0;
        DWORD curLen = 0;
        hr = buffer->Lock(&pData, &maxLen, &curLen);
        // ロックに成功した場合は、取得したデータを SoundClip のバッファに連結して格納する
        if (SUCCEEDED(hr)) {
            size_t old = clip->buffer.size();
            clip->buffer.resize(old + curLen);
            memcpy(clip->buffer.data() + old, pData, curLen);
            buffer->Unlock();
        }
    }

    // 取得した WAVEFORMATEX のバッファを解放する
    if (pwfx) {
        CoTaskMemFree(pwfx);
    }

    // 読み取りに失敗している場合は空のクリップを返す
    return clip;
}

/// <summary>
/// 指定されたサウンドクリップを再生する。内部で IXAudio2SourceVoice を作成し、サウンドクリップのバッファを送信して再生開始する
/// </summary>
void SoundSystem::Play(const std::shared_ptr<SoundClip>& clip)
{
    // クリップが有効でない場合は再生しない
    if (!clip || !clip->IsValid() || !xaudio2_) {
        return;
    }

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

    // コンテキストにボイスを保持する。再生終了時に DestroyVoice() するため
    ctx->voice = pSourceVoice;

    // contexts_ に追加して Poll() でクリーンナップする
    {
        std::lock_guard<std::mutex> lk(contextsMutex_);
        contexts_.push_back(ctx);
    }

    XAUDIO2_BUFFER buf {};
    buf.pAudioData = clip->GetData();
    buf.AudioBytes = clip->GetSize();
    buf.Flags = XAUDIO2_END_OF_STREAM;

    // ボイスにバッファを送信して再生開始する
    pSourceVoice->SubmitSourceBuffer(&buf);
    pSourceVoice->Start();
}

/// <summary>
/// 再生中のサウンドの状態を更新する。再生終了したサウンドのコンテキストをクリーンアップするために、ゲームループの適切なタイミングで呼び出す必要がある
/// </summary>
void SoundSystem::Poll()
{
    // contexts_ へのアクセスを保護するためにミューテックスをロックする
    std::lock_guard<std::mutex> lk(contextsMutex_);
    // contexts_ をイテレートして、finished フラグが立っているコンテキストをクリーンアップする
    for (auto it = contexts_.begin(); it != contexts_.end();) {
        auto ctx = *it;
        // finished フラグが立っている場合は、再生が完了しているとみなしてクリーンアップする
        if (ctx->finished.load()) {
            // ボイスを DestroyVoice() してコンテキストを削除する
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