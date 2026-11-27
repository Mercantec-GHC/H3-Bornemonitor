#pragma once
#include <stddef.h>
#include <stdint.h>

namespace TimeUtils {
bool parseUtc(const char* timestamp, uint32_t& epoch);
void formatUtcTime(uint32_t epoch, char* output, size_t size);
void formatDuration(uint32_t seconds, char* output, size_t size);
}
