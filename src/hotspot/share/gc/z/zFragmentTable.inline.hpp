#ifndef SHARE_GC_Z_ZFRAGMENTTABLE_INLINE_HPP
#define SHARE_GC_Z_ZFRAGMENTTABLE_INLINE_HPP

#include "gc/z/zFragmentTable.hpp"
#include "gc/z/zAddress.inline.hpp"
#include "gc/z/zGranuleMap.inline.hpp"
#include "gc/z/zFragment.inline.hpp"

inline ZFragment* ZFragmentTable::get(uintptr_t addr) const {
  assert(!ZAddress::is_null(addr), "Invalid address");
  return _map.get(ZAddress::offset(addr));
}

#endif // SHARE_GC_Z_ZFRAGMENTTABLE_INLINE_HPP
