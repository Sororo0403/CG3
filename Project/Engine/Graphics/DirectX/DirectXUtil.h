#pragma once

#include <cstdint>

namespace DirectXUtil {

constexpr uint32_t kConstantBufferAlignment = 256;

/// <summary>
/// D3D12 の Constant Buffer View 用にサイズを 256byte アラインする
/// </summary>
inline uint32_t Align256(uint32_t size) {
    return (size + kConstantBufferAlignment - 1) &
           ~(kConstantBufferAlignment - 1);
}

} // namespace DirectXUtil
