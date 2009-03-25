#include "Block.h"
#include "Config.h"

#define METHOD 3

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
ID3D10Buffer *Block::cells_vb_ = NULL;
ID3D10InputLayout *Block::cells_il_ = NULL;
ID3D10Buffer *Block::edges_vb_ = NULL;
ID3D10InputLayout *Block::edges_il_ = NULL;
ID3D10Buffer *Block::voxel_slice_vb_ = NULL;
ID3D10InputLayout *Block::voxel_slice_il_ = NULL;
ID3D10EffectVectorVariable *Block::offset_ev_ = NULL;
ID3D10EffectScalarVariable *Block::activation_time_ev_ = NULL;
ID3D10EffectShaderResourceVariable *Block::density_volume_ev_ = NULL;
ID3D10Texture3D *Block::indices_volume_tex_ = NULL;
ID3D10RenderTargetView *Block::indices_volume_rtv_ = NULL;
ID3D10ShaderResourceView *Block::indices_volume_srv_ = NULL;
ID3D10EffectShaderResourceVariable *Block::indices_volume_ev_ = NULL;
ID3D10Effect *Block::effect_ = NULL;
ID3D10Query *Block::query_ = NULL;
D3DXVECTOR3 Block::camera_pos_;

Block::BLOCK_CACHE Block::cache_;
Block::BLOCK_QUEUE Block::activation_queue_;

Block::Block(const D3DXVECTOR3 &position)
    : position_(position),
      vertex_buffer_(NULL),
      index_buffer_(NULL),
      primitive_count_(-1),
      active_(false),
      waiting_for_activation_(false),
      activation_time_(0.0),
      distance_to_camera_(0.0) {
  id_.x = static_cast<int>(position.x);
  id_.y = static_cast<int>(position.y);
  id_.z = static_cast<int>(position.z);
}

Block::Block(const BLOCK_ID &id)
    : id_(id),
      vertex_buffer_(NULL),
      index_buffer_(NULL),
      primitive_count_(-1),
      active_(false),
      waiting_for_activation_(false),
      activation_time_(0.0),
      distance_to_camera_(0.0) {
  position_.x = static_cast<FLOAT>(id.x);
  position_.y = static_cast<FLOAT>(id.y);
  position_.z = static_cast<FLOAT>(id.z);
}

Block::~Block(void) {
  SAFE_RELEASE(vertex_buffer_);
  SAFE_RELEASE(index_buffer_);
}

void Block::Activate(void) {
  if (waiting_for_activation_) return;
  if (active_) return;
  waiting_for_activation_ = true;
  D3DXVECTOR3 center = position_ + D3DXVECTOR3(0.5f, 0.5f, 0.5f) * Block::kBlockSize;
  D3DXVECTOR3 distance_vector = center - camera_pos_;
  distance_to_camera_ = D3DXVec3Length(&distance_vector);
  activation_queue_.push(this);
}

void Block::Deactivate(void) {
  waiting_for_activation_ = false;
  if (!active_) return;
  SAFE_RELEASE(vertex_buffer_);
  SAFE_RELEASE(index_buffer_);
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

  HRESULT hr;

#if METHOD == 2
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

  V_RETURN(density_volume_ev_->SetResource(density_volume_srv_));
  V_RETURN(effect_->GetTechniqueByName("GenBlock")->GetPassByIndex(1)->Apply(0));

  InitQuery(device);
  device->DrawInstanced(kVoxelDimMinusOne*kVoxelDimMinusOne, kVoxelDimMinusOne, 0, 0);
  primitive_count_ = static_cast<INT>(GetQueryResult());

  if (primitive_count_ == 0) {
    goto done;
  }

  //
  // Create vertex buffer
  //
  {
    D3D10_BUFFER_DESC buffer_desc;
    buffer_desc.ByteWidth = 2*sizeof(D3DXVECTOR3) * 3*primitive_count_;
    buffer_desc.Usage = D3D10_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER | D3D10_BIND_STREAM_OUTPUT;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    V_RETURN(device->CreateBuffer(&buffer_desc, NULL, &vertex_buffer_));
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

  V_RETURN(effect_->GetTechniqueByName("GenBlock")->GetPassByIndex(2)->Apply(0));

  device->DrawAuto();
#elif METHOD == 3
  //
  // List non-empty cells (pass 1)
  //
  UINT strides = sizeof(UINT)*2;
  UINT offsets = 0;
  device->SOSetTargets(1, &cells_vb_, &offsets);
  device->IASetVertexBuffers(0, 1, &voxel_slice_vb_, &strides, &offsets);
  device->IASetInputLayout(voxel_slice_il_);
  device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
  device->OMSetRenderTargets(0, NULL, NULL);

  V_RETURN(density_volume_ev_->SetResource(density_volume_srv_));
  V_RETURN(effect_->GetTechniqueByName("GenBlock3")->GetPassByIndex(1)->Apply(0));

  InitQuery(device);
  device->DrawInstanced(kVoxelDim*kVoxelDim, kVoxelDim, 0, 0);
  if (GetQueryResult() == 0) goto done;

  //
  // List edges (pass 2)
  //
  strides = sizeof(UINT);
  device->SOSetTargets(1, &edges_vb_, &offsets);
  device->IASetVertexBuffers(0, 1, &cells_vb_, &strides, &offsets);
  device->IASetInputLayout(cells_il_);
  device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
  device->OMSetRenderTargets(0, NULL, NULL);

  V_RETURN(effect_->GetTechniqueByName("GenBlock3")->GetPassByIndex(2)->Apply(0));

  InitQuery(device);
  device->DrawAuto();
  primitive_count_ = (UINT)GetQueryResult();
  if (primitive_count_ == 0) goto done; // TODO: Find out why this can happen at all

  //
  // Create vertex buffer
  //
  {
    D3D10_BUFFER_DESC buffer_desc;
    buffer_desc.ByteWidth = 2*sizeof(D3DXVECTOR3) * 3*primitive_count_;
    buffer_desc.Usage = D3D10_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER | D3D10_BIND_STREAM_OUTPUT;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    V_RETURN(device->CreateBuffer(&buffer_desc, NULL, &vertex_buffer_));
  }

  //
  // Create index buffer
  //
  {
    D3D10_BUFFER_DESC buffer_desc;
    buffer_desc.ByteWidth = sizeof(UINT) * 3*primitive_count_;
    buffer_desc.Usage = D3D10_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D10_BIND_INDEX_BUFFER | D3D10_BIND_STREAM_OUTPUT;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    V_RETURN(device->CreateBuffer(&buffer_desc, NULL, &index_buffer_));
  }

  //
  // Generate vertices (pass 3)
  //
  strides = sizeof(UINT);
  device->SOSetTargets(1, &vertex_buffer_, &offsets);
  device->IASetVertexBuffers(0, 1, &edges_vb_, &strides, &offsets);
  device->IASetInputLayout(edges_il_);
  device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
  device->OMSetRenderTargets(0, NULL, NULL);

  V_RETURN(effect_->GetTechniqueByName("GenBlock3")->GetPassByIndex(3)->Apply(0));
  device->DrawAuto();

  //
  // Splat indices (pass 0)
  //
  D3D10_VIEWPORT viewport;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.Width = 3*kVoxelDim;
  viewport.Height = kVoxelDim;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  device->RSSetViewports(1, &viewport);
  device->OMSetRenderTargets(1, &indices_volume_rtv_, NULL);
  {
    ID3D10Buffer *no_buffer = NULL;
    device->SOSetTargets(1, &no_buffer, &offsets);
  }

  strides = sizeof(UINT);
  device->IASetVertexBuffers(0, 1, &edges_vb_, &strides, &offsets);
  device->IASetInputLayout(edges_il_);
  device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);

  V_RETURN(effect_->GetTechniqueByName("GenIndices")->GetPassByIndex(0)->Apply(0));
  device->DrawAuto();

  //
  // Generate index buffer (pass 1)
  //
  strides = sizeof(UINT);
  device->SOSetTargets(1, &index_buffer_, &offsets);
  device->IASetVertexBuffers(0, 1, &cells_vb_, &strides, &offsets);
  device->IASetInputLayout(cells_il_);
  device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
  device->OMSetRenderTargets(0, NULL, NULL);

  V_RETURN(indices_volume_ev_->SetResource(indices_volume_srv_));
  V_RETURN(effect_->GetTechniqueByName("GenIndices")->GetPassByIndex(1)->Apply(0));

  device->DrawAuto();
#endif

  //
  // Reset
  //
done:
  ID3D10Buffer *no_buffer = NULL;
  device->SOSetTargets(1, &no_buffer, &offsets);

  // Get rid of DEVICE_OMSETRENDERTARGETS_HAZARD and DEVICE_VSSETSHADERRESOURCES_HAZARD by
  // explicitly setting the resource slot to 0. But makes some unnecessary calls (VSSetShader etc.)
  V_RETURN(density_volume_ev_->SetResource(NULL));
  V_RETURN(effect_->GetTechniqueByName("GenBlock")->GetPassByIndex(1)->Apply(0));

  return S_OK;
}

void Block::Draw(ID3D10Device *device, ID3D10EffectTechnique *technique) {
  assert(vertex_buffer_ != NULL);
  assert(input_layout_ != NULL);

  if (primitive_count_ <= 0) return;
  UINT strides = sizeof(D3DXVECTOR3)*2;
  UINT offsets = 0;
  device->IASetVertexBuffers(0, 1, &vertex_buffer_, &strides, &offsets);
  device->IASetInputLayout(input_layout_);
  device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  offset_ev_->SetFloatVector(position_);
  activation_time_ev_->SetFloat(activation_time_);
  technique->GetPassByIndex(0)->Apply(0);
#if METHOD == 2
  device->DrawAuto();
#elif METHOD == 3
  device->IASetIndexBuffer(index_buffer_, DXGI_FORMAT_R32_UINT, 0);
  device->DrawIndexed(3*primitive_count_, 0, 0);
#endif
}

HRESULT Block::OnCreateDevice(ID3D10Device *device) {
  HRESULT hr;

  //
  // Create density volume texture (including views)
  //
  {
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
  }

  //
  // Create indices volume texture (including views)
  //
  {
    D3D10_TEXTURE3D_DESC tex3d_desc;
    tex3d_desc.Width = kVoxelDim*3; // 3 edges per cell
    tex3d_desc.Height = kVoxelDim;
    tex3d_desc.Depth = kVoxelDim;
    tex3d_desc.Format = DXGI_FORMAT_R32_UINT;
    tex3d_desc.Usage = D3D10_USAGE_DEFAULT;
    tex3d_desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    tex3d_desc.CPUAccessFlags = 0;
    tex3d_desc.MiscFlags = 0;
    tex3d_desc.MipLevels = 1;
    V_RETURN(device->CreateTexture3D(&tex3d_desc, NULL, &indices_volume_tex_));

    D3D10_RENDER_TARGET_VIEW_DESC rtv_desc;
    rtv_desc.Format = tex3d_desc.Format;
    rtv_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE3D;
    rtv_desc.Texture3D.WSize = tex3d_desc.Depth;
    rtv_desc.Texture3D.FirstWSlice = 0;
    rtv_desc.Texture3D.MipSlice = 0;
    V_RETURN(device->CreateRenderTargetView(indices_volume_tex_, &rtv_desc, &indices_volume_rtv_));

    D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc;
    srv_desc.Format = tex3d_desc.Format;
    srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE3D;
    srv_desc.Texture3D.MipLevels = tex3d_desc.MipLevels;
    srv_desc.Texture3D.MostDetailedMip = 0;
    V_RETURN(device->CreateShaderResourceView(indices_volume_tex_, &srv_desc, &indices_volume_srv_));
  }

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
  // Create cell list vertex buffer
  //
  {
    D3D10_BUFFER_DESC buffer_desc;
    buffer_desc.ByteWidth = sizeof(UINT)*(Block::kVoxelDim*Block::kVoxelDim*Block::kVoxelDim); // Extra row in x, y, z!
    buffer_desc.Usage = D3D10_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER | D3D10_BIND_STREAM_OUTPUT;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    V_RETURN(device->CreateBuffer(&buffer_desc, NULL, &cells_vb_));
  }

  //
  // Create edge list vertex buffer
  //
  {
    D3D10_BUFFER_DESC buffer_desc;
    buffer_desc.ByteWidth = sizeof(UINT)*3*(Block::kVoxelDim*Block::kVoxelDim*Block::kVoxelDim);
    buffer_desc.Usage = D3D10_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER | D3D10_BIND_STREAM_OUTPUT;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    V_RETURN(device->CreateBuffer(&buffer_desc, NULL, &edges_vb_));
  }

  //
  // Create voxel slice vertex buffer
  //
  {
    UINT voxels[kVoxelDim*kVoxelDim][2];
    // 32x32 for method 2
    for (UINT i = 0; i < kVoxelDimMinusOne; ++i) {
      for (UINT j = 0; j < kVoxelDimMinusOne; ++j) {
        voxels[i+j*kVoxelDimMinusOne][0] = i;
        voxels[i+j*kVoxelDimMinusOne][1] = j;
      }
    }
    // missing voxels for 33x33 for method 3
    for (UINT i = 0; i < kVoxelDim - 1; ++i) {
      voxels[kVoxelDimMinusOne*kVoxelDimMinusOne+i][0] = i;
      voxels[kVoxelDimMinusOne*kVoxelDimMinusOne+i][1] = kVoxelDim - 1;
      voxels[kVoxelDimMinusOne*kVoxelDimMinusOne+i+kVoxelDimMinusOne][0] = kVoxelDim - 1;
      voxels[kVoxelDimMinusOne*kVoxelDimMinusOne+i+kVoxelDimMinusOne][1] = i;
    }
    voxels[kVoxelDim*kVoxelDim-1][0] = kVoxelDim - 1;
    voxels[kVoxelDim*kVoxelDim-1][1] = kVoxelDim - 1;

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
  activation_time_ev_ = effect->GetVariableByName("g_vBlockActivationTime")->AsScalar();
  density_volume_ev_ = effect->GetVariableByName("g_tDensityVolume")->AsShaderResource();
  indices_volume_ev_ = effect->GetVariableByName("g_tIndicesVolume")->AsShaderResource();

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

  // Create cell list input layout
  {
    D3D10_INPUT_ELEMENT_DESC input_elements[] = {
      { "CELL", 0, DXGI_FORMAT_R32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT num_elements = sizeof(input_elements)/sizeof(input_elements[0]);
    D3D10_PASS_DESC pass_desc;
    effect->GetTechniqueByName("GenBlock3")->GetPassByIndex(2)->GetDesc(&pass_desc);
    V_RETURN(device->CreateInputLayout(input_elements, num_elements,
                                       pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize,
                                       &cells_il_));
  }

  // Create edge list input layout
  {
    D3D10_INPUT_ELEMENT_DESC input_elements[] = {
      { "EDGE", 0, DXGI_FORMAT_R32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT num_elements = sizeof(input_elements)/sizeof(input_elements[0]);
    D3D10_PASS_DESC pass_desc;
    effect->GetTechniqueByName("GenBlock3")->GetPassByIndex(3)->GetDesc(&pass_desc);
    V_RETURN(device->CreateInputLayout(input_elements, num_elements,
                                       pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize,
                                       &edges_il_));
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
  SAFE_RELEASE(indices_volume_tex_);
  SAFE_RELEASE(indices_volume_rtv_);
  SAFE_RELEASE(indices_volume_srv_);
  SAFE_RELEASE(triangle_list_vb_);
  SAFE_RELEASE(triangle_list_il_);
  SAFE_RELEASE(cells_vb_);
  SAFE_RELEASE(cells_il_);
  SAFE_RELEASE(edges_vb_);
  SAFE_RELEASE(edges_il_);
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

HRESULT Block::ActivateReal(ID3D10Device *device) {
  if (active_) return S_OK;

  assert(offset_ev_ != NULL);
  HRESULT hr;

  offset_ev_->SetFloatVector(position_);
  V_RETURN(RenderDensityVolume(device));
  V_RETURN(GenerateTriangles(device));

  if (IsEmpty()) Deactivate();

  active_ = true;
  waiting_for_activation_ = false;

  activation_time_ = (float)DXUTGetTime();

  return S_OK;
}

void Block::OnFrameMove(float elapsed_time, const D3DXVECTOR3 &camera_pos) {
  camera_pos_ = camera_pos;
  int count = 0;
  // TODO: some sort of adpative max_count
  const int max_count = Config::Get<int>("MaxBlocksPerFrame");
  while (!activation_queue_.empty() && count < max_count) {
    Block *block = activation_queue_.top();
    if (block->waiting_for_activation_) {
      block->ActivateReal(DXUTGetD3D10Device());
      count++;
    }
    activation_queue_.pop();
  }
}