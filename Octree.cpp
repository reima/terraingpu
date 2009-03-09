#include "Octree.h"
#include "Block.h"

Octree::Octree(INT base_x, INT base_y, INT base_z, UINT depth)
    : x_(base_x),
      y_(base_y),
      z_(base_z),
      depth_(depth),
      size_(1<<depth),
      block_(NULL),
      is_empty_(false) {
  Init();
}

Octree::~Octree(void) {
  for (UINT i = 0; i < 8; ++i) SAFE_DELETE(children_[i]);
}

void Octree::Init(void) {
  UINT child_size = size_ / 2;
  if (depth_ > 0) {
    children_[X_POS_Y_POS_Z_POS] = new Octree(x_ + child_size, y_ + child_size, z_ + child_size, depth_ - 1);
    children_[X_POS_Y_POS_Z_NEG] = new Octree(x_ + child_size, y_ + child_size, z_,              depth_ - 1);
    children_[X_POS_Y_NEG_Z_POS] = new Octree(x_ + child_size, y_,              z_ + child_size, depth_ - 1);
    children_[X_POS_Y_NEG_Z_NEG] = new Octree(x_ + child_size, y_,              z_,              depth_ - 1);
    children_[X_NEG_Y_POS_Z_POS] = new Octree(x_,              y_ + child_size, z_ + child_size, depth_ - 1);
    children_[X_NEG_Y_POS_Z_NEG] = new Octree(x_,              y_ + child_size, z_,              depth_ - 1);
    children_[X_NEG_Y_NEG_Z_POS] = new Octree(x_,              y_,              z_ + child_size, depth_ - 1);
    children_[X_NEG_Y_NEG_Z_NEG] = new Octree(x_,              y_,              z_,              depth_ - 1);
  } else {
    for (UINT i = 0; i < 8; ++i) children_[i] = NULL;
    block_ = Block::GetBlockByID(BLOCK_ID(x_, y_, z_));
  }
}

void Octree::Relocate(INT base_x, INT base_y, INT base_z) {
  if (x_ == base_x && y_ == base_y && z_ == base_z) return;
  x_ = base_x;
  y_ = base_y;
  z_ = base_z;
  UINT child_size = size_ / 2;
  if (depth_ > 0) {
    children_[X_POS_Y_POS_Z_POS]->Relocate(x_ + child_size, y_ + child_size, z_ + child_size);
    children_[X_POS_Y_POS_Z_NEG]->Relocate(x_ + child_size, y_ + child_size, z_);
    children_[X_POS_Y_NEG_Z_POS]->Relocate(x_ + child_size, y_,              z_ + child_size);
    children_[X_POS_Y_NEG_Z_NEG]->Relocate(x_ + child_size, y_,              z_);
    children_[X_NEG_Y_POS_Z_POS]->Relocate(x_,              y_ + child_size, z_ + child_size);
    children_[X_NEG_Y_POS_Z_NEG]->Relocate(x_,              y_ + child_size, z_);
    children_[X_NEG_Y_NEG_Z_POS]->Relocate(x_,              y_,              z_ + child_size);
    children_[X_NEG_Y_NEG_Z_NEG]->Relocate(x_,              y_,              z_);
  } else {
    block_ = Block::GetBlockByID(BLOCK_ID(x_, y_, z_));
  }
}

HRESULT Octree::ActivateBlocks(ID3D10Device *device) {
  HRESULT hr;

  if (block_) {
    block_->Activate();
    is_empty_ = block_->IsEmpty();
  } else {
    is_empty_ = true;
    for (UINT i = 0; i < 8; ++i) {
      if (children_[i]) {
        V_RETURN(children_[i]->ActivateBlocks(device));
        is_empty_ &= children_[i]->is_empty_;
      }
    }
  }

  return S_OK;
}

void Octree::Draw(ID3D10Device *device, ID3D10EffectTechnique *technique) const {
  if (is_empty_) return;
  if (block_) {
    block_->Draw(device, technique);
  }
  for (UINT i = 0; i < 8; ++i) {
    if (children_[i]) children_[i]->Draw(device, technique);
  }
}