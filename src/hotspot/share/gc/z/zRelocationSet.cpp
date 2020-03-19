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
    ZPage* new_page = ZHeap::heap()->alloc_page(old_page->type(), old_page->size(), flags, true /* don't change top */);
    new_page->set_top(old_page->live_bytes());
    _fragments[j++] = ZFragment::create(old_page, new_page);
  }
}

void ZRelocationSet::reset() {
  for (size_t i = 0; i < _nfragments; i++) {
    ZFragment::destroy(_fragments[i]);
    _fragments[i] = NULL;
  }
}
