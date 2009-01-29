//--------------------------------------------------------------------------------------
// File: Effect.fx
//--------------------------------------------------------------------------------------
#include "Tables.fxh"
#include "Density.fxh"

cbuffer cb0 {
  float4x4 g_mWorldViewProj;
}

cbuffer cb1 {
  float VoxelDim = 33;
  float VoxelDimMinusOne = 32;
  float BlockSize = 1.0;
  float2 InvVoxelDim = float2(1.0/33.0, 0);
  float2 InvVoxelDimMinusOne = float2(1.0/32.0, 0);
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
  Output.BlockPosition = float3(Input.Tex.xy, Input.InstanceID * InvVoxelDimMinusOne.x);
  Output.WorldPosition = float4(Output.BlockPosition*BlockSize/* + g_vBlockPosition*/, 1);
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
  float2 Position   : POSITION;
  uint   InstanceID : SV_InstanceID;
};

struct VS_GENTRIS_OUTPUT {
  float3 Position   : POSITION;
  uint   Case       : CASE;
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
  float3 uvw = float3(Input.Position.xy, Input.InstanceID * InvVoxelDimMinusOne.x);
  float4 density0123;
  float4 density4567;
  density0123.x = g_tDensityVolume.SampleLevel(ssNearestClamp, uvw + InvVoxelDimMinusOne.yyy, 0);
  density0123.y = g_tDensityVolume.SampleLevel(ssNearestClamp, uvw + InvVoxelDimMinusOne.yxy, 0);
  density0123.z = g_tDensityVolume.SampleLevel(ssNearestClamp, uvw + InvVoxelDimMinusOne.xxy, 0);
  density0123.w = g_tDensityVolume.SampleLevel(ssNearestClamp, uvw + InvVoxelDimMinusOne.xyy, 0);
  density4567.x = g_tDensityVolume.SampleLevel(ssNearestClamp, uvw + InvVoxelDimMinusOne.yyx, 0);
  density4567.y = g_tDensityVolume.SampleLevel(ssNearestClamp, uvw + InvVoxelDimMinusOne.yxx, 0);
  density4567.z = g_tDensityVolume.SampleLevel(ssNearestClamp, uvw + InvVoxelDimMinusOne.xxx, 0);
  density4567.w = g_tDensityVolume.SampleLevel(ssNearestClamp, uvw + InvVoxelDimMinusOne.xyx, 0);

  uint4 i0123 = (uint4)saturate(density0123*99999);
  uint4 i4567 = (uint4)saturate(density4567*99999);

  VS_GENTRIS_OUTPUT Output;
  Output.Case = (i0123.x << 0) | (i0123.y << 1) | (i0123.z << 2) | (i0123.w << 3) |
                (i4567.x << 4) | (i4567.y << 5) | (i4567.z << 6) | (i4567.w << 7);
  Output.Position = uvw;

  return Output;
}

float3 GetVertexFromEdge(float3 pos, int edge) {
  float d1 = g_tDensityVolume.SampleLevel(ssNearestClamp, pos + edgeStartCorner[edge] * InvVoxelDimMinusOne.x, 0);
  float d2 = g_tDensityVolume.SampleLevel(ssNearestClamp, pos + (edgeStartCorner[edge] + edgeDirection[edge]) * InvVoxelDimMinusOne.x, 0);
  return pos + (edgeStartCorner[edge] + (d1/(d1-d2))*edgeDirection[edge]) * InvVoxelDimMinusOne.x;
}

GS_GENTRIS_OUTPUT ladida(float3 pos, int edge) {
  GS_GENTRIS_OUTPUT Output;
  float3 bsPos = GetVertexFromEdge(pos, edge);
  Output.Position = GetVertexFromEdge(pos, edge) * BlockSize;
  float3 grad;
  grad.x = g_tDensityVolume.SampleLevel(ssTrilinearClamp, bsPos + InvVoxelDimMinusOne.xyy, 0) -
           g_tDensityVolume.SampleLevel(ssTrilinearClamp, bsPos - InvVoxelDimMinusOne.xyy, 0);
  grad.y = g_tDensityVolume.SampleLevel(ssTrilinearClamp, bsPos + InvVoxelDimMinusOne.yxy, 0) -
           g_tDensityVolume.SampleLevel(ssTrilinearClamp, bsPos - InvVoxelDimMinusOne.yxy, 0);
  grad.z = g_tDensityVolume.SampleLevel(ssTrilinearClamp, bsPos + InvVoxelDimMinusOne.yyx, 0) -
           g_tDensityVolume.SampleLevel(ssTrilinearClamp, bsPos - InvVoxelDimMinusOne.yyx, 0);
  Output.Normal = -normalize(grad);
  return Output;
}

[MaxVertexCount(15)]
void GenTris_GS(point VS_GENTRIS_OUTPUT Input[1],
                inout TriangleStream<GS_GENTRIS_OUTPUT> Stream) {
  const uint nTris = numTris[Input[0].Case];
  for (uint i = 0; i < nTris; ++i) {
    const int3 edges = triTable[Input[0].Case][i];
    Stream.Append(ladida(Input[0].Position, edges.x));
    Stream.Append(ladida(Input[0].Position, edges.y));
    Stream.Append(ladida(Input[0].Position, edges.z));    
    Stream.RestartStrip();
  }  
}

DepthStencilState dssDisableDepthStencil {
  DepthEnable = FALSE;
  StencilEnable = FALSE;
};

GeometryShader gsGenTris = ConstructGSWithSO(
  CompileShader(gs_4_0, GenTris_GS()),
  "POSITION.xyz; NORMAL.xyz");

technique10 GenBlock {
  pass P0 {
    SetVertexShader(CompileShader(vs_4_0, Density_VS()));
    SetGeometryShader(CompileShader(gs_4_0, Density_GS()));
    SetPixelShader(CompileShader(ps_4_0, Density_PS()));
    SetDepthStencilState(dssDisableDepthStencil, 0);
  }
  pass P1 {
    SetVertexShader(CompileShader(vs_4_0, GenTris_VS()));
    SetGeometryShader(gsGenTris);
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
};

VS_BLOCK_OUTPUT Block_VS(VS_BLOCK_INPUT Input) {
  VS_BLOCK_OUTPUT Output;
  Output.Position = mul(float4(Input.Position, 1), g_mWorldViewProj);
  Output.Normal = Input.Normal;
  return Output;
}

float4 Block_PS(VS_BLOCK_OUTPUT Input) : SV_Target {
  return float4(Input.Normal, 0);
}

technique10 RenderBlock {
  pass P0 {
    SetVertexShader(CompileShader(vs_4_0, Block_VS()));
    SetGeometryShader(NULL);
    SetPixelShader(CompileShader(ps_4_0, Block_PS()));
    SetDepthStencilState(NULL, 0);
  }
}
