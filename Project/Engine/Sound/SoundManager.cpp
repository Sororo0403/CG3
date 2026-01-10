#include "SoundManager.h"
#include <cassert>

void SoundManager::Initialize() {
    MFStartup(MF_VERSION);

    HRESULT hr = XAudio2Create(&xAudio2, 0);
    assert(SUCCEEDED(hr));

    hr = xAudio2->CreateMasteringVoice(&masterVoice);
    assert(SUCCEEDED(hr));
}

void SoundManager::Finalize() {
    soundTable.clear();

    if (masterVoice) {
        masterVoice->DestroyVoice();
        masterVoice = nullptr;
    }

    if (xAudio2) {
        xAudio2->Release();
        xAudio2 = nullptr;
    }

    MFShutdown();
}

SoundData *SoundManager::LoadSound(const std::wstring &path) {
    if (soundTable.contains(path)) {
        return &soundTable[path];
    }

    IMFSourceReader *reader = nullptr;
    MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader);

    IMFMediaType *mediaType = nullptr;
    MFCreateMediaType(&mediaType);
    mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    mediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);

    reader->SetCurrentMediaType(
        static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr,
        mediaType);

    SoundData sound{};
    std::vector<BYTE> pcmData;

    while (true) {
        IMFSample *sample = nullptr;
        DWORD flags = 0;

reader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr,
            &flags, nullptr, &sample);


        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }

        IMFMediaBuffer *buffer = nullptr;
        sample->ConvertToContiguousBuffer(&buffer);

        BYTE *data = nullptr;
        DWORD size = 0;
        buffer->Lock(&data, nullptr, &size);
        pcmData.insert(pcmData.end(), data, data + size);
        buffer->Unlock();

        buffer->Release();
        sample->Release();
    }

    IMFMediaType *outType = nullptr;
    reader->GetCurrentMediaType(
        static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &outType);

    UINT32 sampleRate = 0, channels = 0, bits = 0;
    outType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
    outType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
    outType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);

    sound.waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    sound.waveFormat.nChannels = (WORD) channels;
    sound.waveFormat.nSamplesPerSec = sampleRate;
    sound.waveFormat.wBitsPerSample = (WORD) bits;
    sound.waveFormat.nBlockAlign =
        static_cast<WORD>(sound.waveFormat.nChannels * bits / 8);
    sound.waveFormat.nAvgBytesPerSec =
        sound.waveFormat.nBlockAlign * sampleRate;

    sound.buffer = std::move(pcmData);
    soundTable[path] = sound;

    outType->Release();
    mediaType->Release();
    reader->Release();

    return &soundTable[path];
}
