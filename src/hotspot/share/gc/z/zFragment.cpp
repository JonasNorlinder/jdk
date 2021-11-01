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
    _page_size(old_page->size()),
    _old_virtual(old_page->virtual_memory()),
    _new_page(NULL),
    _refcount(1) {}

ZFragment* ZFragment::create(ZPage* old_page) {
  assert(old_page != NULL, "");
  assert(old_page->live_objects() > 0, "Invalid value");
  const size_t size = old_page->size();

  // Use each bit in a 32 bit int to store liveness information,
  // where each bit spans one word in the page. One word = 8 bytes.
  //
  // Example: Page size is small: 2 MB,
  // then requiered number of entries to describe the liveness,
  // information is thus 2 MB / (32 words * 8 bytes) = 8 192
  const size_t nentries = size / 256;
  ZFragment* fragment = ::new (AttachedArray::alloc(nentries)) ZFragment(old_page, nentries);

  return fragment;
}

void ZFragment::destroy(ZFragment* fragment) {
  AttachedArray::free(fragment);
}
