#include "precompiled.hpp"
#include "memory/allocation.hpp"
#include "gc/z/zFragment.inline.hpp"
#include "gc/z/zPage.inline.hpp"
#include "gc/z/zPageAllocator.hpp"
#include "gc/z/zAllocationFlags.hpp"
#include "gc/z/zHeap.inline.hpp"
#include "gc/z/zAddress.hpp"

ZFragment::ZFragment(ZPage* old_page, ZPage* new_page, size_t nentries, size_t n_sizeentries)
  : _entries(nentries),
    _object_alignment_shift(old_page->object_alignment_shift()),
    _old_page(old_page),
    _ops(old_page->start()),
    _old_virtual(old_page->virtual_memory()),
    _new_page(new_page),
    _snd_page(NULL),
    _refcount(1),
    _pinned(false) {
  assert(old_page != NULL, "");

  for (ZSizeEntry* e = size_entries_begin(); e < size_entries_begin() + n_sizeentries; e= e + 1) {

    e->entry = 0;
  }
}

ZFragment* ZFragment::create(ZPage* old_page, ZPage* new_page) {
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
  const size_t n_sizeentries = size / 8;
  ZFragment* fragment = ::new (AttachedArray::alloc(nentries+n_sizeentries)) ZFragment(old_page, new_page, nentries, n_sizeentries);

  fragment->_conversion_constant = (old_page->start() >> 5);

  return fragment;
}

void ZFragment::destroy(ZFragment* fragment) {
  AttachedArray::free(fragment);
}
