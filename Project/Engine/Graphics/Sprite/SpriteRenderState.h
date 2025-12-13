#pragma once

#include "SpriteLayer.h"

struct SpriteRenderState {
    SpriteLayer layer = SpriteLayer::UI;
    bool visible = true;
};
