#pragma once
#include "SoundManager.h"

class SoundPlayer {
  public:
    static void Play(SoundData *sound, bool loop = false);
};
