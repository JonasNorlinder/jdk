#include "precompiled.hpp"
#include "gc/z/zFragmentTable.inline.hpp"
#include "gc/z/zGlobals.hpp"
#include "gc/z/zGranuleMap.inline.hpp"
#include "utilities/debug.hpp"
#include "gc/z/zFragment.inline.hpp"

ZFragmentTable::ZFragmentTable() :
  _map(ZAddressOffsetMax) {}

void ZFragmentTable::insert(ZFragment* fragment) {
  // TODO add transfer of live map to fragment entries here!
  const uintptr_t offset = fragment->old_start();
  const size_t size = fragment->old_size();
  assert(_map.get(offset) == NULL, "Invalid entry");
  _map.put(offset, size, fragment);
  fragment->fill_entires();
}

void ZFragmentTable::remove(ZFragment* fragment) {
  const uintptr_t offset = fragment->start();
  const size_t size = fragment->size();

  assert(_map.get(offset) == fragment, "Invalid entry");
  _map.put(offset, size, NULL);
}
