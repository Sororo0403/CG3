#pragma once

#include <d3d12.h>

class ShaderCompiler;
class ModelRenderer;

/// <summary>
/// 描画コンテキスト。
/// </summary>
struct RenderContext {
    ID3D12GraphicsCommandList *commandList = nullptr;
	ShaderCompiler *shaderCompiler = nullptr;
	ModelRenderer *modelRenderer = nullptr;
};
