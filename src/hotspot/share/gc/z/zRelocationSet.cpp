/*
 * Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "precompiled.hpp"
#include "gc/z/zFragment.hpp"
#include "gc/z/zRelocationSet.hpp"
#include "gc/z/zAllocationFlags.hpp"
#include "gc/z/zHeap.inline.hpp"
#include "memory/allocation.hpp"

class ZLiveMapIterator : public ObjectClosure {
private:
  ZHeap* _heap;
  ZFragment* _fragment;
  const size_t _page_size;
  ZFragmentEntry *_current_entry;
  size_t _top;
  bool _allocated_on_new_page;

public:
  ZLiveMapIterator(ZFragment* fragment, size_t overlapping_bytes, size_t page_size) :
    _heap(ZHeap::heap()),
    _fragment(fragment),
    _page_size(page_size),
    _current_entry(fragment->entries_begin()),
    _top(overlapping_bytes),
    _allocated_on_new_page(false)
  {
    _current_entry->set_live_bytes_before_fragment(_top);
  }

  size_t overlapping_bytes() {
    return _top;
  }

  size_t alloc(size_t obj_size) {
    if (_top + obj_size <= _page_size) {
      _top += obj_size;
    } else {
      _top = 0;
    }
    return _top;
  }

  virtual void do_object(oop obj) {
    const uintptr_t from_offset = ZAddress::offset(ZOop::to_address(obj));
    const size_t obj_size = ZUtils::object_size(ZAddress::good(from_offset));
    const size_t internal_index = _fragment->offset_to_internal_index(from_offset);
    ZFragmentEntry* entry_for_offset = _fragment->find(from_offset);

    // Copy liveness information
    entry_for_offset->set_liveness(internal_index);

    // Store object size
    entry_for_offset->set_size_bit(internal_index, obj_size);

    // Allocate for object
    if (_current_entry < entry_for_offset) {
      _current_entry = entry_for_offset;
      _current_entry->set_live_bytes_before_fragment(_top);
    }

    uintptr_t allocated_obj = alloc(obj_size);

    if (allocated_obj == 0) {
      if (_allocated_on_new_page) {
        _fragment->add_page_break(from_offset);
      } else {
        _fragment->reset();
        assert(_top == 0, "should not have anything allocated");
        _current_entry->set_live_bytes_before_fragment(_top);
      }
      alloc(obj_size);
     }
    _allocated_on_new_page = true;
  }
};

ZRelocationSet::ZRelocationSet() :
  _fragments(NULL),
  _nfragments(0) {}

void ZRelocationSet::populate(ZPage* const* group0, size_t ngroup0,
                              ZPage* const* group1, size_t ngroup1) {
  _nfragments = ngroup0 + ngroup1;
  _fragments = REALLOC_C_HEAP_ARRAY(ZFragment*, _fragments, _nfragments, mtGC);

  size_t fragment_index = 0;

  // Populate group 0 (medium)
  size_t overlapping_bytes = 0;
  const size_t page_size_ngroup0 = ngroup0 > 0 ? group0[0]->size() : 0;
  ZFragment* previous_fragment = NULL;
  for (size_t i = 0; i < ngroup0; i++) {
    ZPage* old_page = group0[i];
    ZFragment* fragment = ZFragment::create(old_page, previous_fragment);

    ZLiveMapIterator cl = ZLiveMapIterator(fragment, overlapping_bytes, page_size_ngroup0);
    old_page->_livemap.iterate(&cl, ZAddress::good(old_page->start()), old_page->object_alignment_shift());
    overlapping_bytes = cl.overlapping_bytes();
    previous_fragment = fragment;
    _fragments[fragment_index++] = fragment;
  }

  // Populate group 1 (small)
  const size_t page_size_ngroup1 = ngroup1 > 0 ? group1[0]->size() : 0;
  overlapping_bytes = 0;
  previous_fragment = NULL;
  for (size_t i = 0; i < ngroup1; i++) {
    ZPage* old_page = group1[i];
    ZFragment* fragment = ZFragment::create(old_page, previous_fragment);

    ZLiveMapIterator cl = ZLiveMapIterator(fragment, overlapping_bytes, page_size_ngroup1);
    old_page->_livemap.iterate(&cl, ZAddress::good(old_page->start()), old_page->object_alignment_shift());
    overlapping_bytes = cl.overlapping_bytes();
    previous_fragment = fragment;
    _fragments[fragment_index++] = fragment;
  }
}

void ZRelocationSet::reset() {
  for (size_t i = 0; i < _nfragments; i++) {
    ZFragment::destroy(_fragments[i]);
    _fragments[i] = NULL;
  }
}
