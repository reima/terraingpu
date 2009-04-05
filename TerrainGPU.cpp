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
#include "Frustum.h"

#include <iostream>
#include <sstream>
#include <cmath>

ID3D10Effect *g_pEffect;
ID3D10EffectMatrixVariable *g_pWorldViewProjEV;
ID3D10EffectVectorVariable *g_pCamPosEV;
ID3D10EffectScalarVariable *g_pNormalMappingEV;
ID3D10EffectScalarVariable *g_pDetailTexEV;
ID3D10EffectVectorVariable *g_pLightDirEV;
ID3D10EffectScalarVariable *g_pTimeEV;
ID3D10EffectScalarVariable *g_pFogEV;

bool g_bLoading = false;
int g_iLoadingMaxSize = 0;

D3DXVECTOR3 g_vLightDir(0, 1, 1);
D3DXVECTOR3 g_vCamPos(0, 0, 0);
int g_iOctreeBaseOffset = -8;

Octree *octree;
LoadingScreen g_LoadingScreen;

UINT g_uiWidth, g_uiHeight;
CFirstPersonCamera g_Camera;
Frustum g_Frustum(&g_Camera);


//--------------------------------------------------------------------------------------
//  POSTPROCESSING
//--------------------------------------------------------------------------------------
struct SCREEN_VERTEX
{
    D3DXVECTOR4 pos;
    D3DXVECTOR2 tex;

    static const DWORD FVF;
};

#define NUM_TONEMAP_TEXTURES 5

bool g_bHDRenabled = true;
bool g_bDOFenabled = true;
bool g_bMotionBlurenabled = true;

ID3D10EffectShaderResourceVariable* g_tDepth;
ID3D10EffectShaderResourceVariable* g_pt1;
ID3D10EffectShaderResourceVariable* g_pt2;
ID3D10EffectShaderResourceVariable* g_pt3;

ID3D10EffectMatrixVariable* g_pmWorldViewProjInv = NULL;
ID3D10EffectMatrixVariable* g_pmWorldViewProjLastFrame = NULL;
D3DXMATRIX mWorldViewProjectionLastFrame;

ID3D10DepthStencilView* g_pDSV = NULL;
ID3D10Texture2D* g_pDepthStencil = NULL;        
ID3D10ShaderResourceView* g_pDepthStencilRV = NULL;


ID3D10InputLayout* g_pQuadLayout = NULL;
ID3D10Buffer* g_pScreenQuadVB = NULL;

ID3D10Texture2D* g_pHDRTarget0 = NULL;        
ID3D10ShaderResourceView* g_pHDRTarget0RV = NULL;
ID3D10RenderTargetView* g_pHDRTarget0RTV = NULL;


ID3D10Texture2D* g_pHDRTarget1 = NULL;        
ID3D10ShaderResourceView* g_pHDRTarget1RV = NULL;
ID3D10RenderTargetView* g_pHDRTarget1RTV = NULL;


ID3D10Texture2D* g_pToneMap[NUM_TONEMAP_TEXTURES]; 
ID3D10ShaderResourceView* g_pToneMapRV[NUM_TONEMAP_TEXTURES];
ID3D10RenderTargetView* g_pToneMapRTV[NUM_TONEMAP_TEXTURES];



ID3D10Texture2D* g_pHDRBrightPass = NULL;     
ID3D10ShaderResourceView* g_pHDRBrightPassRV = NULL;
ID3D10RenderTargetView* g_pHDRBrightPassRTV = NULL;


ID3D10Texture2D* g_pHDRBloom = NULL;     
ID3D10ShaderResourceView* g_pHDRBloomRV = NULL;
ID3D10RenderTargetView* g_pHDRBloomRTV = NULL;


ID3D10Texture2D* g_pHDRBloom2 = NULL;     
ID3D10ShaderResourceView* g_pHDRBloom2RV = NULL;
ID3D10RenderTargetView* g_pHDRBloom2RTV = NULL;


ID3D10Texture2D* g_pDOFTex1 = NULL;     
ID3D10ShaderResourceView* g_pDOFTex1RV = NULL;
ID3D10RenderTargetView* g_pDOFTex1RTV = NULL;


ID3D10Texture2D* g_pDOFTex2 = NULL;     
ID3D10ShaderResourceView* g_pDOFTex2RV = NULL;
ID3D10RenderTargetView* g_pDOFTex2RTV = NULL;


void DrawFullscreenQuad( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTech, UINT Width, UINT Height );

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
#define STATS_MEMORY 3
#define STATS_COUNTS 4

void TW_CALL GetStatsCallback(void *value, void *clientData) {
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
    case STATS_MEMORY:
      *(UINT *)value = Block::vertex_buffers_total_size() + Block::index_buffers_total_size();
      return;
    case STATS_COUNTS:
      //*(UINT *)value = Block::draw_calls();
      *(UINT *)value = Block::primitives_drawn();
      return;
  }
  std::vector<char> str;
  WideToMultiByte(wstr, &str);
  char **destPtr = (char **)value;
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
  Config::Set<bool>("DetailTex", true);
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
  TwAddVarCB(bar, "Memory", TW_TYPE_UINT32, NULL, GetStatsCallback, (void *)STATS_MEMORY, "Help='Block memory usage stats.'");
  TwAddVarCB(bar, "Draws", TW_TYPE_UINT32, NULL, GetStatsCallback, (void *)STATS_COUNTS, "Help='Number of blocks drawn this frame.'");

  bar = TwNewBar("Settings");
  TwDefine("Settings position='0 70' size='150 500'");
  TwAddVarRW(bar, "Fog", TW_TYPE_BOOLCPP,
             (void *)&Config::Get<bool>("Fog"),
             "Help='Toggles fog effect.' key=f");
  TwAddVarRW(bar, "Normal mapping", TW_TYPE_BOOLCPP,
             (void *)&Config::Get<bool>("NormalMapping"),
             "Help='Toggles normal mapping on the terrain.' key=n");
  TwAddVarRW(bar, "Detail textures", TW_TYPE_BOOLCPP,
             (void *)&Config::Get<bool>("DetailTex"),
             "Help='High quality detail textures.'");
  TwAddVarRW(bar, "Blocks per frame", TW_TYPE_UINT32,
             (void *)&Config::Get<UINT>("MaxBlocksPerFrame"),
             "Help='Maximum number of blocks to generate each frame.' min=0 max=128");
  TwAddVarCB(bar, "Octree depth", TW_TYPE_UINT32,
             OctreeSetCallback, Config::GetCallback<UINT>,
             (void *)&Config::GetKey<UINT>("OctreeDepth"),
             "Help='Max. depth of the terrain octree (1-6)' min=1 max=6");

  TwAddVarRW(bar, "Light direction", TW_TYPE_DIR3F, &g_vLightDir, "Help='Global light direction.' axisx=x axisy=y axisz=-z");  

  TwAddVarRW(bar, "Lock camera", TW_TYPE_BOOLCPP,
             (void *)&Config::Get<bool>("LockCamera"),
             "Help='Locks the camera position used for octree shifting, LOD calculations, culling etc.' key=l");

  // postprocess
  TwAddVarRW(bar, "HDR", TW_TYPE_BOOLCPP, &g_bHDRenabled, "Help='Enable HDR Bloom'");  
  TwAddVarRW(bar, "DoF", TW_TYPE_BOOLCPP, &g_bDOFenabled, "Help='Enable Depth of Field'");  
  TwAddVarRW(bar, "MotionBlur", TW_TYPE_BOOLCPP, &g_bMotionBlurenabled, "Help='Enable Motion Blur'");  

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
  g_pmWorldViewProjInv = g_pEffect->GetVariableByName("g_mViewInv")->AsMatrix();
  g_pmWorldViewProjLastFrame = g_pEffect->GetVariableByName("g_mWorldViewProjectionLastFrame")->AsMatrix();
  g_pCamPosEV = g_pEffect->GetVariableByName("g_vCamPos")->AsVector();
  g_pNormalMappingEV = g_pEffect->GetVariableByName("g_bNormalMapping")->AsScalar();
  g_pDetailTexEV = g_pEffect->GetVariableByName("g_bDetailTex")->AsScalar();
  g_pFogEV = g_pEffect->GetVariableByName("g_bFog")->AsScalar();
  g_pLightDirEV = g_pEffect->GetVariableByName("g_vLightDir")->AsVector();
  g_pTimeEV = g_pEffect->GetVariableByName("g_fTime")->AsScalar();

  {
    ID3D10ShaderResourceView *srview;
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\235-diffuse.jpg", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tDiffuseX")->AsShaderResource()->SetResource(srview);
    g_pEffect->GetVariableByName("g_tDiffuseZ")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\1792-diffuse.jpg", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tDiffuseY")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\235-normal.jpg", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tNormalX")->AsShaderResource()->SetResource(srview);
    g_pEffect->GetVariableByName("g_tNormalZ")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\1792-normal.jpg", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tNormalY")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\1792-bump.jpg", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tBump")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\detail_color.png", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tDetail")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"Textures\\detail_noise.png", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tDetailNormals")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
    V_RETURN(D3DX10CreateShaderResourceViewFromFile(pd3dDevice, L"NoiseVolume.dds", NULL, NULL, &srview, NULL));
    g_pEffect->GetVariableByName("g_tNoise3D")->AsShaderResource()->SetResource(srview);
    SAFE_RELEASE(srview);
  }

  D3DXVECTOR3 eye(0.0f, 1.25f, 0.0f);
  D3DXVECTOR3 lookat(0.0f, 1.25f, -1.0f);
  g_Camera.SetViewParams(&eye, &lookat);
  g_Camera.SetScalers(0.01f, 1.0f);
  //g_Camera.SetDrag(true, 0.5f);

  octree = new Octree(g_iOctreeBaseOffset, g_iOctreeBaseOffset, g_iOctreeBaseOffset,
                      Config::Get<UINT>("OctreeDepth"));
  octree->ActivateBlocks(pd3dDevice);

  g_bLoading = true;
  g_iLoadingMaxSize = Block::queue_size();


  // PostProcessing

  SCREEN_VERTEX svQuad[4];
  svQuad[0].pos = D3DXVECTOR4( -1.0f, 1.0f, 0.5f, 1.0f );
  svQuad[0].tex = D3DXVECTOR2( 0.0f, 0.0f );
  svQuad[1].pos = D3DXVECTOR4( 1.0f, 1.0f, 0.5f, 1.0f );
  svQuad[1].tex = D3DXVECTOR2( 1.0f, 0.0f );
  svQuad[2].pos = D3DXVECTOR4( -1.0f, -1.0f, 0.5f, 1.0f );
  svQuad[2].tex = D3DXVECTOR2( 0.0f, 1.0f );
  svQuad[3].pos = D3DXVECTOR4( 1.0f, -1.0f, 0.5f, 1.0f );
  svQuad[3].tex = D3DXVECTOR2( 1.0f, 1.0f );

  D3D10_BUFFER_DESC vbdesc =
  {
    4 * sizeof( SCREEN_VERTEX ),
    D3D10_USAGE_DEFAULT,
    D3D10_BIND_VERTEX_BUFFER,
    0,
    0
  };

  D3D10_SUBRESOURCE_DATA InitData;
  InitData.pSysMem = svQuad;
  InitData.SysMemPitch = 0;
  InitData.SysMemSlicePitch = 0;
  V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &InitData, &g_pScreenQuadVB ) );


  // Create our quad input layout
  const D3D10_INPUT_ELEMENT_DESC quadlayout[] =
  {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D10_INPUT_PER_VERTEX_DATA, 0 },
  };

  D3D10_PASS_DESC pass_desc;
  g_pEffect->GetTechniqueByName("HDR_Luminosity")->GetPassByIndex(0)->GetDesc(&pass_desc);

  V_RETURN( pd3dDevice->CreateInputLayout( quadlayout, 2, pass_desc.pIAInputSignature,
                                           pass_desc.IAInputSignatureSize, &g_pQuadLayout ) );


  g_pt1 = g_pEffect->GetVariableByName("p_t1")->AsShaderResource();
  g_pt2 = g_pEffect->GetVariableByName("p_t2")->AsShaderResource();
  g_pt3 = g_pEffect->GetVariableByName("p_t3")->AsShaderResource();

  return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
  HRESULT hr;
  g_uiWidth = pBackBufferSurfaceDesc->Width;
  g_uiHeight = pBackBufferSurfaceDesc->Height;

  float aspect = g_uiWidth / (float)g_uiHeight;
  g_Camera.SetProjParams(D3DX_PI / 4, aspect, 0.01f, 100.0f);

  TwWindowSize(g_uiWidth, g_uiHeight);


  // Create depth stencil texture.
  //
  D3D10_TEXTURE2D_DESC smtex;
  smtex.Width = pBackBufferSurfaceDesc->Width;
  smtex.Height = pBackBufferSurfaceDesc->Height;
  smtex.MipLevels = 1;
  smtex.ArraySize = 1;
  smtex.Format = DXGI_FORMAT_R32_TYPELESS;
  smtex.SampleDesc.Count = 1;
  smtex.SampleDesc.Quality = 0;
  smtex.Usage = D3D10_USAGE_DEFAULT;
  smtex.BindFlags = D3D10_BIND_DEPTH_STENCIL | D3D10_BIND_SHADER_RESOURCE;
  smtex.CPUAccessFlags = 0;
  smtex.MiscFlags = 0;
  V_RETURN( pd3dDevice->CreateTexture2D( &smtex, NULL, &g_pDepthStencil ) );

  D3D10_SHADER_RESOURCE_VIEW_DESC DescRV;
  DescRV.Format = DXGI_FORMAT_R32_FLOAT;
  DescRV.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
  DescRV.Texture2D.MipLevels = 1;
  DescRV.Texture2D.MostDetailedMip = 0;
  V_RETURN( pd3dDevice->CreateShaderResourceView( g_pDepthStencil, &DescRV, &g_pDepthStencilRV ) );

  // Create the depth stencil view
  D3D10_DEPTH_STENCIL_VIEW_DESC DescDS;
  DescDS.Format = DXGI_FORMAT_D32_FLOAT;
  DescDS.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
  DescDS.Texture2D.MipSlice = 0;
  V_RETURN( pd3dDevice->CreateDepthStencilView( g_pDepthStencil, &DescDS, &g_pDSV ) );


  g_pEffect->GetVariableByName( "g_tDepth" )->AsShaderResource()->SetResource(g_pDepthStencilRV);


  //HDR resize

  DXGI_FORMAT fmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
  
 
  // Create the render target texture
  D3D10_TEXTURE2D_DESC Desc;
  ZeroMemory( &Desc, sizeof( D3D10_TEXTURE2D_DESC ) );
  Desc.ArraySize = 1;
  Desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
  Desc.Usage = D3D10_USAGE_DEFAULT;
  Desc.Format = fmt;
  Desc.Width = pBackBufferSurfaceDesc->Width;
  Desc.Height = pBackBufferSurfaceDesc->Height;
  Desc.MipLevels = 1;
  Desc.SampleDesc.Count = 1;
  V_RETURN( pd3dDevice->CreateTexture2D( &Desc, NULL, &g_pHDRTarget0 ) );

  // Create the render target view
  D3D10_RENDER_TARGET_VIEW_DESC DescRT;
  DescRT.Format = Desc.Format;
  DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
  DescRT.Texture2D.MipSlice = 0;
  V_RETURN( pd3dDevice->CreateRenderTargetView( g_pHDRTarget0, &DescRT, &g_pHDRTarget0RTV ) );

  // Create the resource view
  DescRV.Format = Desc.Format;
  V_RETURN( pd3dDevice->CreateShaderResourceView( g_pHDRTarget0, &DescRV, &g_pHDRTarget0RV ) );


 
  V_RETURN( pd3dDevice->CreateTexture2D( &Desc, NULL, &g_pHDRTarget1 ) );
  V_RETURN( pd3dDevice->CreateRenderTargetView( g_pHDRTarget1, &DescRT, &g_pHDRTarget1RTV ) );
  V_RETURN( pd3dDevice->CreateShaderResourceView( g_pHDRTarget1, &DescRV, &g_pHDRTarget1RV ) );

  Desc.Width /= 2;
  Desc.Height /= 2;

  V_RETURN( pd3dDevice->CreateTexture2D( &Desc, NULL, & g_pDOFTex1 ) );
  V_RETURN( pd3dDevice->CreateRenderTargetView(  g_pDOFTex1, &DescRT, & g_pDOFTex1RTV ) );
  V_RETURN( pd3dDevice->CreateShaderResourceView(  g_pDOFTex1, &DescRV, & g_pDOFTex1RV ) );

 
  V_RETURN( pd3dDevice->CreateTexture2D( &Desc, NULL, & g_pDOFTex2 ) );
  V_RETURN( pd3dDevice->CreateRenderTargetView(  g_pDOFTex2, &DescRT, & g_pDOFTex2RTV ) );
  V_RETURN( pd3dDevice->CreateShaderResourceView(  g_pDOFTex2, &DescRV, & g_pDOFTex2RV ) );

 
  // Create the bright pass texture
  Desc.Width /= 4;
  Desc.Height /= 4;
  Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  V_RETURN( pd3dDevice->CreateTexture2D( &Desc, NULL, &g_pHDRBrightPass ) );

  // Create the render target view
  DescRT.Format = Desc.Format;
  DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
  DescRT.Texture2D.MipSlice = 0;
  V_RETURN( pd3dDevice->CreateRenderTargetView( g_pHDRBrightPass, &DescRT, &g_pHDRBrightPassRTV ) );

  // Create the resource view
  DescRV.Format = Desc.Format;
  DescRV.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
  DescRV.Texture2D.MipLevels = 1;
  DescRV.Texture2D.MostDetailedMip = 0;
  V_RETURN( pd3dDevice->CreateShaderResourceView( g_pHDRBrightPass, &DescRV, &g_pHDRBrightPassRV ) );

  
  V_RETURN( pd3dDevice->CreateTexture2D( &Desc, NULL, &g_pHDRBloom ) );
  V_RETURN( pd3dDevice->CreateRenderTargetView( g_pHDRBloom, &DescRT, &g_pHDRBloomRTV ) );
  V_RETURN( pd3dDevice->CreateShaderResourceView( g_pHDRBloom, &DescRV, &g_pHDRBloomRV ) );

  V_RETURN( pd3dDevice->CreateTexture2D( &Desc, NULL, &g_pHDRBloom2 ) );
  V_RETURN( pd3dDevice->CreateRenderTargetView( g_pHDRBloom2, &DescRT, &g_pHDRBloom2RTV ) );
  V_RETURN( pd3dDevice->CreateShaderResourceView( g_pHDRBloom2, &DescRV, &g_pHDRBloom2RV ) );

  int nSampleLen = 1;
  for( int i = 0; i < NUM_TONEMAP_TEXTURES; i++ )
  {
    D3D10_TEXTURE2D_DESC tmdesc;
    ZeroMemory( &tmdesc, sizeof( D3D10_TEXTURE2D_DESC ) );
    tmdesc.ArraySize = 1;
    tmdesc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    tmdesc.Usage = D3D10_USAGE_DEFAULT;
    tmdesc.Format = fmt;
    tmdesc.Width = nSampleLen;
    tmdesc.Height = nSampleLen;
    tmdesc.MipLevels = 1;
    tmdesc.SampleDesc.Count = 1;

    V_RETURN( pd3dDevice->CreateTexture2D( &tmdesc, NULL, &g_pToneMap[i] ) );

    // Create the render target view
    D3D10_RENDER_TARGET_VIEW_DESC DescRT;
    DescRT.Format = tmdesc.Format;
    DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    DescRT.Texture2D.MipSlice = 0;
    V_RETURN( pd3dDevice->CreateRenderTargetView( g_pToneMap[i], &DescRT, &g_pToneMapRTV[i] ) );

    // Create the shader resource view
    D3D10_SHADER_RESOURCE_VIEW_DESC DescRV;
    DescRV.Format = tmdesc.Format;
    DescRV.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    DescRV.Texture2D.MipLevels = 1;
    DescRV.Texture2D.MostDetailedMip = 0;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pToneMap[i], &DescRV, &g_pToneMapRV[i] ) );

    nSampleLen *= 3;
  }

  return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
  if (g_bLoading) {
    float loaded = 1 - (float)Block::queue_size() / g_iLoadingMaxSize;
    g_LoadingScreen.set_loaded(loaded);
    if (loaded > 0.9999f) {
      g_bLoading = false;
    }
  }
  g_Camera.FrameMove(fElapsedTime);
  g_Frustum.Update();
  if (!Config::Get<bool>("LockCamera")) {
    g_vCamPos = *g_Camera.GetEyePt();
    octree->Relocate((INT)std::floor(g_vCamPos.x + g_iOctreeBaseOffset + 0.5f),
                     (INT)std::floor(g_vCamPos.y + g_iOctreeBaseOffset + 0.5f),
                     (INT)std::floor(g_vCamPos.z + g_iOctreeBaseOffset + 0.5f));
    octree->Cull(g_Frustum);
  }
  Block::OnFrameMove(fElapsedTime, g_vCamPos);
  Block::ResetStats();
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
  if (g_tDepth != NULL) g_tDepth->SetResource(NULL);
  g_pt1->SetResource(NULL);
  g_pt2->SetResource(NULL);
  g_pt3->SetResource(NULL);
  
  ID3D10ShaderResourceView *const pSRV[1] = {NULL};
  pd3dDevice->PSSetShaderResources(0, 1, pSRV);

  ID3D10RenderTargetView *rtv = DXUTGetD3D10RenderTargetView();
  pd3dDevice->OMSetRenderTargets(0, NULL, NULL);

  D3D10_VIEWPORT viewport = {
    0, 0,
    g_uiWidth, g_uiHeight,
    0.0f, 1.0f
  };
  pd3dDevice->RSSetViewports(1, &viewport);
 
  // Clear render target and the depth stencil
  //float ClearColor[4] = { 0.176f, 0.196f, 0.667f, 0.0f };  
  float ClearColor[4] = { 0.976f, 0.996f, 0.667f, 0.0f };
  pd3dDevice->ClearRenderTargetView( DXUTGetD3D10RenderTargetView(), ClearColor );
  pd3dDevice->ClearRenderTargetView( g_pDOFTex1RTV, ClearColor );
  pd3dDevice->ClearRenderTargetView( g_pDOFTex2RTV, ClearColor );
  pd3dDevice->ClearRenderTargetView( g_pHDRTarget0RTV, ClearColor );
  pd3dDevice->ClearRenderTargetView( g_pHDRTarget1RTV, ClearColor );
  pd3dDevice->ClearDepthStencilView( g_pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );
 
  pd3dDevice->OMSetRenderTargets(1, &rtv, NULL);

  if (g_bLoading) {
    g_LoadingScreen.Draw(pd3dDevice);
    return;
  }

  ID3D10RenderTargetView* aRTViews[ 1 ] = { g_pHDRTarget0RTV };
  pd3dDevice->OMSetRenderTargets(1, aRTViews, g_pDSV);

  octree->ActivateBlocks(pd3dDevice);

  D3DXVECTOR3 eye = *g_Camera.GetEyePt();
  D3DXMATRIX world_view_proj = *g_Camera.GetViewMatrix() *
                               *g_Camera.GetProjMatrix();
  g_pWorldViewProjEV->SetMatrix((float*)&world_view_proj);

  g_pmWorldViewProjLastFrame->SetMatrix((float*)&mWorldViewProjectionLastFrame);
  mWorldViewProjectionLastFrame=world_view_proj;

  D3DXMATRIX view_inv;
  D3DXMatrixInverse(&view_inv, NULL, &world_view_proj);
  g_pmWorldViewProjInv->SetMatrix((float*)&view_inv);




  g_pCamPosEV->SetFloatVector(eye);
  g_pNormalMappingEV->SetBool(Config::Get<bool>("NormalMapping"));
  g_pDetailTexEV->SetBool(Config::Get<bool>("DetailTex"));
  g_pFogEV->SetBool(Config::Get<bool>("Fog"));
  g_pLightDirEV->SetFloatVector(g_vLightDir);
  g_pTimeEV->SetFloat((float)DXUTGetTime());

  octree->Draw(pd3dDevice, g_pEffect->GetTechniqueByName("RenderBlock"));

  // postprocessing
  // Motion Blur
  if (g_bMotionBlurenabled) { 
    D3D10_TEXTURE2D_DESC descScreen;
    g_pHDRTarget1->GetDesc( &descScreen );

    aRTViews[0] =  g_pHDRTarget1RTV;
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );
    g_pt1->SetResource(g_pHDRTarget0RV);


    DrawFullscreenQuad( pd3dDevice, g_pEffect->GetTechniqueByName("Motion_Blur"), descScreen.Width, descScreen.Height );
     
    g_pt1->SetResource(NULL);
    
    ID3D10Texture2D* textemp = g_pHDRTarget0;     
    ID3D10ShaderResourceView* textempRV = g_pHDRTarget0RV;
    ID3D10RenderTargetView* textempRTV = g_pHDRTarget0RTV;
    
    g_pHDRTarget0 = g_pHDRTarget1;     
    g_pHDRTarget0RV = g_pHDRTarget1RV;
    g_pHDRTarget0RTV = g_pHDRTarget1RTV;

    g_pHDRTarget1 = textemp;     
    g_pHDRTarget1RV = textempRV;
    g_pHDRTarget1RTV = textempRTV;

    pd3dDevice->ClearRenderTargetView( g_pHDRTarget1RTV, ClearColor );
  }

  // DOF 
  // bloomH
  if (g_bDOFenabled) {

    D3D10_TEXTURE2D_DESC descScreen;
    g_pDOFTex1->GetDesc( &descScreen );
   
    aRTViews[0] =  g_pDOFTex1RTV;
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );
    g_pt1->SetResource(g_pHDRTarget0RV);

    DrawFullscreenQuad( pd3dDevice, g_pEffect->GetTechniqueByName("DOF_BloomH"), descScreen.Width, descScreen.Height );

    g_pt1->SetResource(NULL);

    // bloomV
    aRTViews[0] =  g_pDOFTex2RTV;
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );
    g_pt1->SetResource(g_pDOFTex1RV);

    DrawFullscreenQuad( pd3dDevice, g_pEffect->GetTechniqueByName("DOF_BloomV"), descScreen.Width, descScreen.Height );
    
    g_pt1->SetResource(NULL);
    // DoF final
    g_pHDRTarget1->GetDesc( &descScreen );
    pd3dDevice->ClearRenderTargetView( g_pDOFTex1RTV, ClearColor );
    aRTViews[0] =  g_pHDRTarget1RTV;
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );

    g_pt1->SetResource(g_pHDRTarget0RV);
    g_pt2->SetResource(g_pDOFTex2RV);

    DrawFullscreenQuad( pd3dDevice, g_pEffect->GetTechniqueByName("DOF_Final"), descScreen.Width, descScreen.Height );

    
    g_pt1->SetResource(NULL);
    g_pt2->SetResource(NULL);

    ID3D10Texture2D* textemp = g_pHDRTarget0;     
    ID3D10ShaderResourceView* textempRV = g_pHDRTarget0RV;
    ID3D10RenderTargetView* textempRTV = g_pHDRTarget0RTV;

    g_pHDRTarget0 = g_pHDRTarget1;     
    g_pHDRTarget0RV = g_pHDRTarget1RV;
    g_pHDRTarget0RTV = g_pHDRTarget1RTV;

    g_pHDRTarget1 = textemp;     
    g_pHDRTarget1RV = textempRV;
    g_pHDRTarget1RTV = textempRTV;

    pd3dDevice->ClearRenderTargetView( g_pHDRTarget1RTV, ClearColor );
  }

  if (g_bHDRenabled) { // hdr = on
    ID3D10ShaderResourceView* pTexSrc = NULL;
    ID3D10ShaderResourceView* pTexDest = NULL;
    ID3D10RenderTargetView* pSurfDest = NULL;

    D3D10_TEXTURE2D_DESC descSrc;
    g_pHDRTarget0->GetDesc( &descSrc );
    D3D10_TEXTURE2D_DESC descDest;
    g_pToneMap[NUM_TONEMAP_TEXTURES - 1]->GetDesc( &descDest );

    aRTViews[0] =  g_pToneMapRTV[NUM_TONEMAP_TEXTURES-1];
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );

    g_pt1->SetResource(g_pHDRTarget0RV);
    g_pt2->SetResource(NULL);

    //luminosity erzeugen
    DrawFullscreenQuad( pd3dDevice, g_pEffect->GetTechniqueByName("HDR_Luminosity"), descDest.Width, descDest.Height );

    //downsamplen 
    for( int i = NUM_TONEMAP_TEXTURES - 1; i > 0; i-- )
    {
      pTexSrc = g_pToneMapRV[i];
      pTexDest = g_pToneMapRV[i - 1];
      pSurfDest = g_pToneMapRTV[i - 1];

      D3D10_TEXTURE2D_DESC desc;
      g_pToneMap[i]->GetDesc( &desc );

      aRTViews[0] = pSurfDest;
      pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );

       g_pt2->SetResource( pTexSrc );

      DrawFullscreenQuad( pd3dDevice, g_pEffect->GetTechniqueByName("HDR_3x3_Downsampling"), desc.Width / 3, desc.Height / 3 );

       g_pt2->SetResource( NULL );
    }

    // bright pass filter
    aRTViews[0] = g_pHDRBrightPassRTV;
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );
   
    g_pt1->SetResource(g_pHDRTarget0RV);   
    g_pt2->SetResource(g_pToneMapRV[0]);
 
    DrawFullscreenQuad( pd3dDevice,  g_pEffect->GetTechniqueByName("HDR_BrightPass"), g_uiWidth / 8, g_uiHeight / 8);

    //bloom

    g_pt1->SetResource(g_pHDRBrightPassRV);  
    g_pt2->SetResource(NULL);
  
    aRTViews[0] = g_pHDRBloomRTV;
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );
    DrawFullscreenQuad( pd3dDevice,  g_pEffect->GetTechniqueByName("HDR_BloomH"), g_uiWidth / 8, g_uiHeight / 8);

    g_pt1->SetResource(g_pHDRBloomRV);  
    g_pt2->SetResource(NULL);

    aRTViews[0] = g_pHDRBloom2RTV;
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );
    DrawFullscreenQuad( pd3dDevice,  g_pEffect->GetTechniqueByName("HDR_BloomV"), g_uiWidth / 8, g_uiHeight / 8);
    
    g_pt1->SetResource(g_pHDRTarget0RV);
    g_pt2->SetResource(g_pToneMapRV[0]);
    g_pt3->SetResource(g_pHDRBloom2RV);

    //auf screen rendern 
    aRTViews[ 0 ] = rtv;
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );   
   
    DrawFullscreenQuad( pd3dDevice,  g_pEffect->GetTechniqueByName("HDR_FinalPass"), g_uiWidth, g_uiHeight);

    g_pt1->SetResource(NULL);
    g_pt2->SetResource(NULL);
    g_pt3->SetResource(NULL);
  }
  else //hdr = off
  {
    aRTViews[ 0 ] = rtv;
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );

    g_pt1->SetResource(g_pHDRTarget0RV);

    DrawFullscreenQuad( pd3dDevice,  g_pEffect->GetTechniqueByName("HDR_FinalPass_disabled"), g_uiWidth, g_uiHeight);

    g_pt1->SetResource(NULL);
  }

  TwDraw();
}

//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext )
{
  TwWindowSize(0, 0);
     
  SAFE_RELEASE(g_pHDRTarget0);        
  SAFE_RELEASE(g_pHDRTarget0RV);
  SAFE_RELEASE(g_pHDRTarget0RTV);

  SAFE_RELEASE(g_pHDRTarget1);        
  SAFE_RELEASE(g_pHDRTarget1RV);
  SAFE_RELEASE(g_pHDRTarget1RTV);

  for( int i = 0; i < NUM_TONEMAP_TEXTURES; i++ )
  {
    SAFE_RELEASE( g_pToneMap[i] );
    SAFE_RELEASE( g_pToneMapRV[i] );
    SAFE_RELEASE( g_pToneMapRTV[i] );
  }

  SAFE_RELEASE(g_pHDRBrightPass);   
  SAFE_RELEASE(g_pHDRBrightPassRV);
  SAFE_RELEASE(g_pHDRBrightPassRTV);

  SAFE_RELEASE(g_pHDRBloom);    
  SAFE_RELEASE(g_pHDRBloomRV);
  SAFE_RELEASE(g_pHDRBloomRTV);

  SAFE_RELEASE(g_pHDRBloom2);    
  SAFE_RELEASE(g_pHDRBloom2RV);
  SAFE_RELEASE(g_pHDRBloom2RTV);

  SAFE_RELEASE(g_pDOFTex1);    
  SAFE_RELEASE(g_pDOFTex1RV);
  SAFE_RELEASE(g_pDOFTex1RTV);

  SAFE_RELEASE(g_pDOFTex2);    
  SAFE_RELEASE(g_pDOFTex2RV);
  SAFE_RELEASE(g_pDOFTex2RTV);

  SAFE_RELEASE(g_pDSV);    
  SAFE_RELEASE(g_pDepthStencil);
  SAFE_RELEASE(g_pDepthStencilRV);
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10CreateDevice
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice( void* pUserContext )
{
  SAFE_RELEASE(g_pEffect);
  SAFE_DELETE(octree);

  SAFE_RELEASE(g_pQuadLayout);
  SAFE_RELEASE(g_pScreenQuadVB);

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


//--------------------------------------------------------------------------------------
// Draw a fullscreen quad, for postprocessing
//--------------------------------------------------------------------------------------
void DrawFullscreenQuad( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTech, UINT Width, UINT Height )
{
    // Save the Old viewport
    D3D10_VIEWPORT vpOld[D3D10_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];
    UINT nViewPorts = 1;
    pd3dDevice->RSGetViewports( &nViewPorts, vpOld );

    // Setup the viewport to match the backbuffer
    D3D10_VIEWPORT vp;
    vp.Width = Width;
    vp.Height = Height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    pd3dDevice->RSSetViewports( 1, &vp );


    UINT strides = sizeof( SCREEN_VERTEX );
    UINT offsets = 0;
    ID3D10Buffer* pBuffers[1] = { g_pScreenQuadVB };

    pd3dDevice->IASetInputLayout( g_pQuadLayout );
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, &strides, &offsets );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    D3D10_TECHNIQUE_DESC techDesc;
    pTech->GetDesc( &techDesc );

    for( UINT uiPass = 0; uiPass < techDesc.Passes; uiPass++ )
    {
        pTech->GetPassByIndex( uiPass )->Apply( 0 );

        pd3dDevice->Draw( 4, 0 );
    }

    // Restore the Old viewport
    pd3dDevice->RSSetViewports( nViewPorts, vpOld );
}
