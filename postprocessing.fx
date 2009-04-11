cbuffer pp_cb0 
{
  float4x4 g_mWorldViewProj;
  float4x4 g_mWorldViewProjectionLastFrame; 
  float4x4 g_mViewInv;
}

cbuffer pp_cb_settings
{
  float g_fDOFoffset;
  int g_iDOFmult;
  int g_iDOFfadespeed;
  float g_fHDRfadespeed;
}

Texture2D g_tDepth;
Texture2D p_t1;
Texture2D p_t2;
Texture2D p_t3;

static const float g_avSampleOffsets[15] = {
  0.0000,
  0.0125,
  0.0250,
  0.0375,
  0.0500,
  0.0625,
  0.0750,
  0.0875,
  -0.0125,
  -0.0250,
  -0.0375,
  -0.0500,
  -0.0625,
  -0.0750,
  -0.0875};

static const float g_avSampleWeights[15] = {
  0.1329807,
  0.1572430,
  0.1331033,
  0.1008211,
  0.0683375,
  0.0414488,
  0.0224962,
  0.0109257,
  0.1572430,
  0.1331033,
  0.1008211,
  0.0683375,
  0.0414488,
  0.0224962,
  0.0109257};

// settings
static const float3 LUMINANCE_VECTOR  = float3(0.2125f, 0.7154f, 0.0721f);
static const float  MIDDLE_GRAY = 0.52f;
static const float  LUM_WHITE = 1.0f;
static const float  BRIGHT_THRESHOLD = 0.8f;
/*static const float3 LUMINANCE_VECTOR  = float3(0.2125f, 0.7154f, 0.0721f);
static const float  MIDDLE_GRAY = 0.72f;
static const float  LUM_WHITE = 1.5f;
static const float  BRIGHT_THRESHOLD = 0.5f;*/

// STRUCTS STRUCTS STRUCTS STRUCTS STRUCTS STRUCTS STRUCTS STRUCTS STRUCTS STRUCTS STRUCTS

struct QuadVS_Input
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

struct QuadVS_Output
{
    float4 Pos : SV_POSITION;              // Transformed position
    float2 Tex : TEXCOORD0;
};

// STATES STATES STATES STATES STATES STATES STATES STATES STATES STATES STATES STATES STATES

SamplerState PointSampler
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState PointSamplerMirror
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Mirror;
    AddressV = Mirror;
};

SamplerState LinearSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

// FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS 

float LinearizeDepth(float depth) {
  const float far = 100.0;
  const float near = 0.01;
  const float q = far / near;
  return depth / (q - (q-1)*depth);
}

// VERTEXT SHADER VERTEXT SHADER VERTEXT SHADER VERTEXT SHADER VERTEXT SHADER VERTEXT SHADER VERTEXT SHADER 

QuadVS_Output QuadVS( QuadVS_Input Input )
{
    QuadVS_Output Output;
    Output.Pos = Input.Pos;
    Output.Tex = Input.Tex;
    return Output;
}

// PIXEL SHADER PIXEL SHADER PIXEL SHADER PIXEL SHADER PIXEL SHADER PIXEL SHADER PIXEL SHADER PIXEL SHADER 

float4 HDR_Luminosity_PS( QuadVS_Output Input ) : SV_TARGET
{    


    float4 vColor = 0.0f;
    float  fAvg = 0.0f;
    
    [unroll] for( int y = -1; y < 1; y++ )
    {
        [unroll] for( int x = -1; x < 1; x++ )
        {
      
            vColor = p_t1.Sample( PointSampler, Input.Tex, int2(x,y) );
                
            fAvg += dot( vColor.rgb, LUMINANCE_VECTOR );
        }
    }
    
    fAvg /= 4;

    return float4(fAvg, fAvg, fAvg, 1.0f);
}

float4 HDR_3x3_Downsampling_PS( QuadVS_Output Input ) : SV_TARGET
{
    float fAvg = 0.0f; 
    float4 vColor;
    
    [unroll] for( int y = -1; y <= 1; y++ )
    {
        [unroll] for( int x = -1; x <= 1; x++ )
        {
            vColor = p_t2.Sample( PointSampler, Input.Tex, int2(x,y) );
            fAvg += vColor.r; 
        }
    }

    fAvg /= 9;

    return float4(fAvg, fAvg, fAvg, 1.0f);
}

float4 HDR_BrightPass_PS( QuadVS_Output Input ) : SV_TARGET
{   
    float3 vColor = 0.0f;
    float4 vLum = p_t2.Sample( PointSampler, float2(0, 0) );
    float  fLum = vLum.r;
       
    [unroll] for( int y = -1; y <= 1; y++ ) 
    {
        [unroll] for( int x = -1; x <= 1; x++ )
        {
            float4 vSample = p_t1.Sample( PointSampler, Input.Tex, int2(x,y) );

            vColor += vSample.rgb; //farben aufsummieren
        }
    }
    
    // average
    vColor /= 9;
 
    // Bright pass and tone mapping
    vColor = max( 0.0f, vColor - BRIGHT_THRESHOLD );
    vColor *= MIDDLE_GRAY / (fLum + 0.001f);
    vColor *= (1.0f + vColor/LUM_WHITE);
    vColor /= (1.0f + vColor);
    
    return float4(vColor, 1.0f);
}

float4 HDR_BloomH_PS( QuadVS_Output Input ) : SV_TARGET
{
  
    float4 vSample = 0.0f;
    float4 vColor = 0.0f;
    float2 vSamplePosition;
    
    for( int iSample = 0; iSample < 15; iSample++ )
    {
        vSamplePosition = Input.Tex;
        vSamplePosition.x += g_avSampleOffsets[iSample];
        
        vColor = p_t1.Sample( PointSampler, vSamplePosition);
        
        vSample += g_avSampleWeights[iSample]*vColor;
    }
    
    return vSample;
}

float4 HDR_BloomV_PS( QuadVS_Output Input ) : SV_TARGET
{
  
    float4 vSample = 0.0f;
    float4 vColor = 0.0f;
    float2 vSamplePosition;
    
    for( int iSample = 0; iSample < 15; iSample++ )
    {
        vSamplePosition = Input.Tex;
        vSamplePosition.y += g_avSampleOffsets[iSample];
        
        vColor = p_t1.Sample( PointSampler, vSamplePosition);
        
        vSample += g_avSampleWeights[iSample]*vColor;
    }
    
    return vSample;
}

float4 HDR_ToneMapFading_PS( QuadVS_Output Input ) : SV_TARGET
{   
    float fTarget = 0.0f; 
    float4 vColor;
    
    [unroll] for( int y = -1; y <= 1; y++ )
    {
        [unroll] for( int x = -1; x <= 1; x++ )
        {
            vColor = p_t2.Sample( PointSampler, Input.Tex, int2(x,y) );
            fTarget += vColor.r; 
        }
    }
    float fCur = p_t1.Sample( PointSampler,int2(0,0) );
    fTarget /= 9;

    float bla = lerp(fCur, fTarget, saturate(g_fHDRfadespeed * g_fElapsedTime));


    return float4(bla, bla, bla, 1.0f);
}

float4 HDR_FinalPass_PS( QuadVS_Output Input ) : SV_TARGET
{   
  
    float4 vColor = p_t1.Sample( PointSampler, Input.Tex );
    float4 vLum = p_t2.Sample( PointSampler, float2(0,0) );
    float3 vBloom = p_t3.Sample( LinearSampler, Input.Tex );

    // Tone mapping
    vColor.rgb *= MIDDLE_GRAY / (vLum.r + 0.001f);
    vColor.rgb *= (1.0f + vColor/LUM_WHITE);
    vColor.rgb /= (1.0f + vColor);
    
    vColor.rgb += 1.0f * vBloom;  
 
    vColor.a = 1.0f;
    
    return vColor;
}


float4 HDR_FinalPass_PS_debug( QuadVS_Output Input ) : SV_TARGET
{
  return p_t1.Sample( PointSampler, Input.Tex );
}

// DOF
float4 DOF_BloomH_PS( QuadVS_Output Input ) : SV_TARGET
{
    float4 vSample = 0.0f;
    float4 vColor = 0.0f;
    float2 vSamplePosition;
    
    for( int iSample = 0; iSample < 15; iSample++ )
    {
        vSamplePosition = Input.Tex;
        vSamplePosition.x += g_avSampleOffsets[iSample]/4;
        
        vColor = p_t1.Sample( LinearSampler, vSamplePosition);
        
        vSample += g_avSampleWeights[iSample]*vColor;
    }
    
    return vSample / 1.2f;
}

float4 DOF_BloomV_PS( QuadVS_Output Input ) : SV_TARGET
{
    float4 vSample = 0.0f;
    float4 vColor = 0.0f;
    float2 vSamplePosition;
    
    for( int iSample = 0; iSample < 15; iSample++ )
    {
        vSamplePosition = Input.Tex;
        vSamplePosition.y += g_avSampleOffsets[iSample]/4;
        
        vColor = p_t1.Sample( LinearSampler, vSamplePosition);
        
        vSample += g_avSampleWeights[iSample]*vColor;
    }
    
    return vSample / 1.2f;
}

float4 DOF_DepthStep_PS( QuadVS_Output Input ) : SV_TARGET
{
  int x,y;
  g_tDepth.GetDimensions(x,y);

  float target_depth = LinearizeDepth(g_tDepth.Load(int3(int2(x/2,y/2),0)));

  target_depth = min(0.4f, target_depth);
  float curDepth = p_t1.Sample( PointSampler, float2(0,0));
  
  float bla = lerp(curDepth, target_depth, saturate(g_iDOFfadespeed * g_fElapsedTime));

  return float4(bla,bla,bla,1);
}


float4 DOF_Final_PS( QuadVS_Output Input ) : SV_TARGET
{
    float3 ColorOrg  = p_t1.Sample( PointSampler, Input.Tex).rgb;
    float3 ColorBlur = p_t2.Sample( LinearSampler, Input.Tex).rgb;

    float Blur = LinearizeDepth(g_tDepth.Load(int3(Input.Pos.xy,0)));
    Blur = min(0.4f, Blur);
    float focal_depth = p_t3.Sample( PointSampler, float2(0,0) );

    Blur = saturate((abs(Blur-focal_depth)) * g_iDOFmult);

    return float4(lerp( ColorOrg, ColorBlur, Blur ), 1.0f);
}



float4 Motion_Blur_PS( QuadVS_Output Input ) : SV_TARGET
{
    // Get the depth buffer value at this pixel.  
    float zOverW = g_tDepth.Load(int3(Input.Pos.xy,0));
    

    // H is the viewport position at this pixel in the range -1 to 1.  
    float4 H = float4(Input.Tex.x * 2 - 1, (1 - Input.Tex.y) * 2 - 1, zOverW, 1);  

    // Transform by the view-projection inverse.  
    float4 D = mul(H, g_mViewInv);  

    // Divide by w to get the world position.  
    float4 worldPos = D / D.w;  

    // Current viewport position  
    float4 currentPos = H;  

    // Use the world position, and transform by the previous view-  
    // projection matrix.  
    float4 previousPos = mul(worldPos, g_mWorldViewProjectionLastFrame);  

    // Convert to nonhomogeneous points [-1,1] by dividing by w.  
    previousPos /= previousPos.w;  

    // Use this frame's position and last frame's to compute the pixel  
    // velocity.  
    float2 velocity = (currentPos.xy - previousPos.xy) / (g_fElapsedTime * 50);  

    // Get the initial color at this pixel.  
    float4 color = p_t1.Sample( PointSampler, Input.Tex);
    float2 texCoord = velocity + Input.Tex; 

    int numSamples = 3;

    [unroll] for(int i = 1; i < numSamples; ++i, texCoord += velocity)  
    {  
      // Sample the color buffer along the velocity vector.  
      float4 currentColor = p_t1.Sample(PointSamplerMirror, texCoord); 

      // Add the current color to our color sum.  
      color += currentColor / 2.f;  
    }  
    // Average all of the samples to get the final blur color.  
    return color / 2 ;  
}

// TECHNIQUES TECHNIQUES TECHNIQUES TECHNIQUES TECHNIQUES TECHNIQUES TECHNIQUES TECHNIQUES TECHNIQUES 

technique10 HDR_Luminosity
{
  pass P0
  {
    SetVertexShader( CompileShader( vs_4_0, QuadVS() ) );
    SetGeometryShader( NULL );
    SetPixelShader( CompileShader( ps_4_0, HDR_Luminosity_PS() ) );
    SetDepthStencilState( dssDisableDepthStencil, 0 );
  }
}

technique10 HDR_3x3_Downsampling
{
  pass P0
  {
    SetVertexShader( CompileShader( vs_4_0, QuadVS() ) );
    SetGeometryShader( NULL );
    SetPixelShader( CompileShader( ps_4_0, HDR_3x3_Downsampling_PS() ) );
    SetDepthStencilState( dssDisableDepthStencil, 0 );
  }
}

technique10 HDR_BrightPass
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, QuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, HDR_BrightPass_PS() ) );
        SetDepthStencilState( dssDisableDepthStencil, 0 );          
    }
}

technique10 HDR_BloomH
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, QuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, HDR_BloomH_PS() ) );
        SetDepthStencilState( dssDisableDepthStencil, 0 );
    }
}

technique10 HDR_BloomV
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, QuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, HDR_BloomV_PS() ) );
        SetDepthStencilState( dssDisableDepthStencil, 0 );
    }
}

technique10 HDR_ToneMapFading
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, QuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, HDR_ToneMapFading_PS() ) );
        SetDepthStencilState( dssDisableDepthStencil, 0 );
    }
}

technique10 HDR_FinalPass
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, QuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, HDR_FinalPass_PS() ) );
        SetDepthStencilState( dssDisableDepthStencil, 0 );
    }
}

technique10 HDR_FinalPass_disabled
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, QuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, HDR_FinalPass_PS_debug() ) );
        SetDepthStencilState( dssDisableDepthStencil, 0 );
    }
}

technique10 DOF_BloomH
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, QuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, DOF_BloomH_PS() ) );
        SetDepthStencilState( dssDisableDepthStencil, 0 );
    }
}

technique10 DOF_BloomV
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, QuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, DOF_BloomV_PS() ) );
        SetDepthStencilState( dssDisableDepthStencil, 0 );
    }
}

technique10 DOF_DepthStep
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, QuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, DOF_DepthStep_PS() ) );
    }
}

technique10 DOF_Final
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, QuadVS() ) );
        //SetGeometryShader( CompileShader( gs_4_0, DOF_GS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, DOF_Final_PS() ) );
    }
}

technique10 Motion_Blur
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, QuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, Motion_Blur_PS() ) );
    }
}

