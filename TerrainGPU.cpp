//--------------------------------------------------------------------------------------
// File: TerrainGPU.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "Block.h"
#include "Octree.h"
#include <iostream>
#include <sstream>
#include <cmath>

ID3D10Effect *g_pEffect;
ID3D10EffectMatrixVariable *g_pWorldViewProjEV;
ID3D10EffectVectorVariable *g_pCamPosEV;

Octree *octree;

UINT g_uiWidth, g_uiHeight;
CFirstPersonCamera g_Camera;

//--------------------------------------------------------------------------------------
// Reject any D3D10 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
  pDeviceSettings->d3d10.SyncInterval = 0;
  return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
  HRESULT hr;

  Block::OnCreateDevice(pd3dDevice);

  // Load effect file
  ID3D10Blob *errors = NULL;
  hr = D3DX10CreateEffectFromFile(L"Effect.fx", NULL, NULL,
                                  "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
                                  pd3dDevice, NULL, NULL,
                                  &g_pEffect, &errors, NULL);
  if (FAILED(hr)) {
    std::wstringstream ss;
    ss << (CHAR *)errors->GetBufferPointer();
    MessageBoxW(NULL, ss.str().c_str(), L"Shader Load Error", MB_OK);
    return hr;
  }

  Block::OnLoadEffect(pd3dDevice, g_pEffect);
  
  g_pWorldViewProjEV = g_pEffect->GetVariableByName("g_mWorldViewProj")->AsMatrix();
  g_pCamPosEV = g_pEffect->GetVariableByName("g_vCamPos")->AsVector();
  {
    ID3D10ShaderResourceView *srview;
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\983-diffuse.jpg", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tDiffuse")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\983-normal.jpg", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tNormal")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\983-bump.jpg", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tBump")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"NoiseVolume.dds", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tNoise3D")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
  }

  D3DXVECTOR3 eye(0.0f, 1.0f, 0.0f);
  D3DXVECTOR3 lookat(1.0f, 1.0f, 0.0f);
  g_Camera.SetViewParams(&eye, &lookat);
  g_Camera.SetScalers(0.01f, 1.0f);

  octree = new Octree(-4, -4, -4, 3);
  octree->ActivateBlocks(pd3dDevice);

  return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
  g_uiWidth = pBackBufferSurfaceDesc->Width;
  g_uiHeight = pBackBufferSurfaceDesc->Height;

  float aspect = g_uiWidth / (float)g_uiHeight;
  g_Camera.SetProjParams(D3DX_PI / 4, aspect, 0.01f, 3.6f);

  return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
  g_Camera.FrameMove(fElapsedTime);
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
  const D3DXVECTOR3 *eye = g_Camera.GetEyePt();
  octree->Relocate(std::floor(eye->x - 4 + 0.5f), std::floor(eye->y - 4 + 0.5f), std::floor(eye->z - 4 + 0.5f));
  octree->ActivateBlocks(pd3dDevice);

  ID3D10RenderTargetView *rtv = DXUTGetD3D10RenderTargetView();
  ID3D10DepthStencilView *dsv = DXUTGetD3D10DepthStencilView();
  pd3dDevice->OMSetRenderTargets(1, &rtv, dsv);

  D3D10_VIEWPORT viewport = {
    0, 0,
    g_uiWidth, g_uiHeight,
    0.0f, 1.0f
  };
  pd3dDevice->RSSetViewports(1, &viewport);

  // Clear render target and the depth stencil
  float ClearColor[4] = { 0.176f, 0.196f, 0.667f, 0.0f };
  pd3dDevice->ClearRenderTargetView( DXUTGetD3D10RenderTargetView(), ClearColor );
  pd3dDevice->ClearDepthStencilView( DXUTGetD3D10DepthStencilView(), D3D10_CLEAR_DEPTH, 1.0, 0 );

  D3DXMATRIX world_view_proj = /**g_Camera.GetWorldMatrix() **/
                               *g_Camera.GetViewMatrix() *
                               *g_Camera.GetProjMatrix();
  g_pWorldViewProjEV->SetMatrix(world_view_proj);
  g_pCamPosEV->SetFloatVector(*const_cast<D3DXVECTOR3 *>(g_Camera.GetEyePt()));

  octree->Draw(pd3dDevice, g_pEffect->GetTechniqueByName("RenderBlock"));
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10CreateDevice
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice( void* pUserContext )
{
  SAFE_RELEASE(g_pEffect);
  SAFE_DELETE(octree);

  Block::OnDestroyDevice();
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
  g_Camera.HandleMessages(hWnd, uMsg, wParam, lParam);
  return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
  if (bKeyDown) {
    switch (nChar) {
      case VK_F5: break;
    }
  }
}


//--------------------------------------------------------------------------------------
// Handle mouse button presses
//--------------------------------------------------------------------------------------
void CALLBACK OnMouse( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown,
                       bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta,
                       int xPos, int yPos, void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Call if device was removed.  Return true to find a new device, false to quit
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved( void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D9 or D3D10)
    // that is available on the system depending on which D3D callbacks are set below

    // Set general DXUT callbacks
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackMouse( OnMouse );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackDeviceRemoved( OnDeviceRemoved );

    // Set the D3D10 DXUT callbacks. Remove these sets if the app doesn't need to support D3D10
    DXUTSetCallbackD3D10DeviceAcceptable( IsD3D10DeviceAcceptable );
    DXUTSetCallbackD3D10DeviceCreated( OnD3D10CreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10ResizedSwapChain );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10ReleasingSwapChain );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );

    // Perform any application-level initialization here

    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"TerrainGPU", NULL, NULL, NULL, 800, 600 );
    DXUTCreateDevice( true, 800, 600 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    // Perform any application-level cleanup here

    return DXUTGetExitCode();
}
