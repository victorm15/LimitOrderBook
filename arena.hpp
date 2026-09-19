#ifndef ARENA_H
#define ARENA_H

#include "types.hpp"
#include <assert.h>

class OrderArena {

private:
    std::vector<Order> slots_;
    Handle free_head_;
public:

    explicit OrderArena(size_t capacity) {
        slots_.resize(capacity);
        for(size_t i = 0; i < capacity; ++i) {
            slots_[i].next = Handle(i+1);
        }
        slots_.back().next = NIL;
        free_head_ = 0;
    }

    Handle allocate() {
        assert(free_head_ != NIL);
        Handle h = free_head_;
        free_head_ = slots_[h].next;
        return h;
    }

    void release(Handle h) {
        slots_[h].next = free_head_;
        free_head_ = h;
    }

    Order& operator[](Handle h) { return slots_[h]; }
    const Order& operator[](Handle h) const { return slots_[h]; }

};


#endif