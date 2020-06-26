#include "precompiled.hpp"
#include "gc/z/zCompactTable.inline.hpp"
#include "gc/z/zGlobals.hpp"
#include "gc/z/zGranuleMap.inline.hpp"
#include "utilities/debug.hpp"
#include "gc/z/zCompact.inline.hpp"

ZFragmentTable::ZFragmentTable() :
  _map(ZAddressOffsetMax) {}

void ZFragmentTable::insert(ZFragment* fragment) {
  const uintptr_t offset = fragment->old_start();
  const size_t size = fragment->old_size();
  assert(_map.get(offset) == NULL, "Invalid entry");
  _map.put(offset, size, fragment);
  assert(_map.get(offset) == fragment, "");
}

void ZFragmentTable::remove(ZFragment* fragment) {
  const uintptr_t offset = fragment->old_start();
  const size_t size = fragment->old_size();

  assert(_map.get(offset) == fragment, "Invalid entry");
  _map.put(offset, size, NULL);
}
