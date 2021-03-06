#include "Octree.h"
#include "Block.h"
#include "Frustum.h"

Octree::Octree(INT base_x, INT base_y, INT base_z, UINT depth)
    : x_(base_x),
      y_(base_y),
      z_(base_z),
      depth_(depth),
      size_(1<<depth),
      block_(NULL),
      is_empty_(false),
      should_cull_(false),
      bounding_box_(D3DXVECTOR3(x_, y_, z_),
                    D3DXVECTOR3(x_+size_, y_+size_, z_+size_)) {
  Init();
}

Octree::~Octree(void) {
  if (block_) {
    block_->Deactivate();
    block_->set_used(false);
  }
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
  bounding_box_ = AxisAlignedBox(D3DXVECTOR3(x_, y_, z_),
                                 D3DXVECTOR3(x_+size_, y_+size_, z_+size_));
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
    block_->Deactivate();
    block_->set_used(false);
    block_ = Block::GetBlockByID(BLOCK_ID(x_, y_, z_));
  }
}

HRESULT Octree::ActivateBlocks(ID3D10Device *device) {
  HRESULT hr;

  if (block_) {
    block_->Activate();
    block_->set_used(true);
    is_empty_ = block_->empty();
  } else {
    is_empty_ = true;
    for (UINT i = 0; i < 8; ++i) {
      if (children_[i]) {
        V_RETURN(children_[i]->ActivateBlocks(device));
        is_empty_ = is_empty_ && children_[i]->is_empty_;
      }
    }
  }

  return S_OK;
}

void Octree::Cull(const Frustum &frustum) {
  should_cull_ = !frustum.AABInside(bounding_box_);
  if (!should_cull_) {
    for (UINT i = 0; i < 8; ++i) {
      if (children_[i]) children_[i]->Cull(frustum);
    }
  }
}

void Octree::Draw(ID3D10Device *device, ID3D10EffectTechnique *technique) const {
  if (is_empty_) return;
  if (should_cull_) return;
  if (block_) block_->Draw(device, technique);
  for (UINT i = 0; i < 8; ++i) {
    if (children_[i]) children_[i]->Draw(device, technique);
  }
}