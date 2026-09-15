#include <generic/timer_thread.hpp>
#include <generic/scheduling.hpp>
#include <utils/foreach.hpp>

timer_thread::timer_thread_guest* timer_thread::create(void (*callback)(void*), void* ctx, std::uint64_t counter, std::uint64_t interval, std::uint64_t ownership) {
    timer_thread::lock.lock();

    auto head = (timer_thread_guest*)timer_thread::head;
    auto found = (timer_thread_guest*)nullptr;

    foreach(head) {
        if(item->is_used == false) {
            item->is_used = true;
            found = item;
            break;
        }
    }

    if(found == nullptr) {
        found = new timer_thread_guest;
        found->is_used = true;
        found->next = timer_thread::head;
        timer_thread::head = found;
    }

    found->ctx = ctx;
    found->callback = callback;
    found->counter = counter;
    found->interval = interval;
    found->ownership = ownership;

    timer_thread::lock.unlock();
    return found;
}

void timer_thread::remove(timer_thread_guest* guest) {
    timer_thread::lock.lock();
    guest->is_used = false;
    timer_thread::lock.unlock();
}

void timer_thread_work(void* arg) {
    (void)arg;

    std::uint64_t last_timestamp = time::timer->current_nano();

    timer_thread::lock.lock();
    log("timer_thread", "im running !");

    auto head = timer_thread::head;
    while(true) {

        std::uint64_t ts = time::timer->current_nano();
        std::uint64_t delta = ts - last_timestamp;
        last_timestamp = ts;

        foreach(head) {
            if(item->is_used == false)
                continue;

            if(item->counter < delta) {
                item->callback(item->ctx);
                item->counter = item->interval;

                if(item->interval == 0) {
                    item->is_used = false;
                }

            } else {
                item->counter -= delta;
            }
        }

        timer_thread::lock.unlock();
        process::yield();
        timer_thread::lock.lock();

        head = timer_thread::head;
    }
}

void timer_thread::init() {
    timer_thread::timer = process::kthread(timer_thread_work, nullptr);
    process::wakeup(timer_thread::timer);
    log("timer_thread", "timer thread id %d", timer_thread::timer->id);
}