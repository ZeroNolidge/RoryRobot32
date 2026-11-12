// Renders Rory's expressions to PPM images on a PC, using the exact same
// face code as the robot.  Build & run:  ./tools/face-preview/make_previews.sh
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include "../../firmware/src/face.h"

static void save(const Raster& r, const std::string& path) {
  FILE* f = fopen(path.c_str(), "wb");
  fprintf(f, "P6 %d %d 255\n", r.w, r.h);
  for (int i = 0; i < r.w * r.h; i++) {
    uint16_t c = r.px[i];
    unsigned char rgb[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31),
                            (unsigned char)(((c >> 5) & 63) * 255 / 63),
                            (unsigned char)((c & 31) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

struct Shot { const char* name; Expression e; float lx, ly, talk; bool zzz; bool sparkle; };

int main(int argc, char** argv) {
  std::string out = argc > 1 ? argv[1] : ".";
  std::vector<uint16_t> fb(240 * 240);
  Raster r(fb.data(), 240, 240);
  Shot shots[] = {
      {"content", Expr::Content, 0, 0, 0, false, false},
      {"happy", Expr::Happy, 0, 0, 0, false, false},
      {"cheer", Expr::Cheer, 0, 0, 0, false, true},
      {"listening", Expr::Listening, 0, -0.4f, 0, false, false},
      {"thinking", Expr::Thinking, 0.7f, -0.8f, 0, false, false},
      {"surprised", Expr::Surprised, 0, 0, 0, false, false},
      {"talking", Expr::Talking, 0, 0, 0.75f, false, false},
      {"dizzy", Expr::Dizzy, 0, 0, 0, false, false},
      {"sleepy", Expr::Sleepy, 0, 0.3f, 0, false, false},
      {"asleep", Expr::Asleep, 0, 0, 0, true, false},
      {"love", Expr::Love, 0, 0, 0, false, false},
      {"look-left", Expr::Content, -1, 0.2f, 0, false, false},
  };
  for (auto& s : shots) {
    Face face;
    uint32_t t = 0;
    face.begin(t);
    face.setAutoLook(false);
    face.setExpression(s.e);
    face.lookAt(s.lx, s.ly);
    face.setTalkLevel(s.talk);
    face.setZzz(s.zzz);
    // run the animation ~1.2 s so smoothing settles, avoiding a blink frame
    for (int i = 0; i < 36; i++) { t += 33; face.update(t); }
    if (s.sparkle) face.sparkle(5000);
    face.update(t + 33);
    face.render(r);
    save(r, out + "/" + s.name + ".ppm");
  }
  return 0;
}
