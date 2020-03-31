#include "precompiled.hpp"
#include "gc/z/zFragmentTable.inline.hpp"
#include "gc/z/zGlobals.hpp"
#include "gc/z/zGranuleMap.inline.hpp"
#include "utilities/debug.hpp"
#include "gc/z/zFragment.inline.hpp"

class ZPageFragmentLiveMapIterator : public ObjectClosure {
private:
  ZFragment* _fragment;

public:
  ZPageFragmentLiveMapIterator(ZFragment* fragment) :
    _fragment(fragment) {
  }

  virtual void do_object(oop obj) {
    uintptr_t from_offset = ZAddress::offset(ZOop::to_address(obj));
    size_t obj_size = ZUtils::object_size(ZAddress::good(from_offset));

    ZFragmentEntry* entry_for_offset = _fragment->find(from_offset);
    size_t internal_index = _fragment->offset_to_internal_index(from_offset);
    entry_for_offset->set_liveness(internal_index);

    size_t p_index = _fragment->page_index(from_offset);
    ZSizeEntry* size_entry = _fragment->size_entries_begin() + p_index;
    size_entry->entry = obj_size;
  }
};


ZFragmentTable::ZFragmentTable() :
  _map(ZAddressOffsetMax) {}

void ZFragmentTable::insert(ZFragment* fragment) {
  const uintptr_t offset = fragment->old_start();
  const size_t size = fragment->old_size();
  assert(_map.get(offset) == NULL, "Invalid entry");
  _map.put(offset, size, fragment);
  assert(_map.get(offset) == fragment, "");

  ZPageFragmentLiveMapIterator cl = ZPageFragmentLiveMapIterator(fragment);
  fragment->old_page()->_livemap.iterate(&cl, ZAddress::good(fragment->old_page()->start()), fragment->old_page()->object_alignment_shift());

}

void ZFragmentTable::remove(ZFragment* fragment) {
  const uintptr_t offset = fragment->old_start();
  const size_t size = fragment->old_size();

  assert(_map.get(offset) == fragment, "Invalid entry");
  _map.put(offset, size, NULL);
}
