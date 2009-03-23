#pragma once
#include "DXUT.h"

class LoadingScreen {
 public:
  LoadingScreen(void);
  ~LoadingScreen(void);

  void set_loaded(float loaded) { loaded_ = loaded; }
  void Draw(ID3D10Device *device) const;
  HRESULT OnCreateDevice(ID3D10Device *device);
  HRESULT OnLoadEffect(ID3D10Device *device, ID3D10Effect *effect);
  void OnDestroyDevice(void);

 private:
  ID3D10Effect *effect_;
  ID3D10Buffer *screen_aligned_quad_vb_;
  ID3D10InputLayout *screen_aligned_quad_il_;
  ID3D10EffectScalarVariable *loaded_ev_;
  float loaded_;
};
