#pragma once

#include "DXUT.h"
#include "AntTweakBar.h"

#define NUM_TONEMAP_TEXTURES 5

struct SCREEN_VERTEX
{
    D3DXVECTOR4 pos;
    D3DXVECTOR2 tex;
 
    static const DWORD FVF;
};


class PostProcessing
{
public:
  PostProcessing(void);
  ~PostProcessing(void);
  
  void DrawFullscreenQuad( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTech, UINT Width, UINT Height );
  HRESULT OnResizedSwapChain(const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
  HRESULT PostProcessing::OnCreateDevice(ID3D10Effect *eff, ID3D10Device *dev, TwBar *bar);
  HRESULT PostProcessing::OnFrameRender(ID3D10RenderTargetView *rtv, const float ClearColor[4]);
  HRESULT ClearRenderTargets(const float clearcolor[4]);
  HRESULT RenderToTexture();
  HRESULT UpdateShaderVariables();
  void OnReleasingSwapChain();
  void OnDestroyDevice();


private:
  ID3D10Effect *g_pEffect;
  ID3D10Device* pd3dDevice;
  UINT g_uiWidth, g_uiHeight;

  bool g_bHDRenabled;
  bool g_bDOFenabled;
  bool g_bMotionBlurenabled;

  float g_fDOF_offset;
  ID3D10EffectScalarVariable *g_pDOF_offset;
  int   g_iDOF_mult;
  ID3D10EffectScalarVariable *g_pDOF_mult;


  ID3D10EffectShaderResourceVariable* g_tDepth;
  ID3D10EffectShaderResourceVariable* g_pt1;
  ID3D10EffectShaderResourceVariable* g_pt2;
  ID3D10EffectShaderResourceVariable* g_pt3;

  ID3D10DepthStencilView* g_pDSV;
  ID3D10Texture2D* g_pDepthStencil;        
  ID3D10ShaderResourceView* g_pDepthStencilRV;


  ID3D10InputLayout* g_pQuadLayout;
  ID3D10Buffer* g_pScreenQuadVB;

  ID3D10Texture2D* g_pHDRTarget0;        
  ID3D10ShaderResourceView* g_pHDRTarget0RV;
  ID3D10RenderTargetView* g_pHDRTarget0RTV;


  ID3D10Texture2D* g_pHDRTarget1;        
  ID3D10ShaderResourceView* g_pHDRTarget1RV;
  ID3D10RenderTargetView* g_pHDRTarget1RTV;


  ID3D10Texture2D* g_pToneMap[NUM_TONEMAP_TEXTURES]; 
  ID3D10ShaderResourceView* g_pToneMapRV[NUM_TONEMAP_TEXTURES];
  ID3D10RenderTargetView* g_pToneMapRTV[NUM_TONEMAP_TEXTURES];



  ID3D10Texture2D* g_pHDRBrightPass;     
  ID3D10ShaderResourceView* g_pHDRBrightPassRV;
  ID3D10RenderTargetView* g_pHDRBrightPassRTV;


  ID3D10Texture2D* g_pHDRBloom;     
  ID3D10ShaderResourceView* g_pHDRBloomRV;
  ID3D10RenderTargetView* g_pHDRBloomRTV;


  ID3D10Texture2D* g_pHDRBloom2;     
  ID3D10ShaderResourceView* g_pHDRBloom2RV;
  ID3D10RenderTargetView* g_pHDRBloom2RTV;


  ID3D10Texture2D* g_pDOFTex1;     
  ID3D10ShaderResourceView* g_pDOFTex1RV;
  ID3D10RenderTargetView* g_pDOFTex1RTV;


  ID3D10Texture2D* g_pDOFTex2;     
  ID3D10ShaderResourceView* g_pDOFTex2RV;
  ID3D10RenderTargetView* g_pDOFTex2RTV;

};
