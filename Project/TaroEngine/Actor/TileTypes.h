#pragma once
#include <cstdint>

/// <summary>タイル種類（Novice版と対応）</summary>
enum class Tile : int32_t {
    Empty = 0,
    Solid,
    FragileAny,
    FragileTop,
    FragileBottom,
    Spring,
    Spike,
    JumpOnly,       // 互換: 普通の床扱い
    Regen,          // 復活床
    Switch,
    SwitchBlockOn,
    SwitchBlockOff,
};

struct FragileState {
    bool  armed = false;
    float t = 0.0f;
    bool  gone = false;
};
struct RegenState {
    float respawn = 0.0f;
};
