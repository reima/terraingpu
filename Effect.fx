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
  uint VoxelDimWithMargins = 37;
  uint VoxelDimWithMarginsMinusOne = 36;
  float2 InvVoxelDimWithMargins = float2(1.0/37.0, 0);
  float2 InvVoxelDimWithMarginsMinusOne = float2(1.0/36.0, 0);
  float BlockSize = 1.0;
}

cbuffer cb2 {
  float3 g_vBlockOffset = float3(0, 0, 0);
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
  float3 vVolumePos = float3(Input.Tex.xy, Input.InstanceID * InvVoxelDimWithMarginsMinusOne.x);
  // Texel (0,0) is half a texel away from "real" position of vertex at (-1,-1), so compensate for that:
  vVolumePos.xy = (vVolumePos - float2(0.5, 0.5)) * (1 + InvVoxelDimMinusOne.x) + float2(0.5, 0.5);
  Output.BlockPosition.xy = (vVolumePos.xy * VoxelDimWithMargins.x - Margin.xx) * InvVoxelDim.x;
  Output.BlockPosition.z = (vVolumePos.z * VoxelDimWithMarginsMinusOne.x - Margin) * InvVoxelDimMinusOne.x;
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



struct VS_LISTTRIS_INPUT {
  uint2 Position   : POSITION;
  uint  InstanceID : SV_InstanceID;
};

struct VS_LISTTRIS_OUTPUT {
  uint3 Position   : POSITION;
  uint  Case       : CASE;
};

struct GS_LISTTRIS_OUTPUT {
  uint Marker : MARKER; // z6_y6_x6_edge1_edge2_edge3
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

VS_LISTTRIS_OUTPUT ListTris_VS(VS_LISTTRIS_INPUT Input) {
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

  VS_LISTTRIS_OUTPUT Output;
  Output.Case = (i0123.x << 0) | (i0123.y << 1) | (i0123.z << 2) | (i0123.w << 3) |
                (i4567.x << 4) | (i4567.y << 5) | (i4567.z << 6) | (i4567.w << 7);
  Output.Position = int3(Input.Position.xy, Input.InstanceID);

  return Output;
}

[MaxVertexCount(5)]
void ListTris_GS(point VS_LISTTRIS_OUTPUT Input[1],
                 inout PointStream<GS_LISTTRIS_OUTPUT> Stream) {
  const uint nTris = numTris[Input[0].Case];
  const int3 vBlockPos = Input[0].Position;
  GS_LISTTRIS_OUTPUT Output;
  for (uint i = 0; i < nTris; ++i) {
    const int3 edges = triTable[Input[0].Case][i];
    // Warning: This only works with VoxelDim 33 (or less)!
    Output.Marker = ((Input[0].Position.z & 0x3F) << 26) |
                    ((Input[0].Position.y & 0x3F) << 20) |
                    ((Input[0].Position.x & 0x3F) << 14) |
                    ((edges[0] & 0x0F) << 8) |
                    ((edges[1] & 0x0F) << 4) |
                    ((edges[2] & 0x0F) << 0);
    Stream.Append(Output);
  }
}



struct VS_GENTRIS_INPUT {
  uint Marker : MARKER; // z6_y6_x6_edge1_edge2_edge3
};

struct VS_GENTRIS_OUTPUT {
  float3 Position1  : POSITION1;
  float3 Normal1    : NORMAL1;
  float3 Position2  : POSITION2;
  float3 Normal2    : NORMAL2;
  float3 Position3  : POSITION3;
  float3 Normal3    : NORMAL3;
};

struct GS_GENTRIS_OUTPUT {
  float3 Position  : POSITION;
  float3 Normal    : NORMAL;
};

float3 GetEdgeOffset(int3 pos, int edge) {
  pos += Margin.xxx;
  float d1 = g_tDensityVolume.Load(int4(pos + edgeStartCorner[edge], 0));
  float d2 = g_tDensityVolume.Load(int4(pos + edgeStartCorner[edge] + edgeDirection[edge], 0));
  return (edgeStartCorner[edge] + (d1/(d1-d2))*edgeDirection[edge]);
}

float3 GetNormal(float3 vVolumePos) {
  float3 grad;
  grad.x = g_tDensityVolume.SampleLevel(ssTrilinearClamp, vVolumePos + InvVoxelDimWithMargins.xyy, 0) -
           g_tDensityVolume.SampleLevel(ssTrilinearClamp, vVolumePos - InvVoxelDimWithMargins.xyy, 0);
  grad.y = g_tDensityVolume.SampleLevel(ssTrilinearClamp, vVolumePos + InvVoxelDimWithMargins.yxy, 0) -
           g_tDensityVolume.SampleLevel(ssTrilinearClamp, vVolumePos - InvVoxelDimWithMargins.yxy, 0);
  grad.z = g_tDensityVolume.SampleLevel(ssTrilinearClamp, vVolumePos + InvVoxelDimWithMargins.yyx, 0) -
           g_tDensityVolume.SampleLevel(ssTrilinearClamp, vVolumePos - InvVoxelDimWithMargins.yyx, 0);
  return -normalize(grad);
}

void CreateVertex(int3 vCellPos, int nEdge, out float3 vPosition, out float3 vNormal) {
  float3 vEdgePos = (vCellPos + GetEdgeOffset(vCellPos, nEdge));
  float3 vVolumePos = (vEdgePos + Margin.xxx) * InvVoxelDimWithMargins.x;
  vPosition = g_vBlockOffset + vEdgePos * InvVoxelDimMinusOne.x * BlockSize;
  vNormal = GetNormal(vVolumePos);
}

VS_GENTRIS_OUTPUT GenTris_VS(VS_GENTRIS_INPUT Input) {
  VS_GENTRIS_OUTPUT Output;
  const int3 vCellPos = int3((Input.Marker >> 14) & 0x3F,
                             (Input.Marker >> 20) & 0x3F,
                             (Input.Marker >> 26) & 0x3F);
  CreateVertex(vCellPos, (Input.Marker >> 8) & 0x0F, Output.Position1, Output.Normal1);
  CreateVertex(vCellPos, (Input.Marker >> 4) & 0x0F, Output.Position2, Output.Normal2);
  CreateVertex(vCellPos, (Input.Marker >> 0) & 0x0F, Output.Position3, Output.Normal3);

  return Output;
}

[MaxVertexCount(3)]
void GenTris_GS(point VS_GENTRIS_OUTPUT Input[1],
                inout TriangleStream<GS_GENTRIS_OUTPUT> Stream) {
  GS_GENTRIS_OUTPUT Output;
  Output.Position = Input[0].Position1;
  Output.Normal   = Input[0].Normal1;
  Stream.Append(Output);
  Output.Position = Input[0].Position2;
  Output.Normal   = Input[0].Normal2;
  Stream.Append(Output);
  Output.Position = Input[0].Position3;
  Output.Normal   = Input[0].Normal3;
  Stream.Append(Output);
  Stream.RestartStrip();
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
  pass list_triangles {
    SetVertexShader(CompileShader(vs_4_0, ListTris_VS()));
    SetGeometryShader(ConstructGSWithSO(CompileShader(gs_4_0, ListTris_GS()), "MARKER.x"));
    SetPixelShader(NULL);
    SetDepthStencilState(dssDisableDepthStencil, 0);
  }
  pass gen_vertices {
    SetVertexShader(CompileShader(vs_4_0, GenTris_VS()));
    SetGeometryShader(ConstructGSWithSO(CompileShader(gs_4_0, GenTris_GS()), "POSITION.xyz; NORMAL.xyz"));
    SetPixelShader(NULL);
    SetDepthStencilState(dssDisableDepthStencil, 0);
  }
}


struct VS_LISTCELLS_INPUT {
  uint2 Position   : POSITION;
  uint  InstanceID : SV_InstanceID;
};

struct VS_LISTCELLS_OUTPUT {
  uint Cell : CELL; // z8_y8_x8_case8
};

struct GS_LISTCELLS_OUTPUT {
  uint Cell : CELL; // z8_y8_x8_case8
};

VS_LISTCELLS_OUTPUT ListCells_VS(VS_LISTCELLS_INPUT Input) {
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

  VS_LISTCELLS_OUTPUT Output;
  uint nCase = (i0123.x << 0) | (i0123.y << 1) | (i0123.z << 2) | (i0123.w << 3) |
               (i4567.x << 4) | (i4567.y << 5) | (i4567.z << 6) | (i4567.w << 7);
  int3 vPosition = int3(Input.Position.xy, Input.InstanceID);
  Output.Cell = ((vPosition.z & 0xFF) << 24) |
                ((vPosition.y & 0xFF) << 16) |
                ((vPosition.x & 0xFF) <<  8) |
                nCase;

  return Output;
}

[MaxVertexCount(1)]
void ListCells_GS(point VS_LISTCELLS_OUTPUT Input[1],
                 inout PointStream<GS_LISTCELLS_OUTPUT> Stream) {
  if (numTris[Input[0].Cell & 0xFF] > 0) {
    Stream.Append(Input[0]);
  }
}


struct VS_LISTVERTS_INPUT {
  uint Cell : CELL; // z8_y8_x8_case8
};

struct VS_LISTVERTS_OUTPUT {
  uint Cell : CELL; // z8_y8_x8_edgeflags8
};

const uint EDGE0 = 0x02;  // 00010b
const uint EDGE3 = 0x08;  // 01000b
const uint EDGE8 = 0x10;  // 10000b

struct GS_LISTVERTS_OUTPUT {
  uint Edge : EDGE; // z8_y8_x8_null4_edge4
};

VS_LISTVERTS_OUTPUT ListVerts_VS(VS_LISTVERTS_INPUT Input) {
  uint edgeFlags = (Input.Cell & 0xFF) ^ ((Input.Cell & 1) * (EDGE0 | EDGE3 | EDGE8));
  Input.Cell = (Input.Cell & 0xFFFFFF00) | edgeFlags;
  return Input;
}

[MaxVertexCount(3)]
void ListVerts_GS(point VS_LISTVERTS_OUTPUT Input[1],
                 inout PointStream<GS_LISTVERTS_OUTPUT> Stream) {
  GS_LISTVERTS_OUTPUT Output;
  uint pos = Input[0].Cell & 0xFFFFFF00;
  //if (Input[0].Cell & EDGE3) {
    Output.Edge = pos | 3;
    Stream.Append(Output);
  //}
  //if (Input[0].Cell & EDGE0) {
    Output.Edge = pos | 0;
    Stream.Append(Output);
  //}
  //if (Input[0].Cell & EDGE8) {
    Output.Edge = pos | 8;
    Stream.Append(Output);
  //}
}


struct VS_GENVERTS_INPUT {
  uint Edge : EDGE; // z8_y8_x8_null4_edge4
};

struct VS_GENVERTS_OUTPUT {
  float3 Position  : POSITION;
  float3 Normal    : NORMAL;
};

struct GS_GENVERTS_OUTPUT {
  float3 Position  : POSITION;
  float3 Normal    : NORMAL;
};

VS_GENVERTS_OUTPUT GenVerts_VS(VS_GENVERTS_INPUT Input) {
  VS_GENVERTS_OUTPUT Output;
  const int3 vCellPos = int3((Input.Edge >>  8) & 0xFF,
                             (Input.Edge >> 16) & 0xFF,
                             (Input.Edge >> 24) & 0xFF);
  CreateVertex(vCellPos, Input.Edge & 0x0F, Output.Position, Output.Normal);
  return Output;
}

[MaxVertexCount(1)]
void GenVerts_GS(point VS_GENVERTS_OUTPUT Input[1],
                 inout PointStream<GS_GENVERTS_OUTPUT> Stream) {
  Stream.Append(Input[0]);
}

technique10 GenBlock3 {
  pass build_densities {
    SetVertexShader(CompileShader(vs_4_0, Density_VS()));
    SetGeometryShader(CompileShader(gs_4_0, Density_GS()));
    SetPixelShader(CompileShader(ps_4_0, Density_PS()));
    SetDepthStencilState(dssDisableDepthStencil, 0);
    SetRasterizerState(NULL);
  }
  pass list_nonempty_cells {
    SetVertexShader(CompileShader(vs_4_0, ListCells_VS()));
    SetGeometryShader(ConstructGSWithSO(CompileShader(gs_4_0, ListCells_GS()), "CELL.x"));
    SetPixelShader(NULL);
    SetDepthStencilState(dssDisableDepthStencil, 0);
  }
  pass list_verts_to_generate {
    SetVertexShader(CompileShader(vs_4_0, ListVerts_VS()));
    SetGeometryShader(ConstructGSWithSO(CompileShader(gs_4_0, ListVerts_GS()), "EDGE.x"));
    SetPixelShader(NULL);
    SetDepthStencilState(dssDisableDepthStencil, 0);
  }
  pass gen_vertices {
    SetVertexShader(CompileShader(vs_4_0, GenVerts_VS()));
    SetGeometryShader(ConstructGSWithSO(CompileShader(gs_4_0, GenVerts_GS()), "POSITION.xyz; NORMAL.xyz"));
    SetPixelShader(NULL);
    SetDepthStencilState(dssDisableDepthStencil, 0);
  }
}


struct VS_SPLATIDS_INPUT {
  uint Edge       : EDGE; // z8_y8_x8_null4_edge4
  uint VertexID   : SV_VertexID;
};

struct VS_SPLATIDS_OUTPUT {
  float4 Position      : POSITION;
  uint   VertexID      : VERTEXID;
  uint   RTIndex       : RTINDEX;
};

struct GS_SPLATIDS_OUTPUT {
  float4 Position      : SV_Position;
  uint   VertexID      : VERTEXID;
  uint   RTIndex       : SV_RenderTargetArrayIndex;
};

VS_SPLATIDS_OUTPUT SplatIDs_VS(VS_SPLATIDS_INPUT Input) {
  VS_SPLATIDS_OUTPUT Output;
  int3 vCellPos = int3((Input.Edge >>  8) & 0xFF,
                       (Input.Edge >> 16) & 0xFF,
                       (Input.Edge >> 24) & 0xFF);
  const int nEdge = Input.Edge & 0x0F;

  vCellPos.x *= 3;
  switch (nEdge) {
    case 3: vCellPos.x += 0; break;
    case 0: vCellPos.x += 1; break;
    case 8: vCellPos.x += 2; break;
  }
  float2 xy = vCellPos.xy;
  xy.x += 0.5;
  xy.y += 0.5;
  xy.x *= InvVoxelDim.x/3.0;
  xy.y *= InvVoxelDim.x;

  Output.Position = float4(xy, 0.5, 1);
  Output.Position.xy = Output.Position.xy * 2 - 1; // [-1..1] range
  Output.Position.y *= -1; // flip y

  Output.VertexID = Input.VertexID;
  Output.RTIndex = vCellPos.z;

  return Output;
}

[MaxVertexCount(1)]
void SplatIDs_GS(point VS_SPLATIDS_OUTPUT Input[1],
                 inout PointStream<GS_SPLATIDS_OUTPUT> Stream) {
  Stream.Append(Input[0]);
}

uint SplatIDs_PS(GS_SPLATIDS_OUTPUT Input) : SV_Target {
  return Input.VertexID;
}


struct VS_GENINDICES_INPUT {
  uint Cell : CELL; // z8_y8_x8_case8
};

struct VS_GENINDICES_OUTPUT {
  uint Cell : CELL; // z8_y8_x8_case8
};

struct GS_GENINDICES_OUTPUT {
  uint Index : INDEX;
};

Texture3D<uint> g_tIndicesVolume;

VS_GENINDICES_OUTPUT GenIndices_VS(VS_GENINDICES_INPUT Input) {
  return Input;
}

[MaxVertexCount(15)]
void GenIndices_GS(point VS_GENINDICES_OUTPUT Input[1],
                   inout PointStream<GS_GENINDICES_OUTPUT> Stream) {
  const uint nCase = Input[0].Cell & 0xFF;
  const uint nTris = numTris[nCase];
  const int3 vCellPos = int3((Input[0].Cell >>  8) & 0xFF,
                             (Input[0].Cell >> 16) & 0xFF,
                             (Input[0].Cell >> 24) & 0xFF);
  if (vCellPos.x == VoxelDimMinusOne.x || vCellPos.y == VoxelDimMinusOne.x || vCellPos.z == VoxelDimMinusOne.x) return;
  GS_GENINDICES_OUTPUT Output;
  int3 vEdgePos;
  for (uint i = 0; i < nTris; ++i) {
    const int3 edges = triTable[nCase][i];
    vEdgePos = vCellPos + edgeStartCorner[edges.x];
    vEdgePos.x = vEdgePos.x*3 + edgeAxis[edges.x];
    Output.Index = g_tIndicesVolume.Load(int4(vEdgePos, 0));
    Stream.Append(Output);

    vEdgePos = vCellPos + edgeStartCorner[edges.y];
    vEdgePos.x = vEdgePos.x*3 + edgeAxis[edges.y];
    Output.Index = g_tIndicesVolume.Load(int4(vEdgePos, 0));
    Stream.Append(Output);

    vEdgePos = vCellPos + edgeStartCorner[edges.z];
    vEdgePos.x = vEdgePos.x*3 + edgeAxis[edges.z];
    Output.Index = g_tIndicesVolume.Load(int4(vEdgePos, 0));
    Stream.Append(Output);
  }
}

technique10 GenIndices {
  pass splat_vertex_ids {
    SetVertexShader(CompileShader(vs_4_0, SplatIDs_VS()));
    SetGeometryShader(CompileShader(gs_4_0, SplatIDs_GS()));
    SetPixelShader(CompileShader(ps_4_0, SplatIDs_PS()));
    SetDepthStencilState(dssDisableDepthStencil, 0);
    SetRasterizerState(NULL);
  }
  pass gen_indices {
    SetVertexShader(CompileShader(vs_4_0, GenIndices_VS()));
    SetGeometryShader(ConstructGSWithSO(CompileShader(gs_4_0, GenIndices_GS()), "INDEX.x"));
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
  float3 LightDir : LIGHTDIR;
  float3 ViewDir  : VIEWDIR;
  float3 WorldPos : WORLDPOS;
  float2 Depth    : DEPTH;
};

Texture2D g_tDiffuse;
Texture2D g_tNormal;
Texture2D g_tBump;

VS_BLOCK_OUTPUT Block_VS(VS_BLOCK_INPUT Input) {
  VS_BLOCK_OUTPUT Output;
  Output.Position = mul(float4(Input.Position, 1), g_mWorldViewProj);
  Output.Depth = Output.Position.zw;
  Output.Normal = Input.Normal;
  Output.WorldPos = Input.Position;
  //Output.LightDir = normalize(g_vCamPos - Input.Position);
  Output.LightDir = normalize(float3(1, 1, 0));
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
  float3 colorX = g_tDiffuse.Sample(ssTrilinearRepeat, Input.WorldPos.yz*5);
  float3 colorY = g_tDiffuse.Sample(ssTrilinearRepeat, Input.WorldPos.xz*5);
  float3 colorZ = g_tDiffuse.Sample(ssTrilinearRepeat, Input.WorldPos.xy*5);
  float3 normalX = g_tNormal.Sample(ssTrilinearRepeat, Input.WorldPos.yz*5)*2-1;
  float3 normalY = g_tNormal.Sample(ssTrilinearRepeat, Input.WorldPos.xz*5)*2-1;
  float3 normalZ = g_tNormal.Sample(ssTrilinearRepeat, Input.WorldPos.xy*5)*2-1;
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
  float intensity = dot(coeffs, float4(0.05, 0.45, 0.25, 0));

  //color = float4(Input.Tangent*0.5+0.5, 0);
  color = float4(0.6 + 0.4*normalize(Input.Normal), 0);
  //return intensity*float4(normalize(Input.Tangent)*0.5+0.5, 0);
  color *= intensity;

  // Fog
  float depth = Input.Depth.x / Input.Depth.y;
  color = lerp(color, float3(0.176, 0.196, 0.667), saturate(depth - 0.995)*200);

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
    //SetRasterizerState(rsWireframe);
  }
}
