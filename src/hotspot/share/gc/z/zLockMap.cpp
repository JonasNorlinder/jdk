#include "gc/z/zLockMap.hpp"
#include "gc/z/zGlobals.hpp"
#include "gc/z/zGranuleLockMap.hpp"

ZLockMap::ZLockMap() :
  _map(ZAddressOffsetMax) {
}
