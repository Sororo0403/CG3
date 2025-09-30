#include "SpriteCommon.h"
#include <cassert>
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

namespace {
/// <summary>
/// HLSL シェーダをコンパイルするユーティリティ関数。
/// </summary>
/// <param name="filePath">シェーダファイルのパス</param>
/// <param name="profile">シェーダプロファイル (例: vs_6_0, ps_6_0)</param>
/// <param name="dxcUtils">DXC ユーティリティ</param>
/// <param name="dxcCompiler">DXC コンパイラ</param>
/// <param name="includeHandler">インクルードハンドラ</param>
/// <returns>コンパイル済みシェーダのバイナリ Blob</returns>
static ComPtr<IDxcBlob> CompileShader(const std::wstring &filePath,
                                      const wchar_t *profile,
                                      IDxcUtils *dxcUtils,
                                      IDxcCompiler3 *dxcCompiler,
                                      IDxcIncludeHandler *includeHandler) {
  assert(dxcUtils && dxcCompiler && includeHandler);

  // シェーダファイルをロード
  ComPtr<IDxcBlobEncoding> source;
  HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &source);
  if (FAILED(hr)) {
    OutputDebugStringW((L"[DXC] LoadFile failed: " + filePath + L"\n").c_str());
    assert(false);
    return {};
  }

  // DxcBuffer に変換
  DxcBuffer buffer{};
  buffer.Ptr = source->GetBufferPointer();
  buffer.Size = source->GetBufferSize();
  buffer.Encoding = DXC_CP_UTF8;

  // コンパイル引数
  LPCWSTR args[] = {
      filePath.c_str(), // 入力ファイル名（仮想パスでもOK）
      L"-E",
      L"main", // エントリーポイント
      L"-T",
      profile,          // シェーダプロファイル
      L"-Zi",           // デバッグ情報
      L"-Qembed_debug", // PDB を埋め込み
      L"-Od",           // 最適化無効（開発用）
      L"-Zpr"           // 行優先レイアウト
  };

  // コンパイル実行
  ComPtr<IDxcResult> result;
  hr = dxcCompiler->Compile(&buffer, args, _countof(args), includeHandler,
                            IID_PPV_ARGS(&result));
  if (FAILED(hr) || result == nullptr) {
    OutputDebugStringW(L"[DXC] Compile failed: result is null\n");
    assert(false);
    return {};
  }

  // エラーチェック
  ComPtr<IDxcBlobUtf8> errors;
  ComPtr<IDxcBlobWide> errorsName;
  (void)result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors),
                          errorsName.ReleaseAndGetAddressOf());
  if (errors && errors->GetStringLength() > 0) {
    OutputDebugStringA(errors->GetStringPointer());
    assert(false);
    return {};
  }

  // コンパイル結果取得
  ComPtr<IDxcBlob> shader;
  ComPtr<IDxcBlobWide> shaderName;
  hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader),
                         shaderName.ReleaseAndGetAddressOf());
  if (FAILED(hr) || !shader) {
    OutputDebugStringW(L"[DXC] GetOutput(DXC_OUT_OBJECT) failed\n");
    assert(false);
    return {};
  }

  return shader;
}
} // namespace

// ===============================
// Public
// ===============================
void SpriteCommon::Initialize(ID3D12Device *device) {
  assert(device);
  device_ = device; // ★ Device を保持
  CreateRootSignature(device_);
}

void SpriteCommon::CreateGraphicsPipeline(IDxcUtils *dxcUtils,
                                          IDxcCompiler3 *dxcCompiler,
                                          IDxcIncludeHandler *includeHandler,
                                          const std::wstring &vsPath,
                                          const std::wstring &psPath,
                                          const PipelineFormats &formats) {
  assert(device_ && rootSignature_);
  assert(dxcUtils && dxcCompiler && includeHandler);

  ComPtr<IDxcBlob> vs =
      CompileShader(vsPath, L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
  ComPtr<IDxcBlob> ps =
      CompileShader(psPath, L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
  assert(vs && ps);

  // 入力レイアウト（スプライト想定：POSITION(float3) + TEXCOORD(float2)）
  D3D12_INPUT_ELEMENT_DESC elems[2] = {};
  elems[0].SemanticName = "POSITION";
  elems[0].SemanticIndex = 0;
  elems[0].Format = DXGI_FORMAT_R32G32B32_FLOAT; // float3
  elems[0].InputSlot = 0;
  elems[0].AlignedByteOffset = 0;
  elems[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  elems[0].InstanceDataStepRate = 0;

  elems[1].SemanticName = "TEXCOORD";
  elems[1].SemanticIndex = 0;
  elems[1].Format = DXGI_FORMAT_R32G32_FLOAT; // float2
  elems[1].InputSlot = 0;
  elems[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  elems[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  elems[1].InstanceDataStepRate = 0;

  D3D12_INPUT_LAYOUT_DESC il{};
  il.pInputElementDescs = elems;
  il.NumElements = _countof(elems);

  // ブレンド（必要に応じてアルファブレンドを後で有効化）
  D3D12_BLEND_DESC blend{};
  blend.AlphaToCoverageEnable = FALSE;
  blend.IndependentBlendEnable = FALSE;
  blend.RenderTarget[0].BlendEnable = FALSE;
  blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  // ラスタライザ
  D3D12_RASTERIZER_DESC rast{};
  rast.FillMode = D3D12_FILL_MODE_SOLID;
  rast.CullMode = D3D12_CULL_MODE_BACK;
  rast.FrontCounterClockwise = FALSE;
  rast.DepthClipEnable = TRUE;

  // 深度ステンシル（DSV 未使用なら無効化）
  D3D12_DEPTH_STENCIL_DESC ds{};
  if (formats.dsvFormat == DXGI_FORMAT_UNKNOWN) {
    ds.DepthEnable = FALSE;
    ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    ds.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    ds.StencilEnable = FALSE;
  } else {
    ds.DepthEnable = TRUE;
    ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    ds.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    ds.StencilEnable = FALSE;
  }

  // PSO 記述子
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
  pso.pRootSignature = rootSignature_.Get();
  pso.InputLayout = il;
  pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
  pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
  pso.BlendState = blend;
  pso.RasterizerState = rast;
  pso.DepthStencilState = ds;
  pso.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
  pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pso.NumRenderTargets = formats.numRenderTargets;
  for (UINT i = 0; i < formats.numRenderTargets; ++i) {
    pso.RTVFormats[i] = formats.rtvFormat; // スワップチェーンのRTVと一致させる
  }
  pso.DSVFormat = formats.dsvFormat; // 未使用なら DXGI_FORMAT_UNKNOWN
  pso.SampleDesc.Count = 1;

  // PSO 作成（★ device_ を使用する。RootSignature から Device は取れない）
  HRESULT hr = device_->CreateGraphicsPipelineState(
      &pso, IID_PPV_ARGS(pipelineState_.ReleaseAndGetAddressOf()));
  if (FAILED(hr)) {
    OutputDebugStringW(L"[D3D12] CreateGraphicsPipelineState failed\n");
    assert(false);
  }
}

void SpriteCommon::ApplyCommonDrawSettings(
    ID3D12GraphicsCommandList *cmd, D3D12_PRIMITIVE_TOPOLOGY topology) const {
  assert(cmd);
  assert(rootSignature_);
  assert(pipelineState_); // PSO 未作成だとここで止めて事故防止
  cmd->SetGraphicsRootSignature(rootSignature_.Get());
  cmd->SetPipelineState(pipelineState_.Get());
  cmd->IASetPrimitiveTopology(topology);
}

// ===============================
// Private
// ===============================
void SpriteCommon::CreateRootSignature(ID3D12Device *device) {
  // SRV (t0) のレンジ
  D3D12_DESCRIPTOR_RANGE srvRange{};
  srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvRange.BaseShaderRegister = 0;
  srvRange.NumDescriptors = 1;
  srvRange.OffsetInDescriptorsFromTableStart =
      D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // サンプラ (s0)
  D3D12_STATIC_SAMPLER_DESC sampler{};
  sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  sampler.MipLODBias = 0.0f;
  sampler.MaxAnisotropy = 1;
  sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  sampler.MinLOD = 0.0f;
  sampler.MaxLOD = D3D12_FLOAT32_MAX;
  sampler.ShaderRegister = 0;
  sampler.RegisterSpace = 0;
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  // ルートパラメータ
  // p0: PS の CBV(b0)
  // p1: VS の CBV(b0)
  // p2: PS の SRV(t0) テーブル
  // p3: PS の CBV(b1)（必要なら）
  D3D12_ROOT_PARAMETER params[4]{};

  params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  params[0].Descriptor.ShaderRegister = 0;

  params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  params[1].Descriptor.ShaderRegister = 0;

  params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  params[2].DescriptorTable.NumDescriptorRanges = 1;
  params[2].DescriptorTable.pDescriptorRanges = &srvRange;

  params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  params[3].Descriptor.ShaderRegister = 1;

  // ルートシグネチャ記述
  D3D12_ROOT_SIGNATURE_DESC desc{};
  desc.NumParameters = _countof(params);
  desc.pParameters = params;
  desc.NumStaticSamplers = 1;
  desc.pStaticSamplers = &sampler;
  desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  // シリアライズと生成
  ComPtr<ID3DBlob> sig, err;
  HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           &sig, &err);
  if (FAILED(hr)) {
    if (err)
      OutputDebugStringA((char *)err->GetBufferPointer());
    assert(false);
  }

  hr = device->CreateRootSignature(
      0, sig->GetBufferPointer(), sig->GetBufferSize(),
      IID_PPV_ARGS(rootSignature_.ReleaseAndGetAddressOf()));
  if (FAILED(hr)) {
    OutputDebugStringA("[D3D12] CreateRootSignature failed\n");
    assert(false);
  }
}
