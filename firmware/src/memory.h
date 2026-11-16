// What Rory remembers about her friend: /rory/memory.txt on the SD card.
// One fact per line. She adds a line when she learns something new; grown-ups
// can open the file on a computer to read, change or delete anything.
#pragma once
#include <Arduino.h>

namespace Memory {
void begin();                    // read the file (after the SD card is up)
const String& text();            // all facts, one per line (sent with each question)
void add(const String& fact);    // remember something new (saved straight away)
}  // namespace Memory
