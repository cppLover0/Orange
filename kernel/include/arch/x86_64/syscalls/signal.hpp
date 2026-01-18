
#include <arch/x86_64/interrupts/idt.hpp>
#include <cstdint>

#include <etc/list.hpp>

#pragma once

void signal_ret();

typedef struct {
    std::uint16_t sig;
} pending_signal_t;

class signalmanager {
private:
    pending_signal_t* sigs = 0;
    Lists::Bitmap* bitmap = 0;
    int size = 0;
    int total_size = 0;
    locks::spinlock lock;

    int allocate() {
        for(int i = 0; i < 128; i++) {
            if(!this->bitmap->test(i))
                return i;
        }
        return -1;
    }

    int find_free() {
        for(int i = 0; i < 128; i++) {
            if(this->bitmap->test(i))
                return i;
        }
        return -1;
    }

public:

    signalmanager() {
        this->total_size = 128;
        sigs = new pending_signal_t[this->total_size];
        memset(sigs,0,sizeof(pending_signal_t) * this->total_size);
        bitmap = new Lists::Bitmap(this->total_size);
    }

    ~signalmanager() {
        delete (void*)this->sigs;
        delete (void*)this->bitmap;
    }

    int pop(pending_signal_t* out) {
        this->lock.lock();
        int idx = find_free();
        if(idx == -1) { this->lock.unlock();
            return -1; } 
        memcpy(out,&sigs[idx],sizeof(pending_signal_t));
        this->bitmap->clear(idx);
        this->lock.unlock();
        return 0;
    }

    int push(pending_signal_t* src) {
        this->lock.lock();
        int idx = allocate();
        if(idx == -1) { this->lock.unlock();
            return -1; }
        memcpy(&sigs[idx],src,sizeof(pending_signal_t));
        this->bitmap->set(idx);
        this->lock.unlock();
        return 0;
    }

    int is_not_empty() {
        this->lock.lock();
        int idx = find_free();
        if(idx == -1) { this->lock.unlock();
            return 0; }
        this->lock.unlock();
        return 1;
    }

};