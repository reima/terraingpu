#include "Frustum.h"
#include "AxisAlignedBox.h"

Frustum::Frustum(const CBaseCamera *camera) : camera_(camera) {  
}

Frustum::~Frustum(void) {
}

void Frustum::Update(void) {
  assert(camera_ != NULL);

  // For details see http://www2.ravensoft.com/users/ggribb/plane%20extraction.pdf
  D3DXMATRIX view_proj = *camera_->GetViewMatrix() *
                         *camera_->GetProjMatrix();
  // Left
  planes_[0] = D3DXPLANE(view_proj._14 + view_proj._11,
                         view_proj._24 + view_proj._21,
                         view_proj._34 + view_proj._31,
                         view_proj._44 + view_proj._41);
  // Right
  planes_[1] = D3DXPLANE(view_proj._14 - view_proj._11,
                         view_proj._24 - view_proj._21,
                         view_proj._34 - view_proj._31,
                         view_proj._44 - view_proj._41);
  // Bottom
  planes_[2] = D3DXPLANE(view_proj._14 + view_proj._12,
                         view_proj._24 + view_proj._22,
                         view_proj._34 + view_proj._32,
                         view_proj._44 + view_proj._42);
  // Top
  planes_[3] = D3DXPLANE(view_proj._14 - view_proj._12,
                         view_proj._24 - view_proj._22,
                         view_proj._34 - view_proj._32,
                         view_proj._44 - view_proj._42);
  // Near
  planes_[4] = D3DXPLANE(view_proj._13,
                         view_proj._23,
                         view_proj._33,
                         view_proj._43);
  // Far
  planes_[5] = D3DXPLANE(view_proj._14 - view_proj._13,
                         view_proj._24 - view_proj._23,
                         view_proj._34 - view_proj._33,
                         view_proj._44 - view_proj._43);
}

bool Frustum::PointInside(const D3DXVECTOR3 &point) const {
  for (size_t i = 0; i < 6; ++i) {
    if (D3DXPlaneDotCoord(&planes_[i], &point) < 0)
      return false;
  }
  return true;
}

bool Frustum::AABInside(const AxisAlignedBox &box) const {
  // TODO: Implement according to http://www.lighthouse3d.com/opengl/viewfrustum/index.php?gatest2
  D3DXVECTOR3 corners[8];
  box.GetCorners(corners);

  UINT in, out;
  for (size_t i = 0; i < 6; ++i) {
    in = out = 0;
    for (size_t k = 0; k < 8 && (in == 0 || out == 0); ++k) {
      if (D3DXPlaneDotCoord(&planes_[i], &corners[k]) < 0) out++;
      else in++;
    }
    if (in == 0) return false;
  }
  return true;
}