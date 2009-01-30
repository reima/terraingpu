//--------------------------------------------------------------------------------------
// File: TerrainGPU.cpp
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include <sstream>

const UINT g_VoxelDim = 17;
const UINT g_VoxelDimMinusOne = 16;
const float g_InvVoxelDim = 1.0f/17.0f;
const float g_InvVoxelDimMinusOne = 1.0f/16.0f;

ID3D10Effect *g_pEffect;
ID3D10EffectMatrixVariable *g_pWorldViewProjEV;
ID3D10EffectVectorVariable *g_pBlockOffsetEV;

D3DXVECTOR3 g_BlockOffset(0, 0, 0);

ID3D10Buffer *g_pScreenAlignedQuadVB;
ID3D10InputLayout *g_pScreenAlignedQuadIL;

ID3D10Texture3D *g_pDensityVolume;
ID3D10RenderTargetView *g_pDensityRTView;
ID3D10ShaderResourceView *g_pDensitySRView;
ID3D10EffectShaderResourceVariable *g_pDensitySRVar;

ID3D10Buffer *g_pBlockVoxelsVB;
ID3D10InputLayout *g_pBlockVoxelsIL;

ID3D10Buffer *g_pBlockTrisVB;
ID3D10InputLayout *g_pBlockTrisIL;

UINT g_uiWidth, g_uiHeight;
CFirstPersonCamera g_Camera;

void RenderDensityVolume(ID3D10Device *pd3dDevice) {
  D3D10_VIEWPORT viewport;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.Width = g_VoxelDim;
  viewport.Height = g_VoxelDim;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  pd3dDevice->RSSetViewports(1, &viewport);
  pd3dDevice->OMSetRenderTargets(1, &g_pDensityRTView, NULL);

  UINT strides = sizeof(D3DXVECTOR4);
  UINT offsets = 0;
  pd3dDevice->IASetVertexBuffers(0, 1, &g_pScreenAlignedQuadVB, &strides, &offsets);
  pd3dDevice->IASetInputLayout(g_pScreenAlignedQuadIL);
  pd3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  g_pEffect->GetTechniqueByName("GenBlock")->GetPassByIndex(0)->Apply(0);
  pd3dDevice->DrawInstanced(4, g_VoxelDim, 0, 0);
}

void GenTris(ID3D10Device *pd3dDevice) {
  UINT strides = sizeof(UINT)*2;
  UINT offsets = 0;
  pd3dDevice->IASetVertexBuffers(0, 1, &g_pBlockVoxelsVB, &strides, &offsets);
  pd3dDevice->IASetInputLayout(g_pBlockVoxelsIL);
  pd3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
  pd3dDevice->SOSetTargets(1, &g_pBlockTrisVB, &offsets);
  pd3dDevice->OMSetRenderTargets(0, NULL, NULL);

  g_pDensitySRVar->SetResource(g_pDensitySRView);
  g_pEffect->GetTechniqueByName("GenBlock")->GetPassByIndex(1)->Apply(0);
  pd3dDevice->DrawInstanced(g_VoxelDimMinusOne*g_VoxelDimMinusOne, g_VoxelDimMinusOne, 0, 0);

  ID3D10Buffer *no_buffer = NULL;
  pd3dDevice->SOSetTargets(1, &no_buffer, &offsets);

  // Get rid of DEVICE_OMSETRENDERTARGETS_HAZARD and DEVICE_VSSETSHADERRESOURCES_HAZARD by
  // explicitly setting the resource slot to 0. But makes some unnecessary calls (VSSetShader etc.)
  g_pDensitySRVar->SetResource(NULL);
  g_pEffect->GetTechniqueByName("GenBlock")->GetPassByIndex(1)->Apply(0);
}

void RenderBlock(ID3D10Device *pd3dDevice, ID3D10Buffer *pBlockVB) {
  UINT strides = sizeof(D3DXVECTOR3)*2;
  UINT offsets = 0;
  pd3dDevice->IASetVertexBuffers(0, 1, &pBlockVB, &strides, &offsets);
  pd3dDevice->IASetInputLayout(g_pBlockTrisIL);
  pd3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  g_pEffect->GetTechniqueByName("RenderBlock")->GetPassByIndex(0)->Apply(0);
  pd3dDevice->DrawAuto();
}

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

  g_pWorldViewProjEV = g_pEffect->GetVariableByName("g_mWorldViewProj")->AsMatrix();
  g_pBlockOffsetEV = g_pEffect->GetVariableByName("g_vBlockOffset")->AsVector();

  // Create density volume texture (including views)
  D3D10_TEXTURE3D_DESC tex3d_desc;
  tex3d_desc.Width = g_VoxelDim;
  tex3d_desc.Height = g_VoxelDim;
  tex3d_desc.Depth = g_VoxelDim;
  tex3d_desc.Format = DXGI_FORMAT_R32_FLOAT;
  tex3d_desc.Usage = D3D10_USAGE_DEFAULT;
  tex3d_desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
  tex3d_desc.CPUAccessFlags = 0;
  tex3d_desc.MiscFlags = 0;
  tex3d_desc.MipLevels = 1;
  V_RETURN(pd3dDevice->CreateTexture3D(&tex3d_desc, NULL, &g_pDensityVolume));

  D3D10_RENDER_TARGET_VIEW_DESC rtv_desc;
  rtv_desc.Format = tex3d_desc.Format;
  rtv_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE3D;
  rtv_desc.Texture3D.WSize = tex3d_desc.Depth;
  rtv_desc.Texture3D.FirstWSlice = 0;
  rtv_desc.Texture3D.MipSlice = 0;
  V_RETURN(pd3dDevice->CreateRenderTargetView(g_pDensityVolume, &rtv_desc, &g_pDensityRTView));

  D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc;
  srv_desc.Format = tex3d_desc.Format;
  srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE3D;
  srv_desc.Texture3D.MipLevels = tex3d_desc.MipLevels;
  srv_desc.Texture3D.MostDetailedMip = 0;
  V_RETURN(pd3dDevice->CreateShaderResourceView(g_pDensityVolume, &srv_desc, &g_pDensitySRView));

  g_pDensitySRVar = g_pEffect->GetVariableByName("g_tDensityVolume")->AsShaderResource();

  // Set up screen aligned quad vertex buffer and input layout
  {
    D3DXVECTOR4 quad_vertices[] = {
      D3DXVECTOR4(-1.0f,  1.0f, 0.0f, 0.0f),
      D3DXVECTOR4( 1.0f,  1.0f, 1.0f, 0.0f),
      D3DXVECTOR4(-1.0f, -1.0f, 0.0f, 1.0f),
      D3DXVECTOR4( 1.0f, -1.0f, 1.0f, 1.0f)
    };
    D3D10_BUFFER_DESC buffer_desc;
    buffer_desc.ByteWidth = 4*4*sizeof(float); // 4x (float2+float2)
    buffer_desc.Usage = D3D10_USAGE_IMMUTABLE;
    buffer_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA init_data;
    init_data.pSysMem = quad_vertices;
    init_data.SysMemPitch = 0;
    init_data.SysMemSlicePitch = 0;
    V_RETURN(pd3dDevice->CreateBuffer(&buffer_desc, &init_data, &g_pScreenAlignedQuadVB));

    D3D10_INPUT_ELEMENT_DESC input_elements[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT num_elements = sizeof(input_elements)/sizeof(input_elements[0]);
    D3D10_PASS_DESC pass_desc;
    g_pEffect->GetTechniqueByName("GenBlock")->GetPassByIndex(0)->GetDesc(&pass_desc);
    V_RETURN(pd3dDevice->CreateInputLayout(input_elements, num_elements,
                                           pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize,
                                           &g_pScreenAlignedQuadIL));
  }

  // Set up per-block voxel vertex buffer and input layout
  {
    UINT voxels[g_VoxelDimMinusOne*g_VoxelDimMinusOne][2];
    for (UINT i = 0; i < g_VoxelDimMinusOne; ++i) {
      for (UINT j = 0; j < g_VoxelDimMinusOne; ++j) {
        voxels[i+j*g_VoxelDimMinusOne][0] = i;
        voxels[i+j*g_VoxelDimMinusOne][1] = j;
      }
    }
    D3D10_BUFFER_DESC buffer_desc;
    buffer_desc.ByteWidth = sizeof(voxels);
    buffer_desc.Usage = D3D10_USAGE_IMMUTABLE;
    buffer_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA init_data;
    init_data.pSysMem = voxels;
    init_data.SysMemPitch = 0;
    init_data.SysMemSlicePitch = 0;
    V_RETURN(pd3dDevice->CreateBuffer(&buffer_desc, &init_data, &g_pBlockVoxelsVB));

    D3D10_INPUT_ELEMENT_DESC input_elements[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT num_elements = sizeof(input_elements)/sizeof(input_elements[0]);
    D3D10_PASS_DESC pass_desc;
    g_pEffect->GetTechniqueByName("GenBlock")->GetPassByIndex(1)->GetDesc(&pass_desc);
    V_RETURN(pd3dDevice->CreateInputLayout(input_elements, num_elements,
                                           pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize,
                                           &g_pBlockVoxelsIL));
  }

  // Set up per-block triangle vertex buffers and input layout
  {
    D3D10_BUFFER_DESC buffer_desc;
    buffer_desc.ByteWidth = 2*sizeof(D3DXVECTOR3)*(g_VoxelDimMinusOne*g_VoxelDimMinusOne*g_VoxelDimMinusOne*15);
    buffer_desc.Usage = D3D10_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER | D3D10_BIND_STREAM_OUTPUT;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    V_RETURN(pd3dDevice->CreateBuffer(&buffer_desc, NULL, &g_pBlockTrisVB));

    D3D10_INPUT_ELEMENT_DESC input_elements[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
      { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT num_elements = sizeof(input_elements)/sizeof(input_elements[0]);
    D3D10_PASS_DESC pass_desc;
    g_pEffect->GetTechniqueByName("RenderBlock")->GetPassByIndex(0)->GetDesc(&pass_desc);
    V_RETURN(pd3dDevice->CreateInputLayout(input_elements, num_elements,
                                           pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize,
                                           &g_pBlockTrisIL));
  }

  D3DXVECTOR3 eye(0.5f, 0.5f, -2);
  D3DXVECTOR3 lookat(0.5f, 0.5f, 0.5f);
  g_Camera.SetViewParams(&eye, &lookat);
  g_Camera.SetScalers(0.01f, 2.0f);

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
  g_Camera.SetProjParams(D3DX_PI / 4, aspect, 0.1f, 10.0f);

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
  RenderDensityVolume(pd3dDevice);
  GenTris(pd3dDevice);

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
  g_pBlockOffsetEV->SetFloatVector(g_BlockOffset);

  RenderBlock(pd3dDevice, g_pBlockTrisVB);
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
  SAFE_RELEASE(g_pDensityVolume);
  SAFE_RELEASE(g_pDensityRTView);
  SAFE_RELEASE(g_pDensitySRView);
  SAFE_RELEASE(g_pScreenAlignedQuadVB);
  SAFE_RELEASE(g_pScreenAlignedQuadIL);
  SAFE_RELEASE(g_pBlockVoxelsVB);
  SAFE_RELEASE(g_pBlockVoxelsIL);
  SAFE_RELEASE(g_pBlockTrisVB);
  SAFE_RELEASE(g_pBlockTrisIL);
  SAFE_RELEASE(g_pEffect);
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
  if (!bKeyDown) return;
  switch (nChar) {
    case 'J': g_BlockOffset.x -= g_InvVoxelDimMinusOne; break;
    case 'L': g_BlockOffset.x += g_InvVoxelDimMinusOne; break;
    case 'K': g_BlockOffset.y -= g_InvVoxelDimMinusOne; break;
    case 'I': g_BlockOffset.y += g_InvVoxelDimMinusOne; break;
    case 'U': g_BlockOffset.z -= g_InvVoxelDimMinusOne; break;
    case 'O': g_BlockOffset.z += g_InvVoxelDimMinusOne; break;
  };
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


