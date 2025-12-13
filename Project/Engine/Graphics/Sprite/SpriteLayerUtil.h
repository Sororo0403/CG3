#pragma once

#include "SpriteLayer.h"

namespace SpriteLayerUtil {

/// <summary>
/// SpriteLayer を文字列に変換する
/// </summary>
inline const char *SpriteLayerToString(SpriteLayer layer) {
    switch (layer) {
    case SpriteLayer::BACKGROUND:
        return "BACKGROUND";
    case SpriteLayer::WORLD:
        return "WORLD";
    case SpriteLayer::UI:
        return "UI";
    case SpriteLayer::FADE:
        return "FADE";
    case SpriteLayer::DEBUG:
        return "DEBUG";
    default:
        return "UNKNOWN";
    }
}

/// <summary>
/// 文字列を SpriteLayer に変換する
/// </summary>
inline SpriteLayer StringToSpriteLayer(const std::string &str) {
    if (str == "BACKGROUND")
        return SpriteLayer::BACKGROUND;
    if (str == "WORLD")
        return SpriteLayer::WORLD;
    if (str == "UI")
        return SpriteLayer::UI;
    if (str == "FADE")
        return SpriteLayer::FADE;
    if (str == "DEBUG")
        return SpriteLayer::DEBUG;

    // 不正 or 未対応文字列
    return SpriteLayer::UI; // デフォルト
}

/// <summary>
/// ImGui Combo 用の名前配列
/// </summary>
inline const char *const *GetSpriteLayerNames() {
    static const char *names[] = {"BACKGROUND", "WORLD", "UI", "FADE", "DEBUG"};

    static_assert(static_cast<uint32_t>(SpriteLayer::COUNT) ==
                      sizeof(names) / sizeof(names[0]),
                  "SpriteLayer count mismatch");

    return names;
}

} // namespace SpriteLayerUtil