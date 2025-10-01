// IScene.h
#pragma once
#include "EngineContext.h"

class IScene {
public:
	virtual ~IScene() = default;
	virtual void Initialize(const EngineContext &engineContext) = 0;
	virtual void Update(float dt) = 0;                    
	virtual void Draw(const EngineContext &engineContext, const RenderContext &renderContext) = 0;      
