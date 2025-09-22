#define _USE_MATH_DEFINES
#include <cmath>
#define DIRECTINPUT_VERSION 0x0800// DirectInputのバージョン指定
#include <dinput.h>
#include <windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <fstream>
#include <sstream>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <xaudio2.h>
#include <wrl.h>
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/DirectXTex/DirectXTex.h"
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "directxtex.lib")

using namespace Microsoft::WRL;

struct Vector2 {
    float x;
    float y;
};

struct Vector3 {
    float x;
    float y;
    float z;
};

struct Vector4 {
    float x;
    float y;
    float z;
    float w;
};

struct Matrix3x3 {
    float m[3][3];
};

struct Matrix4x4 {
    float m[4][4];
};

struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};

struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

struct Material {
    Vector4 color;
    int32_t enableLighting;
    float padding[3];
    Matrix4x4 uvTransform;
};

struct MaterialData {
    std::string textureFilePath;
};

struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
};

struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
    int mode;  // 0=NoLighting, 1=Lambert, 2=HalfLambert
    float padding[3];
};


struct ModelData {
    std::vector<VertexData> vertices;
    MaterialData material;
};

struct SphereMesh {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
};

struct ChunkHeader {
    char id[4]; // チャンクのID
    int32_t size; // チャンクサイズ
};

struct RiffHeader {
    ChunkHeader chunk;//    "RIFF"
    char type[4]; // "WAVE"
};

struct FormatChunk {
    ChunkHeader chunk;    // fot
    WAVEFORMATEX fmt; // 波形フォーマット
};

// 音声データ
struct SoundData {
    // 波形フォーマット
    WAVEFORMATEX wfex;
    // バッファの先頭アドレス
    BYTE* pBuffer;
    // バッファのサイズ
    unsigned int bufferSize;
};

Matrix4x4 MakeIdentity4x4() {
    Matrix4x4 mat{};
    for (int i = 0; i < 4; ++i) {
        mat.m[i][i] = 1.0f;
    }
    return mat;
}

Matrix3x3 MakeIdentity3x3() {
    Matrix3x3 mat{};
    for (int i = 0; i < 3; ++i) {
        mat.m[i][i] = 1.0f;
    }
    return mat;
}

// 行列の掛け算
Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b) {
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] =
                a.m[i][0] * b.m[0][j] +
                a.m[i][1] * b.m[1][j] +
                a.m[i][2] * b.m[2][j] +
                a.m[i][3] * b.m[3][j];
        }
    }
    return result;
}

Matrix4x4 MakeAffineMatrix(Vector3 scale, Vector3 rotation, Vector3 translate) {
    Matrix4x4 mat{};

    // 回転角を事前に計算（ラジアン）
    float cx = cosf(rotation.x);
    float sx = sinf(rotation.x);
    float cy = cosf(rotation.y);
    float sy = sinf(rotation.y);
    float cz = cosf(rotation.z);
    float sz = sinf(rotation.z);

    // 回転行列 (Z → Y → X の順)
    float r00 = cy * cz;
    float r01 = cz * sx * sy - cx * sz;
    float r02 = sx * sz + cx * cz * sy;

    float r10 = cy * sz;
    float r11 = cx * cz + sx * sy * sz;
    float r12 = cx * sy * sz - cz * sx;

    float r20 = -sy;
    float r21 = cy * sx;
    float r22 = cx * cy;

    // スケールを適用
    r00 *= scale.x; r01 *= scale.y; r02 *= scale.z;
    r10 *= scale.x; r11 *= scale.y; r12 *= scale.z;
    r20 *= scale.x; r21 *= scale.y; r22 *= scale.z;

    // アフィン行列を設定
    mat.m[0][0] = r00; mat.m[0][1] = r01; mat.m[0][2] = r02; mat.m[0][3] = 0.0f;
    mat.m[1][0] = r10; mat.m[1][1] = r11; mat.m[1][2] = r12; mat.m[1][3] = 0.0f;
    mat.m[2][0] = r20; mat.m[2][1] = r21; mat.m[2][2] = r22; mat.m[2][3] = 0.0f;
    mat.m[3][0] = translate.x;
    mat.m[3][1] = translate.y;
    mat.m[3][2] = translate.z;
    mat.m[3][3] = 1.0f;

    return mat;
}

Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearZ, float farZ) {
    Matrix4x4 mat{};
    float f = 1.0f / std::tan(fovY * 0.5f);

    mat.m[0][0] = f / aspectRatio;
    mat.m[0][1] = 0.0f;
    mat.m[0][2] = 0.0f;
    mat.m[0][3] = 0.0f;

    mat.m[1][0] = 0.0f;
    mat.m[1][1] = f;
    mat.m[1][2] = 0.0f;
    mat.m[1][3] = 0.0f;

    mat.m[2][0] = 0.0f;
    mat.m[2][1] = 0.0f;
    mat.m[2][2] = farZ / (farZ - nearZ);
    mat.m[2][3] = 1.0f;

    mat.m[3][0] = 0.0f;
    mat.m[3][1] = 0.0f;
    mat.m[3][2] = (-nearZ * farZ) / (farZ - nearZ);
    mat.m[3][3] = 0.0f;

    return mat;
}

// アフィン行列(回転+平行移動+スケール)の逆行列
Matrix4x4 Inverse(const Matrix4x4& m) {
    Matrix4x4 result{};

    // 回転・スケール部分(上3x3行列)の逆行列
    float det =
        m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) -
        m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0]) +
        m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]);

    assert(fabs(det) > 1e-8f); // 逆行列が存在しない場合を防ぐ
    float invDet = 1.0f / det;

    // 3x3部分の逆行列を計算
    result.m[0][0] = (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) * invDet;
    result.m[0][1] = -(m.m[0][1] * m.m[2][2] - m.m[0][2] * m.m[2][1]) * invDet;
    result.m[0][2] = (m.m[0][1] * m.m[1][2] - m.m[0][2] * m.m[1][1]) * invDet;

    result.m[1][0] = -(m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0]) * invDet;
    result.m[1][1] = (m.m[0][0] * m.m[2][2] - m.m[0][2] * m.m[2][0]) * invDet;
    result.m[1][2] = -(m.m[0][0] * m.m[1][2] - m.m[0][2] * m.m[1][0]) * invDet;

    result.m[2][0] = (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]) * invDet;
    result.m[2][1] = -(m.m[0][0] * m.m[2][1] - m.m[0][1] * m.m[2][0]) * invDet;
    result.m[2][2] = (m.m[0][0] * m.m[1][1] - m.m[0][1] * m.m[1][0]) * invDet;

    // 平行移動の逆変換
    result.m[3][0] = -(m.m[3][0] * result.m[0][0] + m.m[3][1] * result.m[1][0] + m.m[3][2] * result.m[2][0]);
    result.m[3][1] = -(m.m[3][0] * result.m[0][1] + m.m[3][1] * result.m[1][1] + m.m[3][2] * result.m[2][1]);
    result.m[3][2] = -(m.m[3][0] * result.m[0][2] + m.m[3][1] * result.m[1][2] + m.m[3][2] * result.m[2][2]);

    // 下段
    result.m[0][3] = 0.0f;
    result.m[1][3] = 0.0f;
    result.m[2][3] = 0.0f;
    result.m[3][3] = 1.0f;

    return result;
}

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
    Matrix4x4 mat{};

    float width = right - left;
    float height = bottom - top;
    float depth = farClip - nearClip;

    // X方向スケーリング
    mat.m[0][0] = 2.0f / width;
    mat.m[0][1] = 0.0f;
    mat.m[0][2] = 0.0f;
    mat.m[0][3] = 0.0f;

    // Y方向スケーリング (上下反転対応: マイナスを付ける)
    mat.m[1][0] = 0.0f;
    mat.m[1][1] = -2.0f / height;
    mat.m[1][2] = 0.0f;
    mat.m[1][3] = 0.0f;

    // Z方向スケーリング (DirectXは0～1)
    mat.m[2][0] = 0.0f;
    mat.m[2][1] = 0.0f;
    mat.m[2][2] = 1.0f / depth;
    mat.m[2][3] = 0.0f;

    // 平行移動部分 (NDCへ合わせる)
    mat.m[3][0] = -(left + right) / width;
    mat.m[3][1] = (top + bottom) / height;   // 上下反転しているので符号を反転
    mat.m[3][2] = -nearClip / depth;
    mat.m[3][3] = 1.0f;

    return mat;
}

Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
    Matrix4x4 mat = {};
    mat.m[0][0] = scale.x;
    mat.m[1][1] = scale.y;
    mat.m[2][2] = scale.z;
    mat.m[3][3] = 1.0f;
    return mat;
}

Matrix4x4 MakeRotateZMatrix(float angleRad) {
    Matrix4x4 mat = {};
    float c = cosf(angleRad);
    float s = sinf(angleRad);

    mat.m[0][0] = c;
    mat.m[0][1] = s;
    mat.m[1][0] = -s;
    mat.m[1][1] = c;
    mat.m[2][2] = 1.0f;
    mat.m[3][3] = 1.0f;
    return mat;
}

Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
    Matrix4x4 mat = {};
    mat.m[0][0] = 1.0f;
    mat.m[1][1] = 1.0f;
    mat.m[2][2] = 1.0f;
    mat.m[3][3] = 1.0f;

    // 平行移動成分を設定
    mat.m[3][0] = translate.x;
    mat.m[3][1] = translate.y;
    mat.m[3][2] = translate.z;
    return mat;
}

void Log(const std::string& message) {
    OutputDebugStringA(message.c_str());
}

std::wstring ConvertString(const std::string& str) {
    if (str.empty()) {
        return std::wstring();
    }

    auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
    if (sizeNeeded == 0) {
        return std::wstring();
    }
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
    return result;
}

std::string ConvertString(const std::wstring& str) {
    if (str.empty()) {
        return std::string();
    }

    auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
    if (sizeNeeded == 0) {
        return std::string();
    }
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
    return result;
}

IDxcBlob* CompileShader(
    // CompilerするShaderファイルへのパス
    const std::wstring& filePath,
    // Compilerに使用するProfile
    const wchar_t* profile,
    // 初期化で生成したものを3つ
    IDxcUtils* dxcUtils,
    IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler) {
    // これからシェーダーをコンパイルする旨をログに出す
    Log(ConvertString(std::format(L"Begin CompileShader, path: {}, profile:{}\n", filePath, profile)));
    // hlslファイルを読む
    IDxcBlobEncoding* shaderSource = nullptr;
    HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
    // 読めなかったら止める
    assert(SUCCEEDED(hr));
    // 読み込んだファイルの内容を設定する
    DxcBuffer shaderSourceBuffer;
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8;    // UTF8の文字コードであることを通知

    LPCWSTR arguments[] = {

    filePath.c_str(), // コンパイル対象のhisl ファイル名
        L"-E", L"main", // エントリーポイントの指定。基本的にmain以外にはしない
        L"-T", profile, // ShaderProfileの設定
        L"-Zi", L"-Qembed_debug", // デバッグ用の情報を埋め込む
        L"-Od", // 最適化を外しておく
        L"-Zpr", // メモリレイアウトは行優先
    };
    // 実際にShaderをコンパイルする
    IDxcResult* shaderResult = nullptr;
    hr = dxcCompiler->Compile(
        &shaderSourceBuffer,
        // 読み込んだファイル
        arguments,
        // コンパイルオプション
        _countof(arguments), // コンパイルオプションの数
        includeHandler,
        // includeが含まれた諸々
        IID_PPV_ARGS(&shaderResult) // コンパイル結果
    );
    // コンパイルエラーではなくdxcが起動できないなど致命的な状況
    assert(SUCCEEDED(hr));

    // 警告・エラーが出てたらログに出して止める
    IDxcBlobUtf8* shaderError = nullptr;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        Log(shaderError->GetStringPointer());
        // 警告・エラーダメゼッタイ
        assert(false);
    }

    // コンパイル結果から実行用のバイナリ部分を取得
    IDxcBlob* shaderBlob = nullptr;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));
    // 成功したログを出す
    Log(ConvertString(std::format(L"Compile Succeeded, path: {}, profile:{}\n", filePath, profile)));
    // もう使わないリソースを解放
    shaderSource->Release();
    shaderResult->Release();
    // 実行用のバイナリを返却
    return shaderBlob;
}

ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
    assert(device != nullptr);

    // ヒーププロパティの設定 (アップロード用)
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;           // CPU → GPU 転送
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // リソースの設定
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeInBytes;                  // バッファサイズ
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ID3D12Resource* buffer = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, // アップロード用は常にこの状態
        nullptr,
        IID_PPV_ARGS(&buffer)
    );

    assert(SUCCEEDED(hr));
    return buffer;
}

ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
    ID3D12DescriptorHeap* descriptorHeap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
    descriptorHeapDesc.Type = heapType;
    descriptorHeapDesc.NumDescriptors = numDescriptors;
    descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
    assert(SUCCEEDED(hr));
    return descriptorHeap;
}

DirectX::ScratchImage LoadTexture(const std::string& filePath) {
    // テクスチャファイルを読んでプログラムで扱えるようにする
    DirectX::ScratchImage image{};
    std::wstring filePathW = ConvertString(filePath);
    HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    assert(SUCCEEDED(hr));
    // ミップマップの作成
    DirectX::ScratchImage mipImages{};
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
    assert(SUCCEEDED(hr));
    // ミップマップ付きのデータを返す
    return mipImages;
}

ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata) {
    // metadataを基にResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width); // Textureの幅
    resourceDesc.Height = UINT(metadata.height);    // Textureの高さ
    resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipmapの数
    resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // 奥行き or 配列Textureの配列数
    resourceDesc.Format = metadata.format; // TextureのFormat
    resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定。
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textureの次元数。普段使っているのは2次元

    // 利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM; //細かい設定を行う
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;    // WriteBackポリシーでCPUアクセス可能
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; // プロセッサの近くに配置

    // Resourceの生成
    ID3D12Resource* resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties, // Heapの設定
        D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
        &resourceDesc, // Resourceの設定
        D3D12_RESOURCE_STATE_GENERIC_READ, // 初回のResourceState Textureは基本読むだけ
        nullptr, // Clear最適値。使わないのでnullptr
        IID_PPV_ARGS(&resource)); // 作成する Resourceポインタへのポインタ
    assert(SUCCEEDED(hr));
    return resource;
}

void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages) {
    // Meta情報を取得
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    // 全MipMapについて
    for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
        // MipMapLevelを指定して各Imageを取得
        const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
        // Textureに転送
        HRESULT hr = texture->WriteToSubresource(
            UINT(mipLevel),
            nullptr,            // 全領域へコピー
            img->pixels,            // 元データアドレス
            UINT(img->rowPitch),            // 1ラインサイズ
            UINT(img->slicePitch)
            // 1枚サイズ
        );
        assert(SUCCEEDED(hr));
    }
}

ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {
    // 生成するResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width; // Textureの幅
    resourceDesc.Height = height; // Textureの高さ
    resourceDesc.MipLevels = 1; // mipmapの数
    resourceDesc.DepthOrArraySize = 1; // 奥行き or 配列Textureの配列数
    resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
    resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定。
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; //2次元
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知
    // 利用するHeapの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

    // 深度値のクリア設定
    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.DepthStencil.Depth = 1.0f; // 1.0f (最大値) でクリア
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット。Resourceと合わせる

    // Resourceの生成
    ID3D12Resource* resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties, // Heapの設定
        D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
        &resourceDesc, // Resourceの設定
        D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
        &depthClearValue, // Clear最適値
        IID_PPV_ARGS(&resource)); // 作成する Resourceボインタへのポインタ
    assert(SUCCEEDED(hr));
    return resource;
}

D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += (descriptorSize * index);
    return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += (descriptorSize * index);
    return handleGPU;
}


MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
    MaterialData materialData; // 構築するMaterial Data
    std::string line; // ファイルから読んだ1行を格納するもの
    std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
    assert(file.is_open()); // とりあえず開けなかったら止める

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;
        // identifierに応じた処理
        if (identifier == "map_Kd") {
            std::string textureFilename;
            s >> textureFilename;
            // 連結してファイルパスにする
            materialData.textureFilePath = directoryPath + "/" + textureFilename;
        }
    }

    return materialData;
}

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename) {
    ModelData modelData;
    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::string line;

    // ファイルを開く
    std::ifstream file(directoryPath + "/" + filename);
    assert(file.is_open());

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::string identifier;
        std::istringstream s(line);
        s >> identifier;

        // 頂点座標
        if (identifier == "v") {
            Vector4 position{};
            s >> position.x >> position.y >> position.z;
            position.w = 1.0f;
            positions.push_back(position);
        }
        // テクスチャ座標
        else if (identifier == "vt") {
            Vector2 texcoord{};
            s >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y;
            texcoords.push_back(texcoord);
        }
        // 法線
        else if (identifier == "vn") {
            Vector3 normal{};
            s >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        }
        // 面情報
        else if (identifier == "f") {
            VertexData triangle[3];

            for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
                std::string vertexDefinition;
                s >> vertexDefinition;
                if (vertexDefinition.empty()) continue;

                std::istringstream v(vertexDefinition);
                uint32_t elementIndices[3] = { 0, 0, 0 };

                for (int32_t element = 0; element < 3; ++element) {
                    std::string index;
                    if (!std::getline(v, index, '/')) break;
                    if (!index.empty()) {
                        elementIndices[element] = static_cast<uint32_t>(std::strtoul(index.c_str(), nullptr, 10));
                    }
                }

                // 座標
                Vector4 position = (elementIndices[0] > 0 && elementIndices[0] <= positions.size())
                    ? positions[elementIndices[0] - 1]
                    : Vector4{ 0, 0, 0, 1 };
                position.x *= -1.0f; // ✅ x反転

                // UV（無い場合は中央UVを仮設定）
                Vector2 texcoord = { 0.5f, 0.5f };
                if (elementIndices[1] > 0 && elementIndices[1] <= texcoords.size()) {
                    texcoord = texcoords[elementIndices[1] - 1];
                }

                // 法線（無い場合は上方向を仮設定）
                Vector3 normal = { 0, 1, 0 };
                if (elementIndices[2] > 0 && elementIndices[2] <= normals.size()) {
                    normal = normals[elementIndices[2] - 1];
                }


                triangle[faceVertex] = { position, texcoord, normal };
            }

            // 頂点を逆順にpush_back（回転方向の補正）
            modelData.vertices.push_back(triangle[2]);
            modelData.vertices.push_back(triangle[1]);
            modelData.vertices.push_back(triangle[0]);
        }
        // マテリアル参照
        else if (identifier == "mtllib") {
            std::string materialFilename;
            s >> materialFilename;
            modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
        }
    }

    return modelData;
}

SphereMesh CreateSphere(uint32_t subdivision, float radius = 1.0f) {
    SphereMesh mesh;
    const float kLonEvery = static_cast<float>(M_PI) * 2.0f / float(subdivision);
    const float kLatEvery = static_cast<float>(M_PI) / float(subdivision);

    for (uint32_t lat = 0; lat <= subdivision; ++lat) {
        float theta = -static_cast<float>(M_PI) / 2.0f + kLatEvery * lat;
        for (uint32_t lon = 0; lon <= subdivision; ++lon) {
            float phi = lon * kLonEvery;

            VertexData v{};
            v.position.x = radius * cosf(theta) * cosf(phi);
            v.position.y = radius * sinf(theta);
            v.position.z = radius * cosf(theta) * sinf(phi);
            v.position.w = 1.0f;

            v.texcoord.x = float(lon) / subdivision;
            v.texcoord.y = 1.0f - float(lat) / subdivision;

            v.normal = { v.position.x, v.position.y, v.position.z };
            mesh.vertices.push_back(v);
        }
    }

    // インデックス作成
    for (uint32_t lat = 0; lat < subdivision; ++lat) {
        for (uint32_t lon = 0; lon < subdivision; ++lon) {
            uint32_t i0 = lat * (subdivision + 1) + lon;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + (subdivision + 1);
            uint32_t i3 = i2 + 1;

            mesh.indices.push_back(i0);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i1);

            mesh.indices.push_back(i1);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i3);
        }
    }

    return mesh;
}

SoundData SoundLoadWave(const char* filename) {
    // ファイル入力ストリームのインスタンス
    std::ifstream file;
    //wavファイルをバイナリモードで開く
    file.open(filename, std::ios_base::binary);
    // ファイルオープン失敗を検出する
    assert(file.is_open());

    // RIFFヘッダーの読み込み
    RiffHeader riff;
    file.read((char*)&riff, sizeof(riff));
    // ファイルがRIFFかチェック
    if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
        assert(0);
    }
    // タイプがWAVEかチェック
    if (strncmp(riff.type, "WAVE", 4) != 0) {
        assert(0);
    }

    // Formatチャンクの読み込み
    FormatChunk format = {};
    // チャンクヘッダーの確認
    file.read((char*)&format, sizeof(ChunkHeader));
    if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
        assert(0);
    }
    // チャンク本体の読み込み
    assert(format.chunk.size <= sizeof(format.fmt));
    file.read((char*)&format.fmt, format.chunk.size);

    // Dataチャンクの読み込み
    ChunkHeader data;
    file.read((char*)&data, sizeof(data));
    // JUNKチャンクを検出した場合
    if (strncmp(data.id, "JUNK", 4) == 0) {
        // 読み取り位置をJUNKチャンクの終わりまで進める
        file.seekg(data.size, std::ios_base::cur);
        // 再読み込み
        file.read((char*)&data, sizeof(data));
    }


    if (strncmp(data.id, "data", 4) != 0) {
        assert(0);
    }
    // Dataチャンクのデータ部(波形データ)の読み込み
    char* pBuffer = new char[data.size];
    file.read(pBuffer, data.size);
    // Waveファイルを閉じる
    file.close();

    // returnする為の音声データ
    SoundData soundData = {};
    soundData.wfex = format.fmt;
    soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
    soundData.bufferSize = data.size;
    return soundData;
}

void SoundUnLoad(SoundData* soundData) {
    // バッファのメモリを解放
    delete[] soundData->pBuffer;
    soundData->pBuffer = 0;
    soundData->bufferSize = 0;
    soundData->wfex = {};
}

void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData) {
    HRESULT result;
    // 波形フォーマットを元にSourceVoiceの生成
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(result));

    // 再生する波形データの設定
    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.pBuffer;
    buf.AudioBytes = soundData.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;

    // 波形データの再生
    result = pSourceVoice->SubmitSourceBuffer(&buf);
    result = pSourceVoice->Start();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    CoInitializeEx(0, COINIT_MULTITHREADED);

    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    // メッセージに応じてゲーム固有の処理を行う
    switch (msg) {
        // ウィンドウが破棄された
    case WM_DESTROY:
        // OSに対して、アプリの終了を伝える
        PostQuitMessage(0);
        return 0;
    }
    // 標準のメッセージ処理を行う
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

// Windowsアプリでのエントリーポイント (main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    WNDCLASS wc{};
    // ウィンドウプロシージャ
    wc.lpfnWndProc = WindowProc;
    // ウィンドウクラス名(なんでも良い)
    wc.lpszClassName = L"CG2WindowClass";
    // インスタンスハンドル
    wc.hInstance = GetModuleHandle(nullptr);
    // カーソル
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // ウィンドウクラスを登録する
    RegisterClass(&wc);

    // クライアント領域のサイズ
    const int32_t kClientWidth = 1280;
    const int32_t kClientHeight = 720;
    // ウィンドウサイズを表す構造体にクライアント領域を入れる
    RECT wrc = { 0, 0, kClientWidth, kClientHeight };
    // クライアント領域を元に実際のサイズにwrcを変更してもらう
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    // ウィンドウの生成
    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        L"CG2",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wrc.right - wrc.left,
        wrc.bottom - wrc.top,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr
    );

    ShowWindow(hwnd, SW_SHOW);

#ifdef _DEBUG
    ID3D12Debug1* debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        // デバッグレイヤーを有効化する
        debugController->EnableDebugLayer();
        // さらにGPU側でもチェックを行うようにする
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif

    // DXGIファクトリーの生成
    IDXGIFactory7* dxgiFactory = nullptr;
    // HRESULTはWindows系のエラーコードであり、
    // 関数が成功したかどうかをSUCCEEDEDマクロで判定できる
    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
    // 初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、どうにもできない場合が多いのでassertにしておく
    assert(SUCCEEDED(hr));

    // 使用するアダプタ用の変数。最初にnullptrを入れておく
    IDXGIAdapter4* useAdapter = nullptr;
    // 良い順にアダプタを頼む
    for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
        // アダプターの情報を取得する
        DXGI_ADAPTER_DESC3  adapterDesc{};
        hr = useAdapter->GetDesc3(&adapterDesc);
        assert(SUCCEEDED(hr)); //取得できないのは一大事
        // ソフトウェアアダプタでなければ採用!
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            // 採用したアダプタの情報をログに出力。wstringの方なので注意
            Log(ConvertString(std::format(L"Use Adapater: {}\n", adapterDesc.Description)));
            break;
        }
        useAdapter = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
    }
    // 適切なアダプタが見つからなかったので起動できない
    assert(useAdapter != nullptr);

    ID3D12Device* device = nullptr;
    // 機能レベルとログ出力用の文字列
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
    };

    const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };
    // 高い順に生成できるか試していく
    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        // 採用したアダプターでデバイスを生成
        hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
        // 指定した機能レベルでデバイスが生成できたかを確認
        if (SUCCEEDED(hr)) {
            // 生成できたのでログ出力を行ってループを抜ける
            Log(std::format("FeatureLevel: {}\n", featureLevelStrings[i]));
            break;
        }
    }
    // デバイスの生成がうまくいかなかったので起動できない
    assert(device != nullptr);
    Log("Complete create D3D12Device!!!\n"); // 初期化完了のログをだす

    // DescriptorSizeを取得
    const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


#ifdef _DEBUG
    ID3D12InfoQueue* infoQueue = nullptr;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        // ヤバイエラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        // エラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        // 警告時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        // 解放
        infoQueue->Release();
    }
#endif

    // dxcCompilerを初期化
    IDxcUtils* dxcUtils = nullptr;
    IDxcCompiler3* dxcCompiler = nullptr;
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
    assert(SUCCEEDED(hr));

    // 現時点でincludeはしないが、includeに対応するための設定を行っておく
    IDxcIncludeHandler* includeHandler = nullptr;
    hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
    assert(SUCCEEDED(hr));

    // RootSignature作成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
    staticSamplers[0].ShaderRegister = 0; // レジスタ番号日を使う
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // Pixel Shaderで使う
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0; // 日から始まる
    descriptorRange[0].NumDescriptors = 1; // 数は1つ
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

    // RootParameter作成。Pixel ShaderのMaterialとVertex ShaderのTransform
    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;    // CBVを使う
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;    // PixelShaderで使う
    rootParameters[0].Descriptor.ShaderRegister = 0;    // レジスタ番号を使う
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;    // CBVを使う
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;        // VertexShaderで使う  
    rootParameters[1].Descriptor.ShaderRegister = 0;        // レジスタ番号を使う
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // Descriptor Tableを使う
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // Pixel Shaderで使う
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // Tableで利用する数
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;
    descriptionRootSignature.pParameters = rootParameters;    // ルートバラメータ配列へのポインタ
    descriptionRootSignature.NumParameters = _countof(rootParameters);    //配列の長さ

    // シリアライズしてバイナリにする
    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }
    // バイナリを元に生成
    ID3D12RootSignature* rootSignature = nullptr;
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));

    // InputLayout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // BlendStateの設定
    D3D12_BLEND_DESC blendDesc{};
    // すべての色要素を書き込む
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // RasiterzerStateの設定
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    //裏面(時計回り)を表示しない
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    // 三角形の中を塗りつぶす
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // Shaderをコンパイルする
    IDxcBlob* vertexShaderBlob = CompileShader(L"Resources/shaders/Object3D.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
    assert(vertexShaderBlob != nullptr);
    IDxcBlob* pixelShaderBlob = CompileShader(L"Resources/shaders/Object3D.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
    assert(pixelShaderBlob != nullptr);
    IDxcBlob* suzannePSBlob = CompileShader(L"Resources/shaders/Object3D_NoUV.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);


    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature; // RootSignature
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
    vertexShaderBlob->GetBufferSize() };
    // VertexShader
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
    pixelShaderBlob->GetBufferSize() };// Pixel Shader
    graphicsPipelineStateDesc.BlendState = blendDesc; // BlendState
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState

    // 書き込むRTVの情報
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    // 利用するトポロジ (形状)のタイプ。三角形
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // どのように画面に色を打ち込むかの設定 (気にしなくて良い)
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // DepthStencil Stateの設定
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    // Depthの機能を有効化する
    depthStencilDesc.DepthEnable = true;
    // 書き込みします
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    // 比較関数はLess Equal。つまり、近ければ描画される
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // DepthStencilの設定
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // 実際に生成
    ID3D12PipelineState* graphicsPipelineState = nullptr;
    hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
    assert(SUCCEEDED(hr));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC suzannePsoDesc = graphicsPipelineStateDesc;
    suzannePsoDesc.PS = { suzannePSBlob->GetBufferPointer(), suzannePSBlob->GetBufferSize() };

    ID3D12PipelineState* suzannePipelineState = nullptr;
    hr = device->CreateGraphicsPipelineState(&suzannePsoDesc, IID_PPV_ARGS(&suzannePipelineState));
    assert(SUCCEEDED(hr));

    // Sprite用の頂点リソースを作る
    ID3D12Resource* vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 4);
    // 頂点バッファビューを作成する
    D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
    // リソースの先頭のアドレスから使う
    vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
    // 使用するリソースのサイズは頂点6つ分のサイズ
    vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;
    // 1頂点あたりのサイズ
    vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

    ID3D12Resource* lightResource = CreateBufferResource(device, sizeof(DirectionalLight));
    DirectionalLight* lightData = nullptr;
    lightResource->Map(0, nullptr, reinterpret_cast<void**>(&lightData));
    lightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };   // 白い光
    lightData->direction = { 0.0f, -1.0f, 0.0f };   // 斜め下に照らす
    lightData->intensity = 1.0f;                   // 明るさ
    lightData->mode = 2;

    // モデル読み込み
    ModelData modelData = LoadObjFile("resources", "plane.obj");
    // 頂点リソースを作る
    ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * modelData.vertices.size());
    // 頂点バッファビューを作成する
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
    // リソースの先頭のアドレスから使う
    vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());// 使用するリソースのサイズは頂点のサイズ
    vertexBufferView.StrideInBytes = sizeof(VertexData); // 1頂点あたりのサイズ

    // 頂点リソースにデータを書き込む
    VertexData* vertexData = nullptr;
    vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData)); // 書き込むためのアドレスを取得
    std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size()); // 頂点データをリソースにコピー

    ModelData suzanneData = LoadObjFile("resources", "suzanne.obj");

    ID3D12Resource* suzanneVB = CreateBufferResource(device, sizeof(VertexData) * suzanneData.vertices.size());
    VertexData* suzanneVBData = nullptr;
    suzanneVB->Map(0, nullptr, reinterpret_cast<void**>(&suzanneVBData));
    memcpy(suzanneVBData, suzanneData.vertices.data(), sizeof(VertexData) * suzanneData.vertices.size());

    D3D12_VERTEX_BUFFER_VIEW suzanneVBV{};
    suzanneVBV.BufferLocation = suzanneVB->GetGPUVirtualAddress();
    suzanneVBV.SizeInBytes = UINT(sizeof(VertexData) * suzanneData.vertices.size());
    suzanneVBV.StrideInBytes = sizeof(VertexData);

    // WVP用
    ID3D12Resource* wvpSuzanne = CreateBufferResource(device, sizeof(TransformationMatrix));
    TransformationMatrix* wvpSuzanneData = nullptr;
    wvpSuzanne->Map(0, nullptr, reinterpret_cast<void**>(&wvpSuzanneData));
    wvpSuzanneData->World = MakeIdentity4x4();
    wvpSuzanneData->WVP = MakeIdentity4x4();

    SphereMesh sphere = CreateSphere(16, 1.0f);

    ID3D12Resource* sphereVertexBuffer = CreateBufferResource(device, sizeof(VertexData) * sphere.vertices.size());
    VertexData* sphereVBData = nullptr;
    sphereVertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&sphereVBData));
    std::memcpy(sphereVBData, sphere.vertices.data(), sizeof(VertexData) * sphere.vertices.size());

    D3D12_VERTEX_BUFFER_VIEW sphereVBV{};
    sphereVBV.BufferLocation = sphereVertexBuffer->GetGPUVirtualAddress();
    sphereVBV.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * sphere.vertices.size());
    sphereVBV.StrideInBytes = sizeof(VertexData);

    ID3D12Resource* sphereIndexBuffer = CreateBufferResource(device, sizeof(uint32_t) * sphere.indices.size());
    uint32_t* sphereIBData = nullptr;
    sphereIndexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&sphereIBData));
    std::memcpy(sphereIBData, sphere.indices.data(), sizeof(uint32_t) * sphere.indices.size());

    D3D12_INDEX_BUFFER_VIEW sphereIBV{};
    sphereIBV.BufferLocation = sphereIndexBuffer->GetGPUVirtualAddress();
    sphereIBV.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * sphere.indices.size());
    sphereIBV.Format = DXGI_FORMAT_R32_UINT;

    VertexData* vertexDataSprite = nullptr;
    vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
    // 1枚目の三角形
    vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f }; // 左下
    vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
    vertexDataSprite[0].normal = { 0.0f, 0.0f, -1.0f };
    vertexDataSprite[1].position = { 0.0f, 0.0f,   0.0f, 1.0f }; // 左上
    vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
    vertexDataSprite[1].normal = { 0.0f, 0.0f, -1.0f };
    vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f }; // 右下
    vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
    vertexDataSprite[2].normal = { 0.0f, 0.0f, -1.0f };
    vertexDataSprite[3].position = { 640.0f, 0.0f,   0.0f, 1.0f }; // 右上
    vertexDataSprite[3].texcoord = { 1.0f, 0.0f };
    vertexDataSprite[3].normal = { 0.0f, 0.0f, -1.0f };

    ID3D12Resource* indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);

    // マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
    ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Material));
    // マテリアルにデータを書き込む
    Material* materialData = nullptr;
    // 書き込むためのアドレスを取得
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    // 今回は白を書き込んでみる
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData->enableLighting = true;
    materialData->uvTransform = MakeIdentity4x4();

    // スプライトマテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
    ID3D12Resource* materialResourceSprite = CreateBufferResource(device, sizeof(Material));
    // マテリアルにデータを書き込む
    Material* materialDataSprite = nullptr;
    // 書き込むためのアドレスを取得
    materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
    // 今回は白を書き込んでみる
    materialDataSprite->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialDataSprite->enableLighting = false;
    materialDataSprite->uvTransform = MakeIdentity4x4();

    // Sphere 用
    ID3D12Resource* wvpResourceSphere = CreateBufferResource(device, sizeof(TransformationMatrix));
    TransformationMatrix* wvpDataSphere = nullptr;
    wvpResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataSphere));
    wvpDataSphere->World = MakeIdentity4x4();
    wvpDataSphere->WVP = MakeIdentity4x4();

    // OBJ 用
    ID3D12Resource* wvpResourceObj = CreateBufferResource(device, sizeof(TransformationMatrix));
    TransformationMatrix* wvpDataObj = nullptr;
    wvpResourceObj->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataObj));
    wvpDataObj->World = MakeIdentity4x4();
    wvpDataObj->WVP = MakeIdentity4x4();

    // Sprite用のTransformation Matrixリソースを作る
    ID3D12Resource* transformationMatrixResourceSprite = CreateBufferResource(device, sizeof(TransformationMatrix));

    // データを書き込む
    TransformationMatrix* transformationMatrixDataSprite = nullptr;
    transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));

    // 単位行列を書きこんでおく
    transformationMatrixDataSprite->World = MakeIdentity4x4();
    transformationMatrixDataSprite->WVP = MakeIdentity4x4();

    D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
    // リソースの先頭のアドレスから使う
    indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
    // 使用するリソースのサイズはインデックス6つ分のサイズ
    indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
    // インデックスはuint32_tとする
    indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

    // インデックスリソースにデータを書き込む
    uint32_t* indexDataSprite = nullptr;
    indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
    indexDataSprite[0] = 0;
    indexDataSprite[1] = 1;
    indexDataSprite[2] = 2;
    indexDataSprite[3] = 1;
    indexDataSprite[4] = 3;
    indexDataSprite[5] = 2;

    // Teapot モデル読み込み
    ModelData teapotData = LoadObjFile("resources", "teapot.obj");

    // 頂点リソース作成
    ID3D12Resource* teapotVertexResource =
        CreateBufferResource(device, sizeof(VertexData) * teapotData.vertices.size());
    VertexData* teapotVertexData = nullptr;
    teapotVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&teapotVertexData));
    std::memcpy(teapotVertexData, teapotData.vertices.data(),
        sizeof(VertexData) * teapotData.vertices.size());

    D3D12_VERTEX_BUFFER_VIEW teapotVBV{};
    teapotVBV.BufferLocation = teapotVertexResource->GetGPUVirtualAddress();
    teapotVBV.SizeInBytes = UINT(sizeof(VertexData) * teapotData.vertices.size());
    teapotVBV.StrideInBytes = sizeof(VertexData);

    // WVP用リソース
    ID3D12Resource* wvpResourceTeapot = CreateBufferResource(device, sizeof(TransformationMatrix));
    TransformationMatrix* wvpDataTeapot = nullptr;
    wvpResourceTeapot->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataTeapot));
    wvpDataTeapot->World = MakeIdentity4x4();
    wvpDataTeapot->WVP = MakeIdentity4x4();

    // Bunny モデル読み込み
    ModelData bunnyData = LoadObjFile("resources", "bunny.obj");

    // 頂点バッファ作成
    ID3D12Resource* bunnyVertexResource =
        CreateBufferResource(device, sizeof(VertexData) * bunnyData.vertices.size());
    VertexData* bunnyVertexData = nullptr;
    bunnyVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&bunnyVertexData));
    std::memcpy(bunnyVertexData, bunnyData.vertices.data(),
        sizeof(VertexData) * bunnyData.vertices.size());

    D3D12_VERTEX_BUFFER_VIEW bunnyVBV{};
    bunnyVBV.BufferLocation = bunnyVertexResource->GetGPUVirtualAddress();
    bunnyVBV.SizeInBytes = UINT(sizeof(VertexData) * bunnyData.vertices.size());
    bunnyVBV.StrideInBytes = sizeof(VertexData);

    // WVPリソース作成
    ID3D12Resource* wvpResourceBunny = CreateBufferResource(device, sizeof(TransformationMatrix));
    TransformationMatrix* wvpDataBunny = nullptr;
    wvpResourceBunny->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataBunny));
    wvpDataBunny->World = MakeIdentity4x4();
    wvpDataBunny->WVP = MakeIdentity4x4();


    // ビューポート
    D3D12_VIEWPORT viewport{};
    // クライアント領域のサイズと一緒にして画面全体に表示
    viewport.Width = kClientWidth;
    viewport.Height = kClientHeight;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    // シザー矩形
    D3D12_RECT scissorRect{};
    // 基本的にビューポートと同じ矩形が構成されるようにする
    scissorRect.left = 0;
    scissorRect.right = kClientWidth;
    scissorRect.top = 0;
    scissorRect.bottom = kClientHeight;

    //コマンドキューを生成する
    ID3D12CommandQueue* commandQueue = nullptr;
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    hr = device->CreateCommandQueue(&commandQueueDesc,
        IID_PPV_ARGS(&commandQueue));
    // コマンドキューの生成がうまくいかなかったので起動できない
    assert(SUCCEEDED(hr));

    // コマンドアロケータを生成する
    ID3D12CommandAllocator* commandAllocator = nullptr;
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
    // コマンドアロケータの生成がうまくいかなかったので起動できない
    assert(SUCCEEDED(hr));
    // コマンドリストを生成する
    ID3D12GraphicsCommandList* commandList = nullptr;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));
    // コマンドリストの生成がうまくいかなかったので起動できない
    assert(SUCCEEDED(hr));
    // コマンドリストをクローズする
    hr = commandList->Close();
    // コマンドリストのクローズがうまくいかなかったので起動できない
    assert(SUCCEEDED(hr));

    // スワップチェーンを生成する
    IDXGISwapChain4* swapChain = nullptr;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = kClientWidth;
    // 画面の幅。ウィンドウのクライアント領域を同じものにしておく
    swapChainDesc.Height = kClientHeight;
    // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
    swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
    swapChainDesc.BufferCount = 2; // ダブルバッファ
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニタにうつしたら、中身を破棄
    // コマンドキュー、ウィンドウハンドル、設定を渡して生成する
    hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
    assert(SUCCEEDED(hr));

    // RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleはfalse
    ID3D12DescriptorHeap* rtvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

    // SRV用のヒープでディスクリプタの数は128。SRVはShader内で触るものなので、ShaderVisibleはtrue
    ID3D12DescriptorHeap* srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
    // DSV用のヒープを作成
    ID3D12DescriptorHeap* dsvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

    //  DepthStencilリソースを作成
    ID3D12Resource* depthStencilResource = CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);

    // DSVの設定
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // Resourceと合わせる
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2Dテクスチャ

    // DSVHeapの先頭にDSVを作成
    device->CreateDepthStencilView(
        depthStencilResource,
        &dsvDesc,
        dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
    );

    // 1枚目のテクスチャ
    DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    ID3D12Resource* textureResource = CreateTextureResource(device, metadata);
    UploadTextureData(textureResource, mipImages);

    // 2枚目のテクスチャ
    DirectX::ScratchImage mipImages2 = LoadTexture(modelData.material.textureFilePath);
    const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
    ID3D12Resource* textureResource2 = CreateTextureResource(device, metadata2);
    UploadTextureData(textureResource2, mipImages2);

    // 2枚目のテクスチャ
    DirectX::ScratchImage mipImages3 = LoadTexture(teapotData.material.textureFilePath);
    const DirectX::TexMetadata& metadata3 = mipImages3.GetMetadata();
    ID3D12Resource* textureResource3 = CreateTextureResource(device, metadata3);
    UploadTextureData(textureResource3, mipImages3);

    // metaDataを基にSRVの設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

    // SRVを作成する DescriptorHeapの場所を決める
    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    // 先頭はImGuiが使っているのでその次を使う
    textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    // SRVの生成
    device->CreateShaderResourceView(textureResource, &srvDesc, textureSrvHandleCPU);

    // 2枚目の SRV 設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
    srvDesc2.Format = metadata2.format;
    srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
    srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

    // SRVを作成する DescriptorHeapの場所を決める (2枚目)
    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);

    // SRVの生成
    device->CreateShaderResourceView(textureResource2, &srvDesc2, textureSrvHandleCPU2);

    // Teapot用のSRV作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc3{};
    srvDesc3.Format = metadata3.format;
    srvDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc3.Texture2D.MipLevels = UINT(metadata3.mipLevels);

    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU3 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);

    device->CreateShaderResourceView(textureResource3, &srvDesc3, textureSrvHandleCPU3);

    // テクスチャ読み込み
    DirectX::ScratchImage mipImagesBunny = LoadTexture(bunnyData.material.textureFilePath);
    const DirectX::TexMetadata& metadataBunny = mipImagesBunny.GetMetadata();
    ID3D12Resource* textureResourceBunny = CreateTextureResource(device, metadataBunny);
    UploadTextureData(textureResourceBunny, mipImagesBunny);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDescBunny{};
    srvDescBunny.Format = metadataBunny.format;
    srvDescBunny.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDescBunny.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDescBunny.Texture2D.MipLevels = UINT(metadataBunny.mipLevels);

    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPUBunny =
        GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 4);
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPUBunny =
        GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 4);

    device->CreateShaderResourceView(textureResourceBunny, &srvDescBunny, textureSrvHandleCPUBunny);


    // SwapChainからResourceを引っ張ってくる
    ID3D12Resource* swapChainResources[2] = { nullptr };
    hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
    // うまく取得できなければ起動できない
    assert(SUCCEEDED(hr));
    hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
    assert(SUCCEEDED(hr));

    // RTVの設定
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換して書き込む
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2dテクスチャとして書き込む
    // ディスクリプタの先頭を取得する
    D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    // RTVを2つ作るのでディスクリプタを2つ用意
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
    // まず1つ目を作る。1つ目は最初のところに作る。作る場所をこちらで指定してあげる必要がある
    rtvHandles[0] = rtvStartHandle;
    device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);
    // 2つ目のディスクリプタハンドルを得る(自力で)
    rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    // 2つ目を作る
    device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);

    ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice;

    // XAudio2エンジンのインスタンスを生成
    HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(result));
    result = xAudio2->CreateMasteringVoice(&masterVoice);
    assert(SUCCEEDED(result));

    // DirectInputの初期化
    IDirectInput8* directInput = nullptr;
    result = DirectInput8Create(
        GetModuleHandle(nullptr),      // アプリケーションインスタンスハンドル
        DIRECTINPUT_VERSION,           // バージョン
        IID_IDirectInput8,
        (void**)&directInput,
        nullptr
    );
    assert(SUCCEEDED(result));

    // キーボードデバイスの生成
    IDirectInputDevice8* keyboard = nullptr;
    result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
    assert(SUCCEEDED(result));

    // 入力データ形式のセット
    result = keyboard->SetDataFormat(&c_dfDIKeyboard);    // 標準形式
    assert(SUCCEEDED(result));

    // 排他制御レベルのセット
    result = keyboard->SetCooperativeLevel(
        hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(result));

    // 初期値でFenceを作る
    ID3D12Fence* fence = nullptr;
    uint64_t fenceValue = 0;
    hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    assert(SUCCEEDED(hr));
    // FenceのSignalを待つためのイベントを作成する
    HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(fenceEvent != nullptr);

    // Fenceの値を更新
    fenceValue++;
    // GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
    commandQueue->Signal(fence, fenceValue);

    // Fenceの値が指定したSignal値にたどり着いているか確認する
    // GetCompletedValueの初期値はFence作成時に渡した初期値
    if (fence->GetCompletedValue() < fenceValue) {
        // 指定したSignalにたどりついていないので、たどり着くまで待つようにイベントを設定する
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        // イベント待つ
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    // ImGuiの初期化。詳細はさして重要ではないので解説は省略する。
    // こういうもんである
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(device,
        swapChainDesc.BufferCount,
        rtvDesc.Format,
        srvDescriptorHeap,
        srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
    );

    SoundData soundData1 = SoundLoadWave("resources/Alarm01.wav");

    // Transform変数を作る
    Transform cameraTransform{ {1.0f, 1.0f, 1.0f},{-0.15f, 0.0f, 0.0f},{0.0f, 5.0f, -20.0f} };
    Transform transformSprite{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
    Transform transformSphere{ {1.0f, 1.0f, 1.0f},{0.0f, 0.0f, 0.0f},{-6.0f, 0.0f, 0.0f} };
    Transform transformObj{ {1.0f, 1.0f, 1.0f},{0.0f, static_cast<float>(M_PI), 0.0f},{0.0f, -1.0f, 0.0f} };
    Transform transformTeapot{ {0.3f, 0.3f, 0.3f},{0.0f, static_cast<float>(M_PI) * 0.25f, 0.0f},{3.0f, 0.0f, -3.0f} };
    Transform transformBunny{ {0.3f, 0.3f, 0.3f},{0.0f, -static_cast<float>(M_PI) * 0.25f, 0.0f},{-3.0f, 0.0f, -3.0f} };
    Transform transformSuzanne{ {0.5f, 0.5f, 0.5f},{0.0f, static_cast<float>(M_PI), 0.0f},{0.0f, 0.0f, 4.0f} };
    Transform uvTransformSprite{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

    bool useMonsterBall = true;

    SoundPlayWave(xAudio2.Get(), soundData1);

    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // =======================
            // ImGui UI
            // =======================
            ImGui::Begin("Settings");
            ImGui::Text("Camera");
            ImGui::DragFloat3("Camera Translate", &cameraTransform.translate.x, 0.05f);
            ImGui::DragFloat3("Camera Rotate", &cameraTransform.rotate.x, 0.01f);
            ImGui::DragFloat3("Camera Scale", &cameraTransform.scale.x, 0.01f);

            ImGui::Separator();

            ImGui::Text("Sphere");
            ImGui::DragFloat3("Sphere Translate", &transformSphere.translate.x, 0.05f);
            ImGui::DragFloat3("Sphere Rotate", &transformSphere.rotate.x, 0.01f);
            ImGui::DragFloat3("Sphere Scale", &transformSphere.scale.x, 0.01f);

            ImGui::Separator();

            ImGui::Text("OBJ");
            ImGui::DragFloat3("OBJ Translate", &transformObj.translate.x, 0.05f);
            ImGui::DragFloat3("OBJ Rotate", &transformObj.rotate.x, 0.01f);
            ImGui::DragFloat3("OBJ Scale", &transformObj.scale.x, 0.01f);

            ImGui::Separator();

            ImGui::Text("Teapot");
            ImGui::DragFloat3("Teapot Translate", &transformTeapot.translate.x, 0.05f);
            ImGui::DragFloat3("Teapot Rotate", &transformTeapot.rotate.x, 0.01f);
            ImGui::DragFloat3("Teapot Scale", &transformTeapot.scale.x, 0.01f);

            ImGui::Separator();

            ImGui::Text("Bunny");
            ImGui::DragFloat3("Bunny Translate", &transformBunny.translate.x, 0.05f);
            ImGui::DragFloat3("Bunny Rotate", &transformBunny.rotate.x, 0.01f);
            ImGui::DragFloat3("Bunny Scale", &transformBunny.scale.x, 0.01f);

            ImGui::Separator();

            ImGui::Text("Suzanne");
            ImGui::DragFloat3("Suzanne Translate", &transformSuzanne.translate.x, 0.1f);
            ImGui::DragFloat3("Suzanne Rotate", &transformSuzanne.rotate.x, 0.01f);
            ImGui::DragFloat3("Suzanne Scale", &transformSuzanne.scale.x, 0.01f);

            ImGui::Separator();

            ImGui::Text("Sprite");
            ImGui::DragFloat3("Sprite Translate", &transformSprite.translate.x, 1.0f);
            ImGui::DragFloat3("Sprite Rotate", &transformSprite.rotate.x, 0.01f);
            ImGui::DragFloat3("Sprite Scale", &transformSprite.scale.x, 0.01f);

            ImGui::Separator();

            ImGui::Text("Directional Light");
            ImGui::ColorEdit3("Light Color", &lightData->color.x);
            ImGui::DragFloat3("Light Direction", &lightData->direction.x, 0.01f, -1.0f, 1.0f);
            ImGui::DragFloat("Light Intensity", &lightData->intensity, 0.01f, 0.0f, 10.0f);
            const char* lightingModes[] = { "No Lighting", "Lambert", "Half Lambert" };
            ImGui::Combo("Lighting Mode", &lightData->mode, lightingModes, IM_ARRAYSIZE(lightingModes));
            ImGui::End();

            float len = sqrtf(lightData->direction.x * lightData->direction.x +
                lightData->direction.y * lightData->direction.y +
                lightData->direction.z * lightData->direction.z);
            if (len > 0.0001f) {
                lightData->direction.x /= len;
                lightData->direction.y /= len;
                lightData->direction.z /= len;
            }

            // =======================
            // カメラ
            // =======================
            Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
            Matrix4x4 viewMatrix = Inverse(cameraMatrix);
            Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
                0.45f,
                float(kClientWidth) / float(kClientHeight),
                0.1f, 100.0f
            );

            // =======================
            // Sprite用WVP
            // =======================
            Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
            Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
            Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(
                0.0f, 0.0f,
                float(kClientWidth), float(kClientHeight),
                0.0f, 100.0f
            );
            transformationMatrixDataSprite->World = worldMatrixSprite;
            transformationMatrixDataSprite->WVP = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

            // UV変換
            Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
            uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
            uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
            materialDataSprite->uvTransform = uvTransformMatrix;

            // キーボード情報の取得開始
            keyboard->Acquire();

            // 全キーの入力状態を取得する
            BYTE key[256] = {};
            keyboard->GetDeviceState(sizeof(key), key);

            // 数字のキーが押されていたら
            if (key[DIK_0]) {
                OutputDebugStringA("Hit 0\n");
                // 出力ウィンドウに 「Hit 0」と表示
            }

            ImGui::Render();

            // =======================
            // コマンドリスト開始
            // =======================
            commandAllocator->Reset();
            commandList->Reset(commandAllocator, graphicsPipelineState);

            UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = swapChainResources[backBufferIndex];
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            commandList->ResourceBarrier(1, &barrier);

            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);

            float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
            commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
            commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            // 描画用のDescriptorHeap設定
            ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
            commandList->SetDescriptorHeaps(1, descriptorHeaps);

            commandList->RSSetViewports(1, &viewport);
            commandList->RSSetScissorRects(1, &scissorRect);
            commandList->SetGraphicsRootSignature(rootSignature);

            // =======================
            // 球体描画
            // =======================
            Matrix4x4 worldSphere = MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);
            wvpDataSphere->World = worldSphere;
            wvpDataSphere->WVP = Multiply(worldSphere, Multiply(viewMatrix, projectionMatrix));

            commandList->IASetVertexBuffers(0, 1, &sphereVBV);
            commandList->IASetIndexBuffer(&sphereIBV);
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
            commandList->SetGraphicsRootConstantBufferView(1, wvpResourceSphere->GetGPUVirtualAddress()); // ← 変更
            commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
            commandList->SetGraphicsRootConstantBufferView(3, lightResource->GetGPUVirtualAddress());
            commandList->DrawIndexedInstanced(static_cast<UINT>(sphere.indices.size()), 1, 0, 0, 0);

            // =======================
            // OBJ描画
            // =======================
            Matrix4x4 worldObj = MakeAffineMatrix(transformObj.scale, transformObj.rotate, transformObj.translate);
            wvpDataObj->World = worldObj;
            wvpDataObj->WVP = Multiply(worldObj, Multiply(viewMatrix, projectionMatrix));

            commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
            commandList->SetGraphicsRootConstantBufferView(1, wvpResourceObj->GetGPUVirtualAddress()); // ← 変更
            commandList->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
            commandList->SetGraphicsRootConstantBufferView(3, lightResource->GetGPUVirtualAddress());
            commandList->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);

            // =======================
            // Teapot描画
            // =======================
            Matrix4x4 worldTeapot = MakeAffineMatrix(transformTeapot.scale, transformTeapot.rotate, transformTeapot.translate);
            wvpDataTeapot->World = worldTeapot;
            wvpDataTeapot->WVP = Multiply(worldTeapot, Multiply(viewMatrix, projectionMatrix));

            commandList->IASetVertexBuffers(0, 1, &teapotVBV);
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
            commandList->SetGraphicsRootConstantBufferView(1, wvpResourceTeapot->GetGPUVirtualAddress());
            commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU3);
            commandList->SetGraphicsRootConstantBufferView(3, lightResource->GetGPUVirtualAddress());
            commandList->DrawInstanced(static_cast<UINT>(teapotData.vertices.size()), 1, 0, 0);

            // =======================
            // Bunny描画
            // =======================
            Matrix4x4 worldBunny = MakeAffineMatrix(transformBunny.scale, transformBunny.rotate, transformBunny.translate);
            wvpDataBunny->World = worldBunny;
            wvpDataBunny->WVP = Multiply(worldBunny, Multiply(viewMatrix, projectionMatrix));

            commandList->IASetVertexBuffers(0, 1, &bunnyVBV);
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
            commandList->SetGraphicsRootConstantBufferView(1, wvpResourceBunny->GetGPUVirtualAddress());
            commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPUBunny);
            commandList->SetGraphicsRootConstantBufferView(3, lightResource->GetGPUVirtualAddress());
            commandList->DrawInstanced(static_cast<UINT>(bunnyData.vertices.size()), 1, 0, 0);

            Matrix4x4 worldSuzanne = MakeAffineMatrix(transformSuzanne.scale, transformSuzanne.rotate, transformSuzanne.translate);
            wvpSuzanneData->World = worldSuzanne;
            wvpSuzanneData->WVP = Multiply(worldSuzanne, Multiply(viewMatrix, projectionMatrix));

            commandList->SetPipelineState(suzannePipelineState);

            commandList->IASetVertexBuffers(0, 1, &suzanneVBV);
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
            commandList->SetGraphicsRootConstantBufferView(1, wvpSuzanne->GetGPUVirtualAddress());
            commandList->SetGraphicsRootConstantBufferView(3, lightResource->GetGPUVirtualAddress());
            commandList->DrawInstanced(static_cast<UINT>(suzanneData.vertices.size()), 1, 0, 0);

            // PSOを元に戻す
            commandList->SetPipelineState(graphicsPipelineState);


            // =======================
            // スプライト描画
            // =======================
            commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
            commandList->IASetIndexBuffer(&indexBufferViewSprite);
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
            commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
            commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
            commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

            // =======================
            // ImGui描画
            // =======================
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            commandList->ResourceBarrier(1, &barrier);

            commandList->Close();
            ID3D12CommandList* cmdLists[] = { commandList };
            commandQueue->ExecuteCommandLists(1, cmdLists);

            swapChain->Present(1, 0);

            fenceValue++;
            commandQueue->Signal(fence, fenceValue);
            if (fence->GetCompletedValue() < fenceValue) {
                fence->SetEventOnCompletion(fenceValue, fenceEvent);
                WaitForSingleObject(fenceEvent, INFINITE);
            }
        }
    }

    CloseHandle(fenceEvent);
    fence->Release();
    rtvDescriptorHeap->Release();
    swapChainResources[0]->Release();
    swapChainResources[1]->Release();
    swapChain->Release();
    commandList->Release();
    commandAllocator->Release();
    commandQueue->Release();
    device->Release();
    useAdapter->Release();
    dxgiFactory->Release();
#ifdef _DEBUG
    debugController->Release();
#endif
    CloseWindow(hwnd);
    vertexResource->Release();
    graphicsPipelineState->Release();
    signatureBlob->Release();
    if (errorBlob) {
        errorBlob->Release();
    }
    rootSignature->Release();
    pixelShaderBlob->Release();
    vertexShaderBlob->Release();
    materialResource->Release();
    srvDescriptorHeap->Release();
    includeHandler->Release();
    dxcCompiler->Release();
    dxcUtils->Release();
    textureResource->Release();
    depthStencilResource->Release();
    dsvDescriptorHeap->Release();
    vertexResourceSprite->Release();
    transformationMatrixResourceSprite->Release();
    textureResource2->Release();
    materialResourceSprite->Release();
    lightResource->Release();
    indexResourceSprite->Release();
    wvpResourceSphere->Release();
    wvpResourceObj->Release();
    sphereVertexBuffer->Release();
    sphereIndexBuffer->Release();
    teapotVertexResource->Release();
    wvpResourceTeapot->Release();
    textureResource3->Release();
    wvpResourceBunny->Release();
    bunnyVertexResource->Release();
    textureResourceBunny->Release();
    suzanneVB->Release();
    wvpSuzanne->Release();
    suzannePipelineState->Release();
    if (keyboard) {
        keyboard->Unacquire();
        keyboard->Release();
    }
    SoundUnLoad(&soundData1);
    if (directInput) {
        directInput->Release();
    }

    if (masterVoice) {
        masterVoice->DestroyVoice();
        masterVoice = nullptr;
    }

    xAudio2.Reset();

    // ImGuiの終了処理。詳細はさして重要ではないので解説は省略する。
    // こういうもんである。初期化と逆順に行う
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CoUninitialize();

    // リソースリークチェック
    IDXGIDebug1* debug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
        debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
        debug->Release();
    }

    return 0;
}
