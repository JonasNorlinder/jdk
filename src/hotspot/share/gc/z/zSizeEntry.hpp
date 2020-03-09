#ifndef SHARE_GC_Z_ZSIZEENTRY_HPP
#define SHARE_GC_Z_ZSIZEENTRY_HPP

#include <cstdint>

class ZSizeEntry {
private:

public:
  ZSizeEntry() :
    entry(0) {
    assert(false, "not here!");
  }

  uint64_t         entry;
};

#endif // SHARE_GC_Z_ZSIZEENTRY_HPP
