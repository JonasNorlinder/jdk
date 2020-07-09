#include "precompiled.hpp"
#include "memory/allocation.hpp"
#include "gc/z/zFragment.inline.hpp"
#include "gc/z/zPage.inline.hpp"
#include "gc/z/zPageAllocator.hpp"
#include "gc/z/zAllocationFlags.hpp"
#include "gc/z/zHeap.inline.hpp"
#include "gc/z/zAddress.hpp"

ZFragment::ZFragment(ZPage* old_page, size_t nentries)
  : _entries(nentries),
    _object_alignment_shift(old_page->object_alignment_shift()),
    _old_page(old_page),
    _ops(old_page->start()),
    _page_type(old_page->type()),
    _previous_fragment(NULL),
    _page_size(old_page->size()),
    _old_virtual(old_page->virtual_memory()),
    _new_page(NULL),
    _refcount(1),
    _first_from_offset_mapped_to_snd_page(0),
    _page_break_entry_index(0) {}

ZFragment* ZFragment::create(ZPage* old_page) {
  assert(old_page != NULL, "");
  assert(old_page->live_objects() > 0, "Invalid value");
  const size_t size = old_page->size();
  const size_t nentries = size / 256 + 1;
  ZFragment* fragment = ::new (AttachedArray::alloc(nentries)) ZFragment(old_page, nentries);

  return fragment;
}

void ZFragment::destroy(ZFragment* fragment) {
  AttachedArray::free(fragment);
}
