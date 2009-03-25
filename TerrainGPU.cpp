//--------------------------------------------------------------------------------------
// File: TerrainGPU.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"

#include "AntTweakBar.h"
#include "Config.h"

#include "Block.h"
#include "Octree.h"
#include "LoadingScreen.h"

#include <iostream>
#include <sstream>
#include <cmath>

ID3D10Effect *g_pEffect;
ID3D10EffectMatrixVariable *g_pWorldViewProjEV;
ID3D10EffectVectorVariable *g_pCamPosEV;
ID3D10EffectScalarVariable *g_pNormalMappingEV;
ID3D10EffectVectorVariable *g_pLightDirEV;
ID3D10EffectScalarVariable *g_pTimeEV;
ID3D10EffectScalarVariable *g_pFogEV;

bool g_bLoading = false;
int g_iLoadingMaxSize = 0;

D3DXVECTOR3 g_vLightDir(1, 1, 1);
D3DXVECTOR3 g_vCamPos(0, 0, 0);
int g_iOctreeBaseOffset = -8;

Octree *octree;
LoadingScreen g_LoadingScreen;

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

void WideToMultiByte(LPCWSTR string, std::vector<char> *output) {
  const std::ctype<wchar_t> &CType = std::use_facet<std::ctype<wchar_t> >(std::locale());
  std::wstring wstr(string);
  output->clear();
  output->resize(wstr.length() + 1);
  CType._Narrow_s(wstr.data(), wstr.data() + wstr.length(), ' ', &(*output)[0], output->size());
}

#define STATS_FRAME  0
#define STATS_DEVICE 1
#define STATS_QUEUE  2

void TW_CALL GetStatsCallback(void *value, void *clientData) {
  char **destPtr = (char **)value;
  std::vector<char> str;
  LPCWSTR wstr = L"";
  switch ((int)clientData) {
    case STATS_FRAME:
      wstr = DXUTGetFrameStats(DXUTIsVsyncEnabled());
      break;
    case STATS_DEVICE:
      wstr = DXUTGetDeviceStats();
      break;
    case STATS_QUEUE:
      *(UINT *)value = Block::queue_size();
      return;
  }
  WideToMultiByte(wstr, &str);
  TwCopyCDStringToLibrary(destPtr, &str[0]);
}

void TW_CALL OctreeSetCallback(const void *value, void *clientData) {
  UINT depth = *(UINT *)value;
  Config::Set<UINT>("OctreeDepth", depth);
  g_iOctreeBaseOffset = -(1 << (depth - 1));
  SAFE_DELETE(octree);
  octree = new Octree(g_iOctreeBaseOffset, g_iOctreeBaseOffset, g_iOctreeBaseOffset, depth);
}

//--------------------------------------------------------------------------------------
// Create any D3D10 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
  HRESULT hr;

  Config::Set<bool>("NormalMapping", true);
  Config::Set<bool>("Fog", true);
  Config::Set<bool>("LockCamera", false);
  Config::Set<UINT>("MaxBlocksPerFrame", 16);
  Config::Set<UINT>("OctreeDepth", 4);

  TwInit(TW_DIRECT3D10, pd3dDevice);
  TwDefine("GLOBAL fontsize=1");

  TwBar *bar = TwNewBar("Stats");
  TwDefine("Stats position='0 0' size='600 70' valueswidth=550 refresh=0.2");
  TwAddVarCB(bar, "Frame", TW_TYPE_CDSTRING, NULL, GetStatsCallback, (void *)STATS_FRAME, "Help='DX10 frame stats.'");
  TwAddVarCB(bar, "Device", TW_TYPE_CDSTRING, NULL, GetStatsCallback, (void *)STATS_DEVICE, "Help='DX10 device stats.'");
  TwAddVarCB(bar, "Queue", TW_TYPE_UINT32, NULL, GetStatsCallback, (void *)STATS_QUEUE, "Help='Size of queue of blocks waiting for activation.'");

  bar = TwNewBar("Settings");
  TwDefine("Settings position='0 70' size='150 500'");
  TwAddVarRW(bar, "Fog", TW_TYPE_BOOLCPP,
             (void *)&Config::Get<bool>("Fog"),
             "Help='Toggles fog effect.' key=f");
  TwAddVarRW(bar, "Normal mapping", TW_TYPE_BOOLCPP,
             (void *)&Config::Get<bool>("NormalMapping"),
             "Help='Toggles normal mapping on the terrain.' key=n");
  TwAddVarRW(bar, "Blocks per frame", TW_TYPE_UINT32,
             (void *)&Config::Get<UINT>("MaxBlocksPerFrame"),
             "Help='Maximum number of blocks to generate each frame.' min=0 max=128");
  TwAddVarCB(bar, "Octree depth", TW_TYPE_UINT32,
             OctreeSetCallback, Config::GetCallback<UINT>,
             (void *)&Config::GetKey<UINT>("OctreeDepth"),
             "Help='Max. depth of the terrain octree (1-4)' min=1 max=4");

  TwAddVarRW(bar, "Light direction", TW_TYPE_DIR3F, &g_vLightDir, "Help='Global light direction.' axisx=x axisy=y axisz=-z");  

  TwAddVarRW(bar, "Lock camera", TW_TYPE_BOOLCPP,
             (void *)&Config::Get<bool>("LockCamera"),
             "Help='Locks the camera position used for octree shifting, LOD calculations, culling etc.' key=l");

  Block::OnCreateDevice(pd3dDevice);
  g_LoadingScreen.OnCreateDevice(pd3dDevice);

  // Load effect file
  ID3D10Blob *errors = NULL;
  UINT flags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined(DEBUG) | defined(_DEBUG)
  flags |= D3D10_SHADER_DEBUG;
  flags |= D3D10_SHADER_SKIP_OPTIMIZATION;
#endif
  hr = D3DX10CreateEffectFromFile(L"Effect.fx", NULL, NULL,
                                  "fx_4_0", flags, 0,
                                  pd3dDevice, NULL, NULL,
                                  &g_pEffect, &errors, NULL);
  if (FAILED(hr)) {
    std::wstringstream ss;
    ss << (CHAR *)errors->GetBufferPointer();
    MessageBoxW(NULL, ss.str().c_str(), L"Shader Load Error", MB_OK);
    return hr;
  }

  Block::OnLoadEffect(pd3dDevice, g_pEffect);
  g_LoadingScreen.OnLoadEffect(pd3dDevice, g_pEffect);

  g_pWorldViewProjEV = g_pEffect->GetVariableByName("g_mWorldViewProj")->AsMatrix();
  g_pCamPosEV = g_pEffect->GetVariableByName("g_vCamPos")->AsVector();
  g_pNormalMappingEV = g_pEffect->GetVariableByName("g_bNormalMapping")->AsScalar();
  g_pFogEV = g_pEffect->GetVariableByName("g_bFog")->AsScalar();
  g_pLightDirEV = g_pEffect->GetVariableByName("g_vLightDir")->AsVector();
  g_pTimeEV = g_pEffect->GetVariableByName("g_fTime")->AsScalar();
  {
    ID3D10ShaderResourceView *srview;
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\863-diffuse.jpg", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tDiffuseX")->AsShaderResource()->SetResource(srview);
    g_pEffect->GetVariableByName("g_tDiffuseZ")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\983-diffuse.jpg", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tDiffuseY")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\863-normal.jpg", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tNormalX")->AsShaderResource()->SetResource(srview);
    g_pEffect->GetVariableByName("g_tNormalZ")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\983-normal.jpg", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tNormalY")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\983-bump.jpg", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tBump")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"NoiseVolume.dds", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tNoise3D")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
  }

  D3DXVECTOR3 eye(0.0f, 1.0f, 0.0f);
  D3DXVECTOR3 lookat(0.0f, 1.0f, 1.0f);
  g_Camera.SetViewParams(&eye, &lookat);
  g_Camera.SetScalers(0.01f, 1.0f);
  g_Camera.SetDrag(true, 0.5f);

  octree = new Octree(g_iOctreeBaseOffset, g_iOctreeBaseOffset, g_iOctreeBaseOffset,
                      Config::Get<UINT>("OctreeDepth"));
  octree->ActivateBlocks(pd3dDevice);

  //g_bLoading = true;
  //g_iLoadingMaxSize = Block::queue_size();

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
  g_Camera.SetProjParams(D3DX_PI / 4, aspect, 0.01f, 100.0f);

  TwWindowSize(g_uiWidth, g_uiHeight);

  return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
  //if (g_bLoading) {
  //  float loaded = 1 - (float)Block::queue_size() / g_iLoadingMaxSize;
  //  g_LoadingScreen.set_loaded(loaded);
  //  if (loaded > 0.9999f) {
  //    g_bLoading = false;
  //  }
  //}
  g_Camera.FrameMove(fElapsedTime);
  if (!Config::Get<bool>("LockCamera")) g_vCamPos = *g_Camera.GetEyePt();
  Block::OnFrameMove(fElapsedTime, g_vCamPos);
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
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

  //if (g_bLoading) {
  //  g_LoadingScreen.Draw(pd3dDevice);
  //  return;
  //}

  octree->Relocate((INT)std::floor(g_vCamPos.x + g_iOctreeBaseOffset + 0.5f),
                   (INT)std::floor(g_vCamPos.y + g_iOctreeBaseOffset + 0.5f),
                   (INT)std::floor(g_vCamPos.z + g_iOctreeBaseOffset + 0.5f));
  octree->ActivateBlocks(pd3dDevice);

  D3DXVECTOR3 eye = *g_Camera.GetEyePt();
  D3DXMATRIX world_view_proj = *g_Camera.GetViewMatrix() *
                               *g_Camera.GetProjMatrix();
  g_pWorldViewProjEV->SetMatrix(world_view_proj);
  g_pCamPosEV->SetFloatVector(eye);
  g_pNormalMappingEV->SetBool(Config::Get<bool>("NormalMapping"));
  g_pFogEV->SetBool(Config::Get<bool>("Fog"));
  g_pLightDirEV->SetFloatVector(g_vLightDir);
  g_pTimeEV->SetFloat((float)DXUTGetTime());

  octree->Draw(pd3dDevice, g_pEffect->GetTechniqueByName("RenderBlock"));

  TwDraw();
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext )
{
  TwWindowSize(0, 0);
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10CreateDevice
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice( void* pUserContext )
{
  SAFE_RELEASE(g_pEffect);
  SAFE_DELETE(octree);

  Block::OnDestroyDevice();
  g_LoadingScreen.OnDestroyDevice();

  TwTerminate();
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
  if (TwEventWin(hWnd, uMsg, wParam, lParam)) return 0;

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
