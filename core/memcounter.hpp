#ifndef DEF_FLEXOGRAPH_PROFILE_MEMCOUNTER
#define DEF_FLEXOGRAPH_PROFILE_MEMCOUNTER

#include <sys/resource.h>
#include <cstdint>

namespace _flexograph_profile {
  /**
   * Copied from Blaze. This class is used to measure the memory usage of the program.
   */
  class MemoryCounter {
  public:
    MemoryCounter() {
      getrusage(RUSAGE_SELF, &__memory);
      __previous_mem = __memory.ru_maxrss;
      __maj_faults = __memory.ru_majflt;
      __min_faults = __memory.ru_minflt;
      __inblock = __memory.ru_inblock;
      __oublock = __memory.ru_oublock;
    }

    ~MemoryCounter() {
      getrusage(RUSAGE_SELF, &__memory);
      uint64_t used_mem = __memory.ru_maxrss - __previous_mem;
      uint64_t maj_faults = __memory.ru_majflt - __maj_faults;
      uint64_t min_faults = __memory.ru_minflt - __min_faults;
      uint64_t inblock = __memory.ru_inblock - __inblock;
      uint64_t oublock = __memory.ru_oublock - __oublock;
      printf("MemoryCounter: %lu MB -> %lu MB, %lu MB total\n", __previous_mem/1024, __memory.ru_maxrss/1024, used_mem/1024);
      printf("MemoryCounter: %lu major faults, %lu minor faults\n", maj_faults, min_faults);
      printf("MemoryCounter: %lu block input operations, %lu block output operations\n", inblock, oublock);
    }

  private:
    struct rusage __memory;
    uint64_t __previous_mem;
    uint64_t __maj_faults;
    uint64_t __min_faults;
    uint64_t __inblock;
    uint64_t __oublock;
  };
} // end namespace _flexograph_profile

#endif
