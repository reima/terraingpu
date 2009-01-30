//--------------------------------------------------------------------------------------
// File: Effect.fx
//--------------------------------------------------------------------------------------
#include "Tables.fxh"
#include "Density.fxh"

cbuffer cb0 {
  float4x4 g_mWorldViewProj;
  float3   g_vCamPos;
}

cbuffer cb1 {
  uint VoxelDim = 33;
  uint VoxelDimMinusOne = 32;
  float2 InvVoxelDim = float2(1.0/33.0, 0);
  float2 InvVoxelDimMinusOne = float2(1.0/32.0, 0);
  uint Margin = 2;
  uint VoxelDimPlusMargins = 37;
  uint VoxelDimPlusMarginsMinusOne = 36;
  float2 InvVoxelDimPlusMargins = float2(1.0/37.0, 0);
  float2 InvVoxelDimPlusMarginsMinusOne = float2(1.0/36.0, 0);
  float BlockSize = 1.0;
}

cbuffer cb2 {
  float3 g_vBlockOffset = float3(1, 0, 0);
}

struct VS_DENSITY_INPUT {
  float2 Position   : POSITION;
  float2 Tex        : TEXCOORD;
  uint   InstanceID : SV_InstanceID;
};

struct VS_DENSITY_OUTPUT {
  float4 Position      : POSITION;
  float4 WorldPosition : WORLDPOSITION;
  float3 BlockPosition : BLOCKPOSITION;
  uint   InstanceID    : INSTANCEID;
};

struct GS_DENSITY_OUTPUT {
  float4 Position      : SV_Position;
  float4 WorldPosition : WORLDPOSITION;
  float3 BlockPosition : BLOCKPOSITION;
  uint   RTIndex       : SV_RenderTargetArrayIndex;
};

VS_DENSITY_OUTPUT Density_VS(VS_DENSITY_INPUT Input) {
  VS_DENSITY_OUTPUT Output;
  Output.Position = float4(Input.Position.xy, 0.5, 1);
  float3 vVolumePos = float3(Input.Tex.xy, Input.InstanceID * InvVoxelDimPlusMarginsMinusOne.x);
  // Texel (0,0) is half a texel away from "real" position of vertex at (-1,-1), so compensate for that:
  vVolumePos = (vVolumePos - float3(0.5, 0.5, 0.5)) * (1 + InvVoxelDimPlusMarginsMinusOne.x) + float3(0.5, 0.5, 0.5);
  Output.BlockPosition = (vVolumePos * VoxelDimPlusMargins.x - Margin.xxx) * InvVoxelDim.x;
  Output.WorldPosition = float4(g_vBlockOffset + Output.BlockPosition * BlockSize, 1);
  Output.InstanceID = Input.InstanceID;
  return Output;
}

[MaxVertexCount(3)]
void Density_GS(triangle VS_DENSITY_OUTPUT Input[3],
                inout TriangleStream<GS_DENSITY_OUTPUT> Stream) {
  GS_DENSITY_OUTPUT Output;
  [unroll] for (uint i = 0; i < 3; ++i) {
    Output.Position = Input[i].Position;
    Output.WorldPosition = Input[i].WorldPosition;
    Output.BlockPosition = Input[i].BlockPosition;
    Output.RTIndex = Input[i].InstanceID;
    Stream.Append(Output);
  }
  Stream.RestartStrip();
}

float Density_PS(GS_DENSITY_OUTPUT Input) : SV_Target {
  return DENSITY(Input.WorldPosition.xyz);
}


struct VS_GENTRIS_INPUT {
  uint2 Position   : POSITION;
  uint  InstanceID : SV_InstanceID;
};

struct VS_GENTRIS_OUTPUT {
  uint3 Position   : POSITION;
  uint  Case       : CASE;
};

struct GS_GENTRIS_OUTPUT {
  float3 Position  : POSITION;
  float3 Normal    : NORMAL;
};

Texture3D g_tDensityVolume;

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


VS_GENTRIS_OUTPUT GenTris_VS(VS_GENTRIS_INPUT Input) {
  int4 pos = int4(int3(Input.Position.xy, Input.InstanceID) + Margin.xxx, 0);
  float4 density0123;
  float4 density4567;
  density0123.x = g_tDensityVolume.Load(pos + int4(0, 0, 0, 0), 0);
  density0123.y = g_tDensityVolume.Load(pos + int4(0, 1, 0, 0), 0);
  density0123.z = g_tDensityVolume.Load(pos + int4(1, 1, 0, 0), 0);
  density0123.w = g_tDensityVolume.Load(pos + int4(1, 0, 0, 0), 0);
  density4567.x = g_tDensityVolume.Load(pos + int4(0, 0, 1, 0), 0);
  density4567.y = g_tDensityVolume.Load(pos + int4(0, 1, 1, 0), 0);
  density4567.z = g_tDensityVolume.Load(pos + int4(1, 1, 1, 0), 0);
  density4567.w = g_tDensityVolume.Load(pos + int4(1, 0, 1, 0), 0);

  uint4 i0123 = (uint4)saturate(density0123*99999);
  uint4 i4567 = (uint4)saturate(density4567*99999);

  VS_GENTRIS_OUTPUT Output;
  Output.Case = (i0123.x << 0) | (i0123.y << 1) | (i0123.z << 2) | (i0123.w << 3) |
                (i4567.x << 4) | (i4567.y << 5) | (i4567.z << 6) | (i4567.w << 7);
  Output.Position = int3(Input.Position.xy, Input.InstanceID);

  return Output;
}

float3 GetEdgeOffset(int3 pos, int edge) {
  pos += Margin.xxx;
  float d1 = g_tDensityVolume.Load(int4(pos + edgeStartCorner[edge], 0));
  float d2 = g_tDensityVolume.Load(int4(pos + edgeStartCorner[edge] + edgeDirection[edge], 0));
  return (edgeStartCorner[edge] + (d1/(d1-d2))*edgeDirection[edge]);
}

GS_GENTRIS_OUTPUT CreateVertex(int3 vBlockPos, int nEdge) {
  GS_GENTRIS_OUTPUT Output;
  float3 vEdgePos = (vBlockPos + GetEdgeOffset(vBlockPos, nEdge));
  float3 vVolumePos = (vEdgePos + Margin.xxx) * InvVoxelDimPlusMargins.x;
  Output.Position = g_vBlockOffset + vEdgePos * InvVoxelDimMinusOne.x * BlockSize;
  float3 grad;
  grad.x = g_tDensityVolume.SampleLevel(ssTrilinearClamp, vVolumePos + InvVoxelDimPlusMargins.xyy, 0) -
           g_tDensityVolume.SampleLevel(ssTrilinearClamp, vVolumePos - InvVoxelDimPlusMargins.xyy, 0);
  grad.y = g_tDensityVolume.SampleLevel(ssTrilinearClamp, vVolumePos + InvVoxelDimPlusMargins.yxy, 0) -
           g_tDensityVolume.SampleLevel(ssTrilinearClamp, vVolumePos - InvVoxelDimPlusMargins.yxy, 0);
  grad.z = g_tDensityVolume.SampleLevel(ssTrilinearClamp, vVolumePos + InvVoxelDimPlusMargins.yyx, 0) -
           g_tDensityVolume.SampleLevel(ssTrilinearClamp, vVolumePos - InvVoxelDimPlusMargins.yyx, 0);
  Output.Normal = -normalize(grad);
  return Output;
}

[MaxVertexCount(15)]
void GenTris_GS(point VS_GENTRIS_OUTPUT Input[1],
                inout TriangleStream<GS_GENTRIS_OUTPUT> Stream) {
  const uint nTris = numTris[Input[0].Case];
  const int3 vBlockPos = Input[0].Position;
  for (uint i = 0; i < nTris; ++i) {
    const int3 edges = triTable[Input[0].Case][i];
    Stream.Append(CreateVertex(vBlockPos, edges.x));
    Stream.Append(CreateVertex(vBlockPos, edges.y));
    Stream.Append(CreateVertex(vBlockPos, edges.z));
    Stream.RestartStrip();
  }
}

DepthStencilState dssDisableDepthStencil {
  DepthEnable = FALSE;
  StencilEnable = FALSE;
};

technique10 GenBlock {
  pass build_densities {
    SetVertexShader(CompileShader(vs_4_0, Density_VS()));
    SetGeometryShader(CompileShader(gs_4_0, Density_GS()));
    SetPixelShader(CompileShader(ps_4_0, Density_PS()));
    SetDepthStencilState(dssDisableDepthStencil, 0);
    SetRasterizerState(NULL);
  }
  pass gen_vertices {
    SetVertexShader(CompileShader(vs_4_0, GenTris_VS()));
    SetGeometryShader(ConstructGSWithSO(CompileShader(gs_4_0, GenTris_GS()), "POSITION.xyz; NORMAL.xyz"));
    SetPixelShader(NULL);
    SetDepthStencilState(dssDisableDepthStencil, 0);
  }
}

struct VS_BLOCK_INPUT {
  float3 Position : POSITION;
  float3 Normal   : NORMAL;
};

struct VS_BLOCK_OUTPUT {
  float4 Position : SV_Position;
  float3 Normal   : NORMAL;
  float3 WorldPos : WORLDPOS;
};

VS_BLOCK_OUTPUT Block_VS(VS_BLOCK_INPUT Input) {
  VS_BLOCK_OUTPUT Output;
  Output.Position = mul(float4(Input.Position, 1), g_mWorldViewProj);
  Output.Normal = Input.Normal;
  Output.WorldPos = Input.Position;
  return Output;
}

float4 Block_PS(VS_BLOCK_OUTPUT Input) : SV_Target {
  float3 N = normalize(Input.Normal);
  float3 L = normalize(g_vCamPos - Input.WorldPos);
  float3 H = L;
  float4 coeffs = lit(dot(N, L), dot(N, H), 128);
  float intensity = dot(coeffs, float4(0.01, 0.29, 0.7, 0));
  return intensity*float4(Input.Normal*0.5+0.5, 0);
}

RasterizerState rsWireframe {
  //FillMode = WIREFRAME;
  CullMode = NONE;
};

technique10 RenderBlock {
  pass P0 {
    SetVertexShader(CompileShader(vs_4_0, Block_VS()));
    SetGeometryShader(NULL);
    SetPixelShader(CompileShader(ps_4_0, Block_PS()));
    SetDepthStencilState(NULL, 0);
    SetRasterizerState(rsWireframe);
  }
}
