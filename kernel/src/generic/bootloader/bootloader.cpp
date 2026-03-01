
#include <cstdint>
#include <generic/bootloader/limine.hpp>
#include <generic/bootloader/bootloader.hpp>

bootloader::limine limine_bootloader;

void bootloader::init() {
    bootloader::bootloader = &limine_bootloader;
}