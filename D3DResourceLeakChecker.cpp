#include "D3DResourceLeakChecker.h"

D3DResourceLeakChecker::~D3DResourceLeakChecker() {
#if _DEBUG
  Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
  if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
    // APP / D3D12 / ALL をまとめて出す
    debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
    debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
    debug->ReportLiveObjects(DXGI_DEBUG_DXGI, DXGI_DEBUG_RLO_ALL);
  }
#endif
}
