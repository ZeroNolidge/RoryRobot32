#include "memory.h"
#include <FS.h>
#include <SD_MMC.h>
#include <vector>

namespace {
const char* kPath = "/rory/memory.txt";
const size_t kMaxFacts = 60;  // oldest ones are forgotten after this
std::vector<String> comments;  // grown-up notes (# lines), kept as they are
std::vector<String> facts;
String joined;
bool ready = false;

const char* kHeader =
    "# What Rory remembers about her friend. She adds a line when she learns something new.\n"
    "# Grown-ups: change or delete any line. One fun fact per line, e.g.  His name is Sam.\n"
    "# (No addresses, school, last names or other private details.)\n";

void rebuild() {
  joined = "";
  for (auto& f : facts) {
    joined += f;
    joined += '\n';
  }
}

void save() {
  File f = SD_MMC.open(kPath, FILE_WRITE);
  if (!f) {
    Serial.println("[memory] couldn't save memory.txt");
    return;
  }
  if (comments.empty()) f.print(kHeader);
  for (auto& c : comments) f.println(c);
  for (auto& x : facts) f.println(x);
  f.close();
}
}  // namespace

namespace Memory {

void begin() {
  ready = SD_MMC.cardType() != CARD_NONE;
  if (!ready) return;
  File f = SD_MMC.open(kPath);
  if (f) {
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.isEmpty()) continue;
      if (line.startsWith("#")) comments.push_back(line);
      else facts.push_back(line);
    }
    f.close();
  }
  while (facts.size() > kMaxFacts) facts.erase(facts.begin());
  rebuild();
  Serial.printf("[memory] Rory remembers %u things about her friend\n", (unsigned)facts.size());
}

const String& text() { return joined; }

void add(const String& fact) {
  String f = fact;
  f.replace("\n", " ");
  f.replace("\r", " ");
  f.trim();
  if (!ready || f.length() < 4) return;
  for (auto& x : facts)
    if (x.equalsIgnoreCase(f)) return;  // already knew that
  facts.push_back(f);
  while (facts.size() > kMaxFacts) facts.erase(facts.begin());
  rebuild();
  save();
  Serial.printf("[memory] learned: %s\n", f.c_str());
}

}  // namespace Memory
