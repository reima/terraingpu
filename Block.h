#pragma once
#include "DXUT.h"

class Block {
 public:
  Block(const D3DXVECTOR3 &position, float size);
  ~Block(void);
  
  HRESULT Generate(ID3D10Device *device);
  void Draw(ID3D10Device *device, ID3D10EffectTechnique *technique) const;
  bool IsEmpty(void) const { return primitive_count_ == 0; }

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
  float size_;
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
