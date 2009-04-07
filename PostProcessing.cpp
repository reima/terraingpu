#include "PostProcessing.h"

PostProcessing::PostProcessing(void)
{
  g_pDSV = NULL;
  g_pDepthStencil = NULL;        
  g_pDepthStencilRV = NULL;

  g_pQuadLayout = NULL;
  g_pScreenQuadVB = NULL;

  g_pHDRTarget0 = NULL;        
  g_pHDRTarget0RV = NULL;
  g_pHDRTarget0RTV = NULL;

  g_pHDRTarget1 = NULL;        
  g_pHDRTarget1RV = NULL;
  g_pHDRTarget1RTV = NULL;

  g_pHDRBrightPass = NULL;     
  g_pHDRBrightPassRV = NULL;
  g_pHDRBrightPassRTV = NULL;

  g_pHDRBloom = NULL;     
  g_pHDRBloomRV = NULL;
  g_pHDRBloomRTV = NULL;

  g_pHDRBloom2 = NULL;     
  g_pHDRBloom2RV = NULL;
  g_pHDRBloom2RTV = NULL;

  g_pDOFTex1 = NULL;     
  g_pDOFTex1RV = NULL;
  g_pDOFTex1RTV = NULL;

  g_pDOFTex2 = NULL;     
  g_pDOFTex2RV = NULL;
  g_pDOFTex2RTV = NULL;
}

PostProcessing::~PostProcessing(void)
{
}

HRESULT PostProcessing::OnResizedSwapChain(const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc) 
{
  HRESULT hr;
  g_uiWidth = pBackBufferSurfaceDesc->Width;
  g_uiHeight = pBackBufferSurfaceDesc->Height;

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


HRESULT PostProcessing::OnCreateDevice(ID3D10Effect *eff, ID3D10Device *dev, TwBar *bar) 
{
  HRESULT hr;

  g_pEffect=eff;
  pd3dDevice=dev;

  //  USER INTERFACE
  TwAddVarRW(bar, "HDR", TW_TYPE_BOOLCPP, &g_bHDRenabled, "Help='Enable HDR Bloom'");  
  TwAddVarRW(bar, "DoF", TW_TYPE_BOOLCPP, &g_bDOFenabled, "Help='Enable Depth of Field'");  
    TwAddVarRW(bar, "DoF offset", TW_TYPE_FLOAT, &g_fDOF_offset, "Help='Depth of Field' min=0.9 max=0.99999"); 
    TwAddVarRW(bar, "DoF multi", TW_TYPE_INT32, &g_iDOF_mult, "Help='Depth of Field' min=1 max=10000"); 
  TwAddVarRW(bar, "MotionBlur", TW_TYPE_BOOLCPP, &g_bMotionBlurenabled, "Help='Enable Motion Blur'");  

  // SHADER VARIABLES
  g_pDOF_offset = g_pEffect->GetVariableByName("g_fDOFoffset")->AsScalar();
  g_fDOF_offset = 0.99f;

  g_pDOF_mult = g_pEffect->GetVariableByName("g_iDOFmult")->AsScalar();
  g_iDOF_mult = 100;


   // DATA STRUCTURES

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

HRESULT PostProcessing::ClearRenderTargets(const float ClearColor[4]) {
    
  if (g_tDepth != NULL) g_tDepth->SetResource(NULL);
  g_pt1->SetResource(NULL);
  g_pt2->SetResource(NULL);
  g_pt3->SetResource(NULL);

  pd3dDevice->ClearRenderTargetView( DXUTGetD3D10RenderTargetView(), ClearColor );
  pd3dDevice->ClearRenderTargetView( g_pDOFTex1RTV, ClearColor );
  pd3dDevice->ClearRenderTargetView( g_pDOFTex2RTV, ClearColor );
  pd3dDevice->ClearRenderTargetView( g_pHDRTarget0RTV, ClearColor );
  pd3dDevice->ClearRenderTargetView( g_pHDRTarget1RTV, ClearColor );
  pd3dDevice->ClearDepthStencilView( g_pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

  return S_OK;
}

HRESULT PostProcessing::RenderToTexture() 
{
  ID3D10RenderTargetView* aRTViews[ 1 ] = { g_pHDRTarget0RTV };
  pd3dDevice->OMSetRenderTargets(1, aRTViews, g_pDSV);

  return S_OK;  
}

HRESULT PostProcessing::UpdateShaderVariables()
{
  g_pDOF_offset->SetFloat(g_fDOF_offset);
  g_pDOF_mult->SetInt(g_iDOF_mult);

  return S_OK; 
}

HRESULT PostProcessing::OnFrameRender(ID3D10RenderTargetView *rtv, const float ClearColor[4])
{
  ID3D10RenderTargetView* aRTViews[1];
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

  return S_OK;
}

void PostProcessing::OnReleasingSwapChain()
{
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

void PostProcessing::OnDestroyDevice()
{
  SAFE_RELEASE(g_pQuadLayout);
  SAFE_RELEASE(g_pScreenQuadVB);

}



//--------------------------------------------------------------------------------------
// Draw a fullscreen quad, for postprocessing
//--------------------------------------------------------------------------------------
void PostProcessing::DrawFullscreenQuad( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTech, UINT Width, UINT Height )
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
