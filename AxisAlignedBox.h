#pragma once
#include "DXUT.h"

class AxisAlignedBox {
 public:
  AxisAlignedBox(const D3DXVECTOR3 &p1, const D3DXVECTOR3 &p2);
  ~AxisAlignedBox(void);

  void GetCorners(D3DXVECTOR3 *corners_out) const;

 private:
  D3DXVECTOR3 min_, max_;
};
