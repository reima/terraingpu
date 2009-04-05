//--------------------------------------------------------------------------------------
// File: Effect.fx
//--------------------------------------------------------------------------------------

cbuffer cb0 {
  float4x4 g_mWorldViewProj;
  float3   g_vCamPos;
  float    g_fTime;
}

cbuffer cb2 {
  float3 g_vBlockOffset = float3(0, 0, 0);
  float  g_vBlockActivationTime = 0;
}

cbuffer cb3 {
  bool g_bNormalMapping = true;
  bool g_bDetailTex = true;
  bool g_bFog = true;
  float3 g_vLightDir = float3(0, 1, 1);
}

cbuffer cb4 {
  float g_fLoaded = 0.0;
}

SamplerState ssNearestClamp {
  AddressU = CLAMP;
  AddressV = CLAMP;
  AddressW = CLAMP;
  Filter = MIN_MAG_MIP_POINT;
};

SamplerState ssTrilinearClamp {
  AddressU = CLAMP;
  AddressV = CLAMP;
  AddressW = CLAMP;
  Filter = MIN_MAG_MIP_LINEAR;
};

SamplerState ssTrilinearRepeat {
  AddressU = WRAP;
  AddressV = WRAP;
  AddressW = WRAP;
  Filter = MIN_MAG_MIP_LINEAR;
};

#include "BlockGen.fxh"

struct VS_BLOCK_INPUT {
  float3 Position : POSITION;
  float3 Normal   : NORMAL;
};

struct VS_BLOCK_OUTPUT {
  float4 Position : SV_Position;
  float3 Normal   : NORMAL;
  float3 LightDir : LIGHTDIR;
  float3 ViewDir  : VIEWDIR;
  float3 WorldPos : WORLDPOS;
  float  Age      : AGE;
};

Texture2D g_tDiffuseX;
Texture2D g_tDiffuseY;
Texture2D g_tDiffuseZ;
Texture2D g_tNormalX;
Texture2D g_tNormalY;
Texture2D g_tNormalZ;
Texture2D g_tBump;
Texture2D g_tDetail;
Texture2D g_tDetailNormals;

VS_BLOCK_OUTPUT Block_VS(VS_BLOCK_INPUT Input) {
  VS_BLOCK_OUTPUT Output;
  Output.Position = mul(float4(Input.Position, 1), g_mWorldViewProj);
  Output.Normal = Input.Normal;
  Output.WorldPos = Input.Position;
  Output.Age = g_fTime - g_vBlockActivationTime;
  Output.LightDir = normalize(g_vLightDir);
  Output.ViewDir = normalize(g_vCamPos - Input.Position);

  float3 N_abs = abs(Input.Normal);
  float3 Tangent;
  if (N_abs.x > N_abs.y && N_abs.x > N_abs.z) {
    // x dominant
    if (N_abs.y > N_abs.z) {
      // other vertices of this primitive possibly y dominant
      Tangent = float3(0, 0, 1);
    } else {
      // other vertices of this primitive possibly z dominant
      Tangent = float3(0, 1, 0);
    }
  } else if (N_abs.y > N_abs.x && N_abs.y > N_abs.z) {
    // y dominant
    if (N_abs.x > N_abs.z) {
      // other vertices of this primitive possibly x dominant
      Tangent = float3(0, 0, 1);
    } else {
      // other vertices of this primitive possibly z dominant
      Tangent = float3(1, 0, 0);
    }
  } else {
    // z dominant
    if (N_abs.x > N_abs.y) {
      // other vertices of this primitive possibly x dominant
      Tangent = float3(0, 1, 0);
    } else {
      // other vertices of this primitive possibly y dominant
      Tangent = float3(1, 0, 0);
    }
  }

  float3 Binormal = normalize(cross(Tangent, Input.Normal));
  Tangent = normalize(cross(Input.Normal, Binormal));

  float3x3 world_to_tangent = float3x3(Tangent, Binormal, Input.Normal);
  Output.LightDir = mul(world_to_tangent, Output.LightDir);
  Output.ViewDir = mul(world_to_tangent, Output.ViewDir);

  return Output;
}

float4 Block_PS(VS_BLOCK_OUTPUT Input) : SV_Target {
  const float2 texX = Input.WorldPos.yz*4;
  const float2 texY = Input.WorldPos.xz*3;
  const float2 texZ = Input.WorldPos.xy*4;

  float3 colorX = g_tDiffuseX.Sample(ssTrilinearRepeat, texX);
  float3 colorY = g_tDiffuseY.Sample(ssTrilinearRepeat, texY);
  float3 colorZ = g_tDiffuseZ.Sample(ssTrilinearRepeat, texZ);
  float3 normalX = g_tNormalX.Sample(ssTrilinearRepeat, texX)*2-1;
  float3 normalY = g_tNormalY.Sample(ssTrilinearRepeat, texY)*2-1;
  float3 normalZ = g_tNormalZ.Sample(ssTrilinearRepeat, texZ)*2-1;
  if (g_bDetailTex) {
    colorX *= g_tDetail.Sample(ssTrilinearRepeat, texX*7.97);
    colorY *= g_tDetail.Sample(ssTrilinearRepeat, texY*7.95);
    colorZ *= g_tDetail.Sample(ssTrilinearRepeat, texZ*7.98);
    normalX += g_tDetailNormals.Sample(ssTrilinearRepeat, texX*5)*2-1;
    normalY += g_tDetailNormals.Sample(ssTrilinearRepeat, texY*5)*2-1;
    normalZ += g_tDetailNormals.Sample(ssTrilinearRepeat, texZ*5)*2-1;
  }
  float3 N = normalize(Input.Normal);
  float3 blend_weights = abs(N) - 0.1;
  blend_weights *= 12;
  blend_weights = max(0, blend_weights);
  blend_weights = pow(blend_weights, 5);
  blend_weights /= dot(blend_weights, 1);
  float3 color = blend_weights.x*colorX + blend_weights.y*colorY + blend_weights.z*colorZ;
  float3 normal = blend_weights.x*normalX + blend_weights.y*normalY + blend_weights.z*normalZ;

  if (g_bNormalMapping) {
    N = normalize(normal + float3(0, 0, 1));
  } else {
    Input.LightDir = normalize(g_vLightDir);
    Input.ViewDir = normalize(g_vCamPos - Input.WorldPos);
  }
  float3 L = normalize(Input.LightDir);
  float3 H = normalize(Input.ViewDir + Input.LightDir);
  float4 coeffs = lit(dot(N, L), abs(dot(N, H)), 32);
  float intensity = dot(coeffs, float4(0.05, 0.5, 0.5, 0));

  //color = float4(Input.Tangent*0.5+0.5, 0);
  //color = float4(0.6 + 0.4*normalize(Input.Normal), 0);
  //return intensity*float4(normalize(Input.Tangent)*0.5+0.5, 0);
  color *= intensity;

  // Fog
  if (g_bFog) {
    float depth = length(g_vCamPos - Input.WorldPos);
    const float fFogStart = 5.0, fFogEnd = 7.5;
    color = lerp(color, float3(0.176, 0.196, 0.667), saturate((depth-fFogStart) / (fFogEnd - fFogStart)));
  }

  return float4(color, saturate(Input.Age*3));
}

RasterizerState rsWireframe {
  FillMode = WIREFRAME;
  //CullMode = NONE;
};

BlendState bsSrcAlphaBlendingAdd
{
  BlendEnable[0] = TRUE;
  SrcBlend = SRC_ALPHA;
  DestBlend = INV_SRC_ALPHA;
  BlendOp = ADD;
  RenderTargetWriteMask[0] = 0x0F;
};

DepthStencilState dssEnableDepth
{
  DepthEnable = TRUE;
  DepthWriteMask = ALL;
  DepthFunc = LESS;
};

technique10 RenderBlock {
  pass P0 {
    SetVertexShader(CompileShader(vs_4_0, Block_VS()));
    SetGeometryShader(NULL);
    SetPixelShader(CompileShader(ps_4_0, Block_PS()));
    SetDepthStencilState(dssEnableDepth, 0);
    SetBlendState(bsSrcAlphaBlendingAdd, float4(0, 0, 0, 0), 0xFFFFFFFF);
    SetRasterizerState(NULL);
  }
}



float4 Loading_VS(float4 vPosition : POSITION, out float2 vScreenPos : SCREENPOS) : SV_Position
{
  vScreenPos = vPosition.xy*0.5 + 0.5;
  vScreenPos.y = 1 - vScreenPos.y;
  return vPosition;
}

float4 Loading_PS(float4 vPosition : SV_Position, float2 vScreenPos : SCREENPOS) : SV_Target
{
  if (vScreenPos.y >= 0.45 && vScreenPos.y <= 0.55) {
    float x = vScreenPos.x*1.2-0.1;
    if (x >= 0 && x <= g_fLoaded) {
      float s = 1-sqrt(1-g_fLoaded);
      return float4(s, s, s, 1);
    }
    if ((x >= -0.025 && x <= -0.0125) || (x >= 1.0125 && x <= 1.025)) return float4(1, 1, 1, 1);
  }
  return float4(0, 0, 0, 1);
}

technique10 LoadingScreen {
  pass P0 {
    SetVertexShader(CompileShader(vs_4_0, Loading_VS()));
    SetGeometryShader(NULL);
    SetPixelShader(CompileShader(ps_4_0, Loading_PS()));
    SetDepthStencilState(dssDisableDepthStencil, 0);
    SetBlendState(NULL, float4(0, 0, 0, 0), 0xFFFFFFFF);
    SetRasterizerState(NULL);
  }
}

#include "postprocessing.fx"