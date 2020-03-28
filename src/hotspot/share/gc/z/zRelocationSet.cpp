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
  ZPage *_curr;
  ZAllocationFlags _flags;
  size_t _live;

public:
  ZLiveMapIterator(ZFragment* fragment, ZAllocationFlags flags) :
    _heap(ZHeap::heap()),
    _fragment(fragment),
    _curr(fragment->new_page()),
    _flags(flags),
    _live(0)
  {}

  virtual void do_object(oop obj) {
    uintptr_t offset = ZAddress::offset(ZOop::to_address(obj));
    uintptr_t addr = ZAddress::good(offset);

    size_t obj_size = ZUtils::object_size(addr);
    uintptr_t allocated_obj = _curr->alloc_object(obj_size);
    ZFragmentEntry *e = _fragment->find(offset);

    if (allocated_obj) {
      _live += obj_size;
      e->set_live_bytes(_live);
    } else {
      assert(false, "overlapping not supported yet");
    }

    _heap->global_lock.lock();
    _heap->add_expected(offset, ZAddress::offset(allocated_obj));
    _heap->global_lock.unlock();
  }
};

ZRelocationSet::ZRelocationSet() :
    _fragments(NULL),
    _nfragments(0) {}

void ZRelocationSet::populate(ZPage* const* group0, size_t ngroup0,
                              ZPage* const* group1, size_t ngroup1) {
  _nfragments = ngroup0 + ngroup1;
  _fragments = REALLOC_C_HEAP_ARRAY(ZFragment*, _fragments, _nfragments, mtGC);

  size_t j = 0;

  ZAllocationFlags flags;
  flags.set_relocation();
  flags.set_non_blocking();
  flags.set_worker_thread();

  // Populate group 0 (medium)
  for (size_t i = 0; i < ngroup0; i++) {
    ZPage* old_page = group0[i];
     ZPage* new_page = ZHeap::heap()->alloc_page(old_page->type(), old_page->size(), flags, true /* don't change top */);
    new_page->set_top(old_page->live_bytes());
    _fragments[j++] = ZFragment::create(old_page, new_page);
  }

  // Populate group 1 (small)
  for (size_t i = 0; i < ngroup1; i++) {
    ZPage* old_page = group1[i];
    ZPage* new_page = ZHeap::heap()->alloc_page(old_page->type(), old_page->size(), flags, false);

    ZFragment* fragment = ZFragment::create(old_page, new_page);
   ZLiveMapIterator cl = ZLiveMapIterator(fragment, flags);
    old_page->_livemap.iterate(&cl, ZAddress::good(old_page->start()), old_page->object_alignment_shift());

    _fragments[j++] = fragment;
  }
}

void ZRelocationSet::reset() {
  for (size_t i = 0; i < _nfragments; i++) {
    ZFragment::destroy(_fragments[i]);
    _fragments[i] = NULL;
  }
}
