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
#include "gc/z/zForwarding.hpp"
#include "gc/z/zRelocationSet.hpp"
#include "memory/allocation.hpp"
#include <iostream>

ZRelocationSet::ZRelocationSet() :
    _forwardings(NULL),
    _nforwardings(0) {}

void ZRelocationSet::populate(ZPage* const* group0, size_t ngroup0,
                              ZPage* const* group1, size_t ngroup1) {
  _nforwardings = ngroup0 + ngroup1;
  _forwardings = REALLOC_C_HEAP_ARRAY(ZForwarding*, _forwardings, _nforwardings, mtGC);

  size_t j = 0;


  size_t junk = 0;
  size_t count_zforwardingentry = 0;
  log_info(gc)("Relocating Set (small pages): " SIZE_FORMAT, ngroup1);
  log_info(gc)("Relocating Set (medium pages): " SIZE_FORMAT, ngroup0);
  // Populate group 0
  for (size_t i = 0; i < ngroup0; i++) {
    _forwardings[j++] = ZForwarding::create(group0[i], &junk);
  }

  // Populate group 1
  for (size_t i = 0; i < ngroup1; i++) {
    _forwardings[j++] = ZForwarding::create(group1[i], &count_zforwardingentry);
  }
  size_t zforwen = count_zforwardingentry * sizeof(ZForwardingEntry);
  size_t zfrage = ngroup1 * 8192 * sizeof(uint64_t);
  log_info(gc)("Total amount of allocated ZForwardingEntry: " SIZE_FORMAT, count_zforwardingentry);
  log_info(gc)("Total allocated ZForwardingEntry size: " SIZE_FORMAT, zforwen);
  log_info(gc)("Total allocated ZFragmentEntry size: " SIZE_FORMAT, zfrage);

  //std::cout << (float)zforwen/(float)zfrage << std::endl;


}

void ZRelocationSet::reset() {
  for (size_t i = 0; i < _nforwardings; i++) {
    ZForwarding::destroy(_forwardings[i]);
    _forwardings[i] = NULL;
  }
}
