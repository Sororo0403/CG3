#pragma once

// ★ 必ず Windows.h を最初に
#include <Windows.h>

// ★ Media Foundation の正しい順番
#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>

// XAudio2
#include <xaudio2.h>

#include <unordered_map>
#include <vector>
#include <string>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

struct SoundData {
    WAVEFORMATEX waveFormat{};
    std::vector<BYTE> buffer;
};

class SoundManager {
  public:
    static void Initialize();
    static void Finalize();

    static SoundData *LoadSound(const std::wstring &path);
    static IXAudio2 *GetXAudio2() {
        return xAudio2;
    }

  private:
    static inline IXAudio2 *xAudio2 = nullptr;
    static inline IXAudio2MasteringVoice *masterVoice = nullptr;
    static inline std::unordered_map<std::wstring, SoundData> soundTable;
};
