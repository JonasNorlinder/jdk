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
  ZFragment* _previous_fragment;
  const size_t _page_size;
  ZPage *_current_page;
  ZFragmentEntry *_current_entry;
  size_t _top;
  bool _first_allocation;

public:
  ZLiveMapIterator(ZFragment* fragment, size_t page_size, size_t previous_fragment_top,
                   ZFragment* previous_fragment) :
    _heap(ZHeap::heap()),
    _fragment(fragment),
    _previous_fragment(previous_fragment),
    _page_size(page_size),
    _current_entry(fragment->entries_begin()),
    _top(previous_fragment_top),
    _first_allocation(true)
  {
    _current_entry->set_live_bytes_before_fragment(_top);
  }

  size_t current_top() {
    return _top;
  }

  bool move_top(size_t obj_size) {
    if (_top + obj_size < _page_size) {
      _top += obj_size;
      return false;
    }
    _top = obj_size;
    return true;
  }

  virtual void do_object(oop obj) {
    const uintptr_t from_offset = ZAddress::offset(ZOop::to_address(obj));
    const size_t obj_size = ZUtils::object_size(ZAddress::good(from_offset));
    const size_t internal_index = _fragment->offset_to_internal_index(from_offset);
    ZFragmentEntry* entry_for_offset = _fragment->find(from_offset);

    // Allocate for object
    if (_current_entry < entry_for_offset) {
      _current_entry = entry_for_offset;
      _current_entry->set_live_bytes_before_fragment(_top);
    }

    bool page_break = move_top(obj_size);

    // Overlapping pages
    if (page_break) {
      entry_for_offset++;
      _current_entry = entry_for_offset;
      _current_entry->set_live_bytes_before_fragment(0);
      if (!_first_allocation) {
        _fragment->add_page_break(from_offset, _previous_fragment);
      }
    }

    // Copy liveness information
    entry_for_offset->set_liveness(internal_index);

    // Store object size
    entry_for_offset->set_size_bit(internal_index, obj_size);
    _first_allocation = false;
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

  const size_t page_size_ngroup0 = ngroup0 > 0 ? group0[0]->size() : 0;
  size_t previous_fragment_top = 0;
  ZFragment* previous_fragment = NULL;
  // Populate group 0 (medium)
  for (size_t i = 0; i < ngroup0; i++) {
    ZPage* old_page = group0[i];
    ZFragment* fragment = ZFragment::create(old_page);

    ZLiveMapIterator cl = ZLiveMapIterator(fragment, page_size_ngroup0, previous_fragment_top, previous_fragment);
    old_page->_livemap.iterate(&cl, ZAddress::good(old_page->start()), old_page->object_alignment_shift());
    previous_fragment_top = cl.current_top();
    previous_fragment = fragment;
    _fragments[fragment_index++] = fragment;
  }

  const size_t page_size_ngroup1 = ngroup1 > 0 ? group1[0]->size() : 0;
  previous_fragment_top = 0;
  previous_fragment = NULL;
  // Populate group 1 (small)
  for (size_t i = 0; i < ngroup1; i++) {
    ZPage* old_page = group1[i];
    ZFragment* fragment = ZFragment::create(old_page);

    ZLiveMapIterator cl = ZLiveMapIterator(fragment, page_size_ngroup1, previous_fragment_top, previous_fragment);
    old_page->_livemap.iterate(&cl, ZAddress::good(old_page->start()), old_page->object_alignment_shift());
    previous_fragment_top = cl.current_top();
    previous_fragment = fragment;
    _fragments[fragment_index++] = fragment;

    ZFragmentEntry* e = fragment->entries_begin();
    size_t start = fragment->_previous_fragment ? previous_fragment_top : 0;
    for(size_t curr_size=start,idx=0;e<fragment->entries_end();e++,idx++) {
      size_t tmp = e->live_bytes_before_fragment();
      if (idx == fragment->_page_break_entry_index && fragment->_first_from_offset_mapped_to_snd_page) {
        assert(tmp == 0, "");
      } else {
        assert(curr_size <= tmp || tmp == 0, "");
        assert(tmp < old_page->size(), "");
      }
      curr_size = tmp;
    }
  }
}

void ZRelocationSet::reset() {
  for (size_t i = 0; i < _nfragments; i++) {
    ZFragment::destroy(_fragments[i]);
    _fragments[i] = NULL;
  }
}
