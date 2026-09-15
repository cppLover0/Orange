#pragma once

namespace process {
    int id();
    void lock();
    void unlock();
    long last_sys();
};