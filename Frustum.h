#pragma once
#include "DXUT.h"
#include "DXUTcamera.h"

class AxisAlignedBox;

class Frustum {
 public:
  Frustum(const CBaseCamera *camera_);
  ~Frustum(void);

  void set_camera(const CBaseCamera *camera) { camera_ = camera; }

  void Update(void);
  bool PointInside(const D3DXVECTOR3 &point) const;
  bool AABInside(const AxisAlignedBox &box) const;

 private:
  const CBaseCamera *camera_;
  D3DXPLANE planes_[6];
};
