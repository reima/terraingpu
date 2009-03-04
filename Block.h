#pragma once
#include "DXUT.h"
#include <functional>

struct BLOCK_ID {
  int x, y, z;
};

size_t std::tr1::hash<BLOCK_ID>::operator ()(const BLOCK_ID &id) const {
  return ((static_cast<size_t>(id.y) & 0x0FF) << 24) |
         ((static_cast<size_t>(id.x) & 0xFFF) << 12) |
         ((static_cast<size_t>(id.z) & 0xFFF) <<  0);
}

bool std::equal_to<BLOCK_ID>::operator ()(const BLOCK_ID &left, const BLOCK_ID &right) const {
  return left.x == right.x && left.y == right.y && left.z == right.z;
}

class Block {
 public:
  Block(const D3DXVECTOR3 &position);
  Block(const BLOCK_ID &id);
  ~Block(void);
  
  HRESULT Generate(ID3D10Device *device);
  void Draw(ID3D10Device *device, ID3D10EffectTechnique *technique) const;
  bool IsEmpty(void) const { return primitive_count_ == 0; }

  const BLOCK_ID &id(void) const { return id_; }

  static HRESULT OnCreateDevice(ID3D10Device *device);
  static HRESULT OnLoadEffect(ID3D10Device *device, ID3D10Effect *effect);
  static void OnDestroyDevice(void);

  // Constants
  static const UINT kVoxelDim;
  static const UINT kVoxelDimMinusOne;
  static const float kInvVoxelDim;
  static const float kInvVoxelDimMinusOne;
  static const UINT kMargin;
  static const UINT kVoxelDimWithMargins;
  static const UINT kVoxelDimWithMarginsMinusOne;
  static const float kInvVoxelDimWithMargins;
  static const float kInvVoxelDimWithMarginsMinusOne;
  static const float kBlockSize;

 private:
  // Generation steps
  HRESULT RenderDensityVolume(ID3D10Device *device);
  HRESULT GenerateTriangles(ID3D10Device *device);

  D3DXVECTOR3 position_;
  BLOCK_ID id_;
  UINT64 primitive_count_;

  // Rendering resources
  ID3D10Buffer *vertex_buffer_;
  static ID3D10InputLayout *input_layout_;

  // Generation resources
  static ID3D10Effect *effect_;
  static ID3D10Buffer *screen_aligned_quad_vb_;
  static ID3D10InputLayout *screen_aligned_quad_il_;
  static ID3D10Texture3D *density_volume_tex_;
  static ID3D10RenderTargetView *density_volume_rtv_;
  static ID3D10ShaderResourceView *density_volume_srv_;
  static ID3D10EffectShaderResourceVariable *density_volume_ev_;
  static ID3D10Buffer *voxel_slice_vb_;
  static ID3D10InputLayout *voxel_slice_il_;
  static ID3D10EffectVectorVariable *offset_ev_;
};
