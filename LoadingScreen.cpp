#include "LoadingScreen.h"

LoadingScreen::LoadingScreen(void)
    : effect_(NULL),
      screen_aligned_quad_vb_(NULL),
      screen_aligned_quad_il_(NULL),
      loaded_(0.0f) {
}

LoadingScreen::~LoadingScreen(void) {
  OnDestroyDevice();  
}

HRESULT LoadingScreen::OnCreateDevice(ID3D10Device *device) {
  HRESULT hr;

  D3DXVECTOR4 quad_vertices[] = {
    D3DXVECTOR4(-1.0f,  1.0f, 0.0f, 1.0f),
    D3DXVECTOR4( 1.0f,  1.0f, 0.0f, 1.0f),
    D3DXVECTOR4(-1.0f, -1.0f, 0.0f, 1.0f),
    D3DXVECTOR4( 1.0f, -1.0f, 0.0f, 1.0f)
  };
  D3D10_BUFFER_DESC buffer_desc;
  buffer_desc.ByteWidth = 4*sizeof(D3DXVECTOR4);
  buffer_desc.Usage = D3D10_USAGE_IMMUTABLE;
  buffer_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
  buffer_desc.CPUAccessFlags = 0;
  buffer_desc.MiscFlags = 0;
  D3D10_SUBRESOURCE_DATA init_data;
  init_data.pSysMem = quad_vertices;
  init_data.SysMemPitch = 0;
  init_data.SysMemSlicePitch = 0;
  V_RETURN(device->CreateBuffer(&buffer_desc, &init_data, &screen_aligned_quad_vb_));

  return S_OK;
}

HRESULT LoadingScreen::OnLoadEffect(ID3D10Device *device, ID3D10Effect *effect) {
  HRESULT hr;

  effect_ = effect;
  loaded_ev_ = effect->GetVariableByName("g_fLoaded")->AsScalar();

  D3D10_INPUT_ELEMENT_DESC input_elements[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
  };
  UINT num_elements = sizeof(input_elements)/sizeof(input_elements[0]);
  D3D10_PASS_DESC pass_desc;
  effect->GetTechniqueByName("LoadingScreen")->GetPassByIndex(0)->GetDesc(&pass_desc);
  V_RETURN(device->CreateInputLayout(input_elements, num_elements,
                                     pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize,
                                     &screen_aligned_quad_il_));
  
  return S_OK;
}

void LoadingScreen::Draw(ID3D10Device *device) const {
  assert(screen_aligned_quad_vb_ != NULL);
  assert(screen_aligned_quad_il_ != NULL);
  assert(loaded_ev_ != NULL);

  UINT strides = sizeof(D3DXVECTOR4);
  UINT offsets = 0;
  device->IASetVertexBuffers(0, 1, &screen_aligned_quad_vb_, &strides, &offsets);
  device->IASetInputLayout(screen_aligned_quad_il_);
  device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  loaded_ev_->SetFloat(loaded_);

  effect_->GetTechniqueByName("LoadingScreen")->GetPassByIndex(0)->Apply(0);
  device->Draw(4, 0);
}

void LoadingScreen::OnDestroyDevice(void) {
  SAFE_RELEASE(screen_aligned_quad_vb_);
  SAFE_RELEASE(screen_aligned_quad_il_);
}
