#ifndef PMC_H
#define PMC_H

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

        // kernel publishes hardware counter it assigned
        // 0 meaning the event is not currently on hardware 
        idx_ = page_->index;
        if (idx_ == 0)
            throw std::runtime_error("perf counter not scheduled on hardware (index 0); "
                                     "check /sys/bus/event_source/devices/cpu/rdpmc is 1 or 2");
        --idx_;

        width_mask_ = (page_->pmc_width >= 64) ? ~0ULL
                                               : ((1ULL << page_->pmc_width) - 1);
    }

    inline uint64_t read() const {
        _mm_lfence();
        uint64_t c = __rdpmc(idx_);
        _mm_lfence();
        return c & width_mask_;
    }

    inline uint64_t delta(uint64_t before, uint64_t after) const {
        return (after - before) & width_mask_;
    }

private:
    int                   fd_   = -1;
    perf_event_mmap_page* page_ = nullptr;
    uint32_t              idx_  = 0;
    uint64_t              width_mask_ = ~0ULL;
};

#endif