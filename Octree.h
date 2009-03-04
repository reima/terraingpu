#pragma once
#include "DXUT.h"

class Block;

class Octree {
 public:
  Octree(INT base_x, INT base_y, INT base_z, UINT depth);
  ~Octree(void);

  void Relocate(INT base_x, INT base_y, INT base_z);
  HRESULT ActivateBlocks(ID3D10Device *device);
  void Draw(ID3D10Device *device, ID3D10EffectTechnique *technique) const;

 private:
  typedef enum {
    X_POS_Y_POS_Z_POS = 0,
    X_POS_Y_POS_Z_NEG,
    X_POS_Y_NEG_Z_POS,
    X_POS_Y_NEG_Z_NEG,
    X_NEG_Y_POS_Z_POS,
    X_NEG_Y_POS_Z_NEG,
    X_NEG_Y_NEG_Z_POS,
    X_NEG_Y_NEG_Z_NEG,
  } ChildDirection;

  void Init(void);

  Block *block_;
  bool is_empty_;
  Octree *children_[8];
  INT x_, y_, z_;
  UINT depth_;
  UINT size_;
};
