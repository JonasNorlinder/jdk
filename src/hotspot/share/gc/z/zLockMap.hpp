#ifndef SHARE_GC_Z_ZLOCKMAP_HPP
#define SHARE_GC_Z_ZLOCKMAP_HPP

#include "gc/z/zLockMap.hpp"
#include "gc/z/zLock.inline.hpp"
#include "gc/z/zGranuleLockMap.inline.hpp"

class ZLockMap {
private:
  ZGranuleLockMap<ZLock> _map;

public:
  ZLockMap();

  inline void lock(uintptr_t from_offset) const {
    _map.get(from_offset)->lock();
  }

  inline void unlock(uintptr_t from_offset) const {
    _map.get(from_offset)->unlock();
  }
};

#endif // SHARE_GC_Z_ZLOCKMAP_HPP
