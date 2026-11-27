#include "TimeUtils.h"
#include <stdio.h>
#include <string.h>

namespace {
bool leap(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
int digits(const char* text, int offset, int count) {
  int value = 0;
  for (int i = 0; i < count; ++i) value = value * 10 + text[offset + i] - '0';
  return value;
}
}

bool TimeUtils::parseUtc(const char* timestamp, uint32_t& epoch) {
  if (!timestamp || strlen(timestamp) != 20) return false;
  for (int i = 0; i < 20; ++i) {
    const char expected = i == 4 || i == 7 ? '-'
      : i == 10 ? 'T' : i == 13 || i == 16 ? ':' : i == 19 ? 'Z' : 0;
    if (expected ? timestamp[i] != expected
                 : timestamp[i] < '0' || timestamp[i] > '9') return false;
  }
  const int year = digits(timestamp, 0, 4);
  const int month = digits(timestamp, 5, 2);
  const int day = digits(timestamp, 8, 2);
  const int hour = digits(timestamp, 11, 2);
  const int minute = digits(timestamp, 14, 2);
  const int second = digits(timestamp, 17, 2);
  if (year < 2024 || year > 2106 || month < 1 || month > 12 ||
      hour > 23 || minute > 59 || second > 59) return false;
  const int months[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  const int maxDay = months[month - 1] + (month == 2 && leap(year) ? 1 : 0);
  if (day < 1 || day > maxDay) return false;
  uint64_t days = 0;
  for (int y = 1970; y < year; ++y) days += leap(y) ? 366 : 365;
  for (int m = 1; m < month; ++m) days += months[m - 1] + (m == 2 && leap(year) ? 1 : 0);
  days += day - 1;
  const uint64_t result = days * 86400ULL + hour * 3600UL + minute * 60UL + second;
  if (result > 0xffffffffULL) return false;
  epoch = static_cast<uint32_t>(result);
  return true;
}

void TimeUtils::formatUtcTime(uint32_t epoch, char* output, size_t size) {
  snprintf(output, size, "%02lu:%02lu", static_cast<unsigned long>(epoch % 86400UL / 3600UL), static_cast<unsigned long>(epoch % 3600UL / 60UL));
}

void TimeUtils::formatDuration(uint32_t seconds, char* output, size_t size) {
  const unsigned long hours = seconds / 3600UL;
  if (hours < 100) {
    snprintf(output, size, "%02lu:%02lu:%02lu", hours,
             static_cast<unsigned long>(seconds % 3600UL / 60UL),
             static_cast<unsigned long>(seconds % 60UL));
  } else {
    snprintf(output, size, "%lud %02luh", hours / 24UL, hours % 24UL);
  }
}
