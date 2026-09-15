#pragma once

#include <utils/linux.hpp>

// if someone will ever see this dont ask why i made this in new file

struct timerfd_spec {
	struct timespec it_interval;
	struct timespec it_value;
};
