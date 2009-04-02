#include "AxisAlignedBox.h"

AxisAlignedBox::AxisAlignedBox(const D3DXVECTOR3 &p1, const D3DXVECTOR3 &p2) {
  D3DXVec3Minimize(&min_, &p1, &p2);
  D3DXVec3Maximize(&max_, &p1, &p2);
}

AxisAlignedBox::~AxisAlignedBox(void) {
}

void AxisAlignedBox::GetCorners(D3DXVECTOR3 *corners_out) const {
  corners_out[0] = min_;
  corners_out[1] = D3DXVECTOR3(min_.x, min_.y, max_.z);
  corners_out[2] = D3DXVECTOR3(min_.x, max_.y, min_.z);
  corners_out[3] = D3DXVECTOR3(min_.x, max_.y, max_.z);
  corners_out[4] = D3DXVECTOR3(max_.x, min_.y, min_.z);
  corners_out[5] = D3DXVECTOR3(max_.x, min_.y, max_.z);
  corners_out[6] = D3DXVECTOR3(max_.x, max_.y, min_.z);
  corners_out[7] = max_;
}