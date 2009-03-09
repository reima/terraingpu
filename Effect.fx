//--------------------------------------------------------------------------------------
// File: Effect.fx
//--------------------------------------------------------------------------------------
cbuffer cb0 {
  float4x4 g_mWorldViewProj;
  float3   g_vCamPos;
}

cbuffer cb2 {
  float3 g_vBlockOffset = float3(0, 0, 0);
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

Texture2D g_tDiffuseX;
Texture2D g_tDiffuseY;
Texture2D g_tDiffuseZ;
Texture2D g_tNormalX;
Texture2D g_tNormalY;
Texture2D g_tNormalZ;
Texture2D g_tBump;

VS_BLOCK_OUTPUT Block_VS(VS_BLOCK_INPUT Input) {
  VS_BLOCK_OUTPUT Output;
  Output.Position = mul(float4(Input.Position, 1), g_mWorldViewProj);
  Output.Depth = Output.Position.zw;
  Output.Normal = Input.Normal;
  Output.WorldPos = Input.Position;
  //Output.LightDir = normalize(g_vCamPos - Input.Position);
  Output.LightDir = normalize(float3(1, 1, 1));
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
  float2 texX = Input.WorldPos.yz*2;
  float2 texY = Input.WorldPos.xz*2;
  float2 texZ = Input.WorldPos.xy*2;

  float3 colorX = g_tDiffuseX.Sample(ssTrilinearRepeat, texX);
  float3 colorY = g_tDiffuseY.Sample(ssTrilinearRepeat, texY);
  float3 colorZ = g_tDiffuseZ.Sample(ssTrilinearRepeat, texZ);
  float3 normalX = g_tNormalX.Sample(ssTrilinearRepeat, texX)*2-1;
  float3 normalY = g_tNormalY.Sample(ssTrilinearRepeat, texY)*2-1;
  float3 normalZ = g_tNormalZ.Sample(ssTrilinearRepeat, texZ)*2-1;
  float3 N = normalize(Input.Normal);
  float3 blend_weights = abs(N) - 0.1;
  blend_weights *= 12;
  blend_weights = max(0, blend_weights);
  blend_weights = pow(blend_weights, 5);
  blend_weights /= dot(blend_weights, 1);
  float3 color = blend_weights.x*colorX + blend_weights.y*colorY + blend_weights.z*colorZ;
  float3 normal = blend_weights.x*normalX + blend_weights.y*normalY + blend_weights.z*normalZ;

  N = normalize(float3(normal.xy, 1));
  float3 L = normalize(Input.LightDir);
  float3 H = normalize(Input.ViewDir + Input.LightDir);
  float4 coeffs = lit(dot(N, L), dot(N, H), 32);
  float intensity = dot(coeffs, float4(0.05, 0.5, 0.5, 0));

  //color = float4(Input.Tangent*0.5+0.5, 0);
  //color = float4(0.6 + 0.4*normalize(Input.Normal), 0);
  //return intensity*float4(normalize(Input.Tangent)*0.5+0.5, 0);
  color *= intensity;

  // Fog
  float depth = Input.Depth.x / Input.Depth.y;
  color = lerp(color, float3(0.176, 0.196, 0.667), saturate(saturate(depth) - 0.999)*1000);

  return float4(color, 0);
}

RasterizerState rsWireframe {
  FillMode = WIREFRAME;
  //CullMode = NONE;
};

technique10 RenderBlock {
  pass P0 {
    SetVertexShader(CompileShader(vs_4_0, Block_VS()));
    SetGeometryShader(NULL);
    SetPixelShader(CompileShader(ps_4_0, Block_PS()));
    SetDepthStencilState(NULL, 0);
    SetBlendState(NULL, float4(0, 0, 0, 0), 0xFFFFFFFF);
    SetRasterizerState(NULL);
  }
}
