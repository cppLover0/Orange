
#include "generic/status/status.hpp"
#include "drivers/serial/serial.hpp"

void main() {
    Serial::Init();
    Serial::WriteString("Hello, World !\nFrom serial !\n");
    Serial::printf("Hello, World 0x%p %s",0xFF,"vc");
}