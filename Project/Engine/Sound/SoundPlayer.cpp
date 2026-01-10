#include "SoundPlayer.h"
#include <cassert>

void SoundPlayer::Play(SoundData *sound, bool loop) {
    IXAudio2SourceVoice *sourceVoice = nullptr;

    HRESULT hr = SoundManager::GetXAudio2()->CreateSourceVoice(
        &sourceVoice, &sound->waveFormat);
    assert(SUCCEEDED(hr));

    XAUDIO2_BUFFER buffer{};
    buffer.pAudioData = sound->buffer.data();
    buffer.AudioBytes = static_cast<UINT32>(sound->buffer.size());
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

    sourceVoice->SubmitSourceBuffer(&buffer);
    sourceVoice->Start();
}
