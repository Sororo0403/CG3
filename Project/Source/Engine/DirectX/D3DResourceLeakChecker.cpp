#include "D3DResourceLeakChecker.h"

D3DResourceLeakChecker::~D3DResourceLeakChecker() {
#ifdef _DEBUG
  Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
  if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
    dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
  }
#endif
}
