#ifndef PMC_H
#define PMC_H

// Reads the core-cycle performance counter from userspace with rdpmc.
//
// Why not rdtsc: the TSC is a *clock*, driven by a ~100 MHz reference and
// advanced by the nominal frequency multiplier each reference tick. On a
// 3.65 GHz nominal part that is +36.5 every 10 ns, so every reading is a
// multiple of 36.5 and the instrument cannot resolve an operation shorter
// than 10 ns.
//
// The PMU core-cycle counter is a different register in a different clock
// domain: it increments once per actual core clock, in steps of 1. Same
// instruction cost to read, no stepping.

#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <x86intrin.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

class PmcCycles {
public:
    PmcCycles() = default;
    ~PmcCycles() {
        if (page_) munmap((void*)page_, sysconf(_SC_PAGESIZE));
        if (fd_ >= 0) close(fd_);
    }
    PmcCycles(const PmcCycles&) = delete;
    PmcCycles& operator=(const PmcCycles&) = delete;

    // Call once at startup, on the thread that will do the measuring.
    void open() {
        perf_event_attr attr{};
        attr.type           = PERF_TYPE_HARDWARE;
        attr.config         = PERF_COUNT_HW_CPU_CYCLES;
        attr.size           = sizeof(attr);
        attr.exclude_kernel = 1;   // don't count time in interrupts / syscalls
        attr.exclude_hv     = 1;
        attr.pinned         = 1;   // keep it on hardware, never multiplexed
        attr.disabled       = 0;

        // pid 0 = this thread, cpu -1 = whichever CPU the thread is on.
        fd_ = int(syscall(__NR_perf_event_open, &attr, 0, -1, -1, 0));
        if (fd_ < 0)
            throw std::runtime_error(std::string("perf_event_open failed: ") + strerror(errno) +
                                     "  (check kernel.perf_event_paranoid)");

        page_ = (perf_event_mmap_page*)mmap(nullptr, sysconf(_SC_PAGESIZE),
                                            PROT_READ, MAP_SHARED, fd_, 0);
        if (page_ == MAP_FAILED)
            throw std::runtime_error(std::string("mmap of perf fd failed: ") + strerror(errno));

        // The kernel publishes which hardware counter it assigned. 0 means the
        // event is not currently on hardware, so rdpmc would read nothing.
        // With pinned = 1 this should not happen; fail loudly if it does.
        idx_ = page_->index;
        if (idx_ == 0)
            throw std::runtime_error("perf counter not scheduled on hardware (index 0); "
                                     "check /sys/bus/event_source/devices/cpu/rdpmc is 1 or 2");
        --idx_;   // rdpmc takes a 0-based index

        width_mask_ = (page_->pmc_width >= 64) ? ~0ULL
                                               : ((1ULL << page_->pmc_width) - 1);
    }

    // Hot path: identical shape and cost to the old get_tsc().
    // The fences are still required - rdpmc is not serialising, so without
    // them the read can drift past the work it is meant to bracket.
    inline uint64_t read() const {
        unsigned aux;
        (void)__rdtscp(&aux);        // serialise against earlier instructions
        uint64_t c = __rdpmc(idx_);
        _mm_lfence();                // stop later work being hoisted above it
        return c & width_mask_;
    }

    // Counters are narrower than 64 bits (48 on most parts), so a naive
    // subtraction is wrong across a wrap. Intervals here are microseconds at
    // most, but the mask costs nothing.
    inline uint64_t delta(uint64_t before, uint64_t after) const {
        return (after - before) & width_mask_;
    }

    uint32_t index() const { return idx_; }
    uint16_t width() const { return page_ ? page_->pmc_width : 0; }

private:
    int                   fd_   = -1;
    perf_event_mmap_page* page_ = nullptr;
    uint32_t              idx_  = 0;
    uint64_t              width_mask_ = ~0ULL;
};

#endif