#include "Block.h"

const UINT Block::kVoxelDim = 33;
const UINT Block::kVoxelDimMinusOne = Block::kVoxelDim-1;
const float Block::kInvVoxelDim = 1.0f/Block::kVoxelDim;
const float Block::kInvVoxelDimMinusOne = 1.0f/Block::kVoxelDimMinusOne;
const UINT Block::kMargin = 2;
const UINT Block::kVoxelDimWithMargins = Block::kVoxelDim+2*Block::kMargin;
const UINT Block::kVoxelDimWithMarginsMinusOne = Block::kVoxelDimWithMargins-1;
const float Block::kInvVoxelDimWithMargins = 1.0f/Block::kVoxelDimWithMargins;
const float Block::kInvVoxelDimWithMarginsMinusOne = 1.0f/Block::kVoxelDimWithMarginsMinusOne;
const float Block::kBlockSize = 1.0f;

ID3D10InputLayout *Block::input_layout_ = NULL;
ID3D10Buffer *Block::screen_aligned_quad_vb_ = NULL;
ID3D10InputLayout *Block::screen_aligned_quad_il_ = NULL;
ID3D10Texture3D *Block::density_volume_tex_ = NULL;
ID3D10RenderTargetView *Block::density_volume_rtv_ = NULL;
ID3D10ShaderResourceView *Block::density_volume_srv_ = NULL;
ID3D10Buffer *Block::triangle_list_vb_ = NULL;
ID3D10InputLayout *Block::triangle_list_il_ = NULL;
ID3D10Buffer *Block::voxel_slice_vb_ = NULL;
ID3D10InputLayout *Block::voxel_slice_il_ = NULL;
ID3D10EffectVectorVariable *Block::offset_ev_ = NULL;
ID3D10EffectShaderResourceVariable *Block::density_volume_ev_ = NULL;
ID3D10Effect *Block::effect_ = NULL;
ID3D10Query *Block::query_ = NULL;

Block::BLOCK_CACHE Block::cache_;

Block::Block(const D3DXVECTOR3 &position)
    : position_(position),
      vertex_buffer_(NULL),
      primitive_count_(-1),
      active_(false) {
  id_.x = static_cast<int>(position.x);
  id_.y = static_cast<int>(position.y);
  id_.z = static_cast<int>(position.z);
}

Block::Block(const BLOCK_ID &id)
    : id_(id),
      vertex_buffer_(NULL),
      primitive_count_(-1),
      active_(false) {
  position_.x = static_cast<FLOAT>(id.x);
  position_.y = static_cast<FLOAT>(id.y);
  position_.z = static_cast<FLOAT>(id.z);
}

Block::~Block(void) {
  SAFE_RELEASE(vertex_buffer_);
}

HRESULT Block::Activate(ID3D10Device *device) {
  if (active_) return S_OK;

  assert(offset_ev_ != NULL);
  HRESULT hr;

  // Create vertex buffer
  {
    D3D10_BUFFER_DESC buffer_desc;
    //buffer_desc.ByteWidth = 2*sizeof(D3DXVECTOR3)*(Block::kVoxelDimMinusOne*Block::kVoxelDimMinusOne*Block::kVoxelDimMinusOne*15); // Worst case
    buffer_desc.ByteWidth = 2*sizeof(D3DXVECTOR3)*(Block::kVoxelDimMinusOne*Block::kVoxelDimMinusOne*Block::kVoxelDimMinusOne*3); // Should suffice...
    buffer_desc.Usage = D3D10_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER | D3D10_BIND_STREAM_OUTPUT;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    V_RETURN(device->CreateBuffer(&buffer_desc, NULL, &vertex_buffer_));
  }

  offset_ev_->SetFloatVector(position_);
  V_RETURN(RenderDensityVolume(device));
  V_RETURN(GenerateTriangles(device));

  if (IsEmpty()) Deactivate();

  active_ = true;

  return S_OK;
}

void Block::Deactivate(void) {
  if (!active_) return;
  SAFE_RELEASE(vertex_buffer_);
  active_ = false;
}

HRESULT Block::RenderDensityVolume(ID3D10Device *device) {
  assert(density_volume_rtv_ != NULL);
  assert(screen_aligned_quad_vb_ != NULL);
  assert(screen_aligned_quad_il_ != NULL);

  D3D10_VIEWPORT viewport;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.Width = kVoxelDimWithMargins;
  viewport.Height = kVoxelDimWithMargins;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  device->RSSetViewports(1, &viewport);
  device->OMSetRenderTargets(1, &density_volume_rtv_, NULL);

  UINT strides = sizeof(D3DXVECTOR4);
  UINT offsets = 0;
  device->IASetVertexBuffers(0, 1, &screen_aligned_quad_vb_, &strides, &offsets);
  device->IASetInputLayout(screen_aligned_quad_il_);
  device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  effect_->GetTechniqueByName("GenBlock")->GetPassByIndex(0)->Apply(0);
  device->DrawInstanced(4, kVoxelDimWithMargins, 0, 0);

  return S_OK;
}

HRESULT Block::GenerateTriangles(ID3D10Device *device) {
  assert(voxel_slice_vb_ != NULL);
  assert(voxel_slice_il_ != NULL);
  assert(triangle_list_vb_ != NULL);
  assert(triangle_list_il_ != NULL);
  assert(density_volume_ev_ != NULL);
  assert(density_volume_srv_ != NULL);

  //
  // List Triangles (pass 1)
  //
  UINT strides = sizeof(UINT)*2;
  UINT offsets = 0;
  device->SOSetTargets(1, &triangle_list_vb_, &offsets);
  device->IASetVertexBuffers(0, 1, &voxel_slice_vb_, &strides, &offsets);
  device->IASetInputLayout(voxel_slice_il_);
  device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
  device->OMSetRenderTargets(0, NULL, NULL);

  density_volume_ev_->SetResource(density_volume_srv_);
  effect_->GetTechniqueByName("GenBlock")->GetPassByIndex(1)->Apply(0);

  InitQuery(device);
  device->DrawInstanced(kVoxelDimMinusOne*kVoxelDimMinusOne, kVoxelDimMinusOne, 0, 0);
  primitive_count_ = static_cast<INT>(GetQueryResult());

  if (primitive_count_ == 0) {
    goto done;
  }

  //
  // Generate triangles (pass 2)
  //
  strides = sizeof(UINT);
  device->SOSetTargets(1, &vertex_buffer_, &offsets);
  device->IASetVertexBuffers(0, 1, &triangle_list_vb_, &strides, &offsets);
  device->IASetInputLayout(triangle_list_il_);
  device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
  device->OMSetRenderTargets(0, NULL, NULL);

  effect_->GetTechniqueByName("GenBlock")->GetPassByIndex(2)->Apply(0);

  device->DrawAuto();

  //
  // Reset
  //
done:
  ID3D10Buffer *no_buffer = NULL;
  device->SOSetTargets(1, &no_buffer, &offsets);

  // Get rid of DEVICE_OMSETRENDERTARGETS_HAZARD and DEVICE_VSSETSHADERRESOURCES_HAZARD by
  // explicitly setting the resource slot to 0. But makes some unnecessary calls (VSSetShader etc.)
  density_volume_ev_->SetResource(NULL);
  effect_->GetTechniqueByName("GenBlock")->GetPassByIndex(1)->Apply(0);

  return S_OK;
}

void Block::Draw(ID3D10Device *device, ID3D10EffectTechnique *technique) const {
  if (primitive_count_ == 0) return;
  UINT strides = sizeof(D3DXVECTOR3)*2;
  UINT offsets = 0;
  device->IASetVertexBuffers(0, 1, &vertex_buffer_, &strides, &offsets);
  device->IASetInputLayout(input_layout_);
  device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  technique->GetPassByIndex(0)->Apply(0);
  device->DrawAuto();
}

HRESULT Block::OnCreateDevice(ID3D10Device *device) {
  HRESULT hr;

  //
  // Create density volume texture (including views)
  //
  D3D10_TEXTURE3D_DESC tex3d_desc;
  tex3d_desc.Width = kVoxelDimWithMargins;
  tex3d_desc.Height = kVoxelDimWithMargins;
  tex3d_desc.Depth = kVoxelDimWithMargins;
  tex3d_desc.Format = DXGI_FORMAT_R16_FLOAT;
  tex3d_desc.Usage = D3D10_USAGE_DEFAULT;
  tex3d_desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
  tex3d_desc.CPUAccessFlags = 0;
  tex3d_desc.MiscFlags = 0;
  tex3d_desc.MipLevels = 1;
  V_RETURN(device->CreateTexture3D(&tex3d_desc, NULL, &density_volume_tex_));

  D3D10_RENDER_TARGET_VIEW_DESC rtv_desc;
  rtv_desc.Format = tex3d_desc.Format;
  rtv_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE3D;
  rtv_desc.Texture3D.WSize = tex3d_desc.Depth;
  rtv_desc.Texture3D.FirstWSlice = 0;
  rtv_desc.Texture3D.MipSlice = 0;
  V_RETURN(device->CreateRenderTargetView(density_volume_tex_, &rtv_desc, &density_volume_rtv_));

  D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc;
  srv_desc.Format = tex3d_desc.Format;
  srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE3D;
  srv_desc.Texture3D.MipLevels = tex3d_desc.MipLevels;
  srv_desc.Texture3D.MostDetailedMip = 0;
  V_RETURN(device->CreateShaderResourceView(density_volume_tex_, &srv_desc, &density_volume_srv_));

  //
  // Create screen aligned quad vertex buffer
  //
  {
    D3DXVECTOR4 quad_vertices[] = {
      D3DXVECTOR4(-1.0f,  1.0f, 0.0f, 0.0f),
      D3DXVECTOR4( 1.0f,  1.0f, 1.0f, 0.0f),
      D3DXVECTOR4(-1.0f, -1.0f, 0.0f, 1.0f),
      D3DXVECTOR4( 1.0f, -1.0f, 1.0f, 1.0f)
    };
    D3D10_BUFFER_DESC buffer_desc;
    buffer_desc.ByteWidth = 4*4*sizeof(float); // 4x (float2+float2)
    buffer_desc.Usage = D3D10_USAGE_IMMUTABLE;
    buffer_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA init_data;
    init_data.pSysMem = quad_vertices;
    init_data.SysMemPitch = 0;
    init_data.SysMemSlicePitch = 0;
    V_RETURN(device->CreateBuffer(&buffer_desc, &init_data, &screen_aligned_quad_vb_));
  }

  //
  // Create triangle list vertex buffer
  //
  {
    D3D10_BUFFER_DESC buffer_desc;
    buffer_desc.ByteWidth = sizeof(UINT)*(Block::kVoxelDimMinusOne*Block::kVoxelDimMinusOne*Block::kVoxelDimMinusOne*5);
    buffer_desc.Usage = D3D10_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER | D3D10_BIND_STREAM_OUTPUT;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    V_RETURN(device->CreateBuffer(&buffer_desc, NULL, &triangle_list_vb_));
  }

  //
  // Create voxel slice vertex buffer
  //
  {
    UINT voxels[kVoxelDimMinusOne*kVoxelDimMinusOne][2];
    for (UINT i = 0; i < kVoxelDimMinusOne; ++i) {
      for (UINT j = 0; j < kVoxelDimMinusOne; ++j) {
        voxels[i+j*kVoxelDimMinusOne][0] = i;
        voxels[i+j*kVoxelDimMinusOne][1] = j;
      }
    }
    D3D10_BUFFER_DESC buffer_desc;
    buffer_desc.ByteWidth = sizeof(voxels);
    buffer_desc.Usage = D3D10_USAGE_IMMUTABLE;
    buffer_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA init_data;
    init_data.pSysMem = voxels;
    init_data.SysMemPitch = 0;
    init_data.SysMemSlicePitch = 0;
    V_RETURN(device->CreateBuffer(&buffer_desc, &init_data, &voxel_slice_vb_));
  }

  return S_OK;
}

HRESULT Block::OnLoadEffect(ID3D10Device *device, ID3D10Effect *effect) {
  HRESULT hr;

  effect_ = effect;

  // Get variables
  offset_ev_ = effect->GetVariableByName("g_vBlockOffset")->AsVector();
  density_volume_ev_ = effect->GetVariableByName("g_tDensityVolume")->AsShaderResource();

  // Set constants
  {
    effect->GetVariableByName("BlockSize")->AsScalar()->SetFloat(kBlockSize);
    effect->GetVariableByName("VoxelDim")->AsScalar()->SetInt(kVoxelDim);
    effect->GetVariableByName("VoxelDimMinusOne")->AsScalar()->SetInt(kVoxelDimMinusOne);
    effect->GetVariableByName("Margin")->AsScalar()->SetInt(kMargin);
    effect->GetVariableByName("VoxelDimWithMargins")->AsScalar()->SetInt(kVoxelDimWithMargins);
    effect->GetVariableByName("VoxelDimWithMarginsMinusOne")->AsScalar()->SetInt(kVoxelDimWithMarginsMinusOne);
    D3DXVECTOR2 dummy(0, 0);
    dummy.x = kInvVoxelDim;
    effect->GetVariableByName("InvVoxelDim")->AsVector()->SetFloatVector(dummy);
    dummy.x = kInvVoxelDimMinusOne;
    effect->GetVariableByName("InvVoxelDimMinusOne")->AsVector()->SetFloatVector(dummy);
    dummy.x = kInvVoxelDimWithMargins;
    effect->GetVariableByName("InvVoxelDimWithMargins")->AsVector()->SetFloatVector(dummy);
    dummy.x = kInvVoxelDimWithMarginsMinusOne;
    effect->GetVariableByName("InvVoxelDimWithMarginsMinusOne")->AsVector()->SetFloatVector(dummy);
  }

  // Create screen aligned quad input layout
  {
    D3D10_INPUT_ELEMENT_DESC input_elements[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT num_elements = sizeof(input_elements)/sizeof(input_elements[0]);
    D3D10_PASS_DESC pass_desc;
    effect->GetTechniqueByName("GenBlock")->GetPassByIndex(0)->GetDesc(&pass_desc);
    V_RETURN(device->CreateInputLayout(input_elements, num_elements,
                                       pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize,
                                       &screen_aligned_quad_il_));
  }

  // Create voxel slice input layout
  {
    D3D10_INPUT_ELEMENT_DESC input_elements[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT num_elements = sizeof(input_elements)/sizeof(input_elements[0]);
    D3D10_PASS_DESC pass_desc;
    effect->GetTechniqueByName("GenBlock")->GetPassByIndex(1)->GetDesc(&pass_desc);
    V_RETURN(device->CreateInputLayout(input_elements, num_elements,
                                       pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize,
                                       &voxel_slice_il_));
  }

  // Create triangle list input layout
  {
    D3D10_INPUT_ELEMENT_DESC input_elements[] = {
      { "MARKER", 0, DXGI_FORMAT_R32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT num_elements = sizeof(input_elements)/sizeof(input_elements[0]);
    D3D10_PASS_DESC pass_desc;
    effect->GetTechniqueByName("GenBlock")->GetPassByIndex(2)->GetDesc(&pass_desc);
    V_RETURN(device->CreateInputLayout(input_elements, num_elements,
                                       pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize,
                                       &triangle_list_il_));
  }

  // Create render input layout
  {
    D3D10_INPUT_ELEMENT_DESC input_elements[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
      { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT num_elements = sizeof(input_elements)/sizeof(input_elements[0]);
    D3D10_PASS_DESC pass_desc;
    effect->GetTechniqueByName("RenderBlock")->GetPassByIndex(0)->GetDesc(&pass_desc);
    V_RETURN(device->CreateInputLayout(input_elements, num_elements,
                                       pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize,
                                       &input_layout_));
  }

  return S_OK;
}

void Block::OnDestroyDevice() {
  SAFE_RELEASE(input_layout_);
  SAFE_RELEASE(screen_aligned_quad_vb_);
  SAFE_RELEASE(screen_aligned_quad_il_);
  SAFE_RELEASE(density_volume_tex_);
  SAFE_RELEASE(density_volume_rtv_);
  SAFE_RELEASE(density_volume_srv_);
  SAFE_RELEASE(triangle_list_vb_);
  SAFE_RELEASE(triangle_list_il_);
  SAFE_RELEASE(voxel_slice_vb_);
  SAFE_RELEASE(voxel_slice_il_);

  BLOCK_CACHE::const_iterator it;
  for (it = cache_.begin(); it != cache_.end(); ++it) {
    delete it->second;
  }
  cache_.clear();
}

Block *Block::GetBlockByID(const BLOCK_ID &id) {
  BLOCK_CACHE::const_iterator it = cache_.find(id);
  if (it != cache_.end()) return it->second;
  Block *block = new Block(id);
  cache_[id] = block;
  return block;
}

void Block::InitQuery(ID3D10Device *device) {
  D3D10_QUERY_DESC query_desc = {
    D3D10_QUERY_SO_STATISTICS, 0
  };
  device->CreateQuery(&query_desc, &query_);
  query_->Begin();
}

UINT64 Block::GetQueryResult(void) {
  query_->End();

  D3D10_QUERY_DATA_SO_STATISTICS query_data;
  ZeroMemory(&query_data, sizeof(D3D10_QUERY_DATA_SO_STATISTICS));
  while (S_OK != query_->GetData(&query_data, sizeof(D3D10_QUERY_DATA_SO_STATISTICS), 0));
  query_->Release();
  query_ = NULL;

  return query_data.NumPrimitivesWritten;
}