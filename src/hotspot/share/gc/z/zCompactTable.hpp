#ifndef SHARE_GC_Z_ZFRAGMENTTABLE_HPP
#define SHARE_GC_Z_ZFRAGMENTTABLE_HPP

#include "gc/z/zGranuleMap.hpp"

class ZFragment;

class ZFragmentTable {
private:
  ZGranuleMap<ZFragment*> _map;

public:
  ZFragmentTable();

  ZFragment* get(uintptr_t addr) const;

  void insert(ZFragment* fragment);
  void remove(ZFragment* fragment);
};

#endif // SHARE_GC_Z_ZFRAGMENTTABLE_HPP
