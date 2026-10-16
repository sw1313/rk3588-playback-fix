// SPDX-License-Identifier: MIT
#pragma once
#include <C2Component.h>
#include <C2Buffer.h>
std::shared_ptr<const C2Component> CompatPoolOwner(std::shared_ptr<const C2Component>);
bool CompatSurfacePool(C2BlockPool::local_id_t);
c2_status_t MakeCompatRouter(const std::shared_ptr<C2Component>&,const std::shared_ptr<C2Component>&,std::shared_ptr<C2Component>*);
