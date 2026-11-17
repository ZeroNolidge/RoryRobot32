// Rory the Robot - head (v2), for the Waveshare ESP32-S3-Touch-LCD-1.54
//
// The board stays in its own white 46 x 46 x 22.5 mm case. The case slides
// into the head from the back, the screen becomes Rory's face, and a back
// plate screws on. Behind the case there's room for the antenna LED, which
// shines up into a clear antenna on top. A hollow neck at the bottom lets the
// USB-C cable and the LED wires go down into her body (body coming later).
//
// Parts (pick one, render with F6, export STL):
//   part = "head"      the head shell                      almond yellow
//   part = "back"      back plate with speaker holes       almond yellow
//   part = "antenna"   antenna: stalk + ball               clear
//   part = "fit_test"  quick 20-minute ring to check the case fits (print this first!)
//   part = "all"       everything together, for looking at
//
// Hardware: 4 x M3 x 12 mm self-tapping screws for the back plate.
// The LED: the DIYables RGB LED module's 5 mm LED pushes up through the hole
// in the top from inside (press fit), and the antenna presses on over it.

part = "all";

/* ---------- the Waveshare case (outline drawing, mm) ---------- */
case_w = 46;     // width and height
case_d = 22.5;   // depth, front to back
case_r = 5.8;    // corner radius (front view)
// Openings - measure the real case and adjust if needed:
btn_y = [4, 16.4];    // buttons on top, measured from the case FRONT
usb_y = [3, 19.5];    // USB-C on the bottom, measured from the case front
usb_w = 16;           // room for the USB-C plug (straight or right-angle)
ear_d = 15;           // side windows (microSD slot side)

/* ---------- Rory's head ---------- */
clr   = 0.4;   // clearance around the case
wall  = 3.2;   // shell walls - strong for little hands
lip   = 1.6;   // frame in front of the screen
win   = 40.6;  // face window (the black glass is ~40 mm)
win_r = 3;
back_space = 16;   // room behind the case for the LED module and wires
plate_t = 2.4;     // back plate thickness
pad_h = 2;         // the plate is thicker around each screw...
head_pocket = 3.2; // ...so the screw heads sink in flush (socket or flat heads, up to 6.2 mm wide)

head_w = case_w + 2 * (clr + wall);     // 53.2 mm
head_r = case_r + clr + wall;
case_back = lip + case_d + clr;         // where the case's back sits
D = case_back + back_space;             // shell depth (the back plate goes on behind)

boss_d = 7;  pilot = 2.5;  screw_clear = 3.4;
boss_xz = case_w / 2 + clr - boss_d / 2 + 0.5;   // bosses tucked into the corners
bosses = [for (sx = [-1, 1], sz = [-1, 1]) [sx * boss_xz, sz * boss_xz]];

// antenna / LED
led_y = case_back + back_space / 2;   // over the middle of the back space
led_hole = 5.1;                       // 5 mm LED, press fit (its rim stops it)
socket_d = 8.4;  socket_h = 1.6;      // the antenna's peg sits in this
neck_w = 26; neck_d = 30; neck_h = 6; // collar that will sit in the body

$fn = 64;
YELLOW = "#E9C46A"; CLEAR = "#CFEFFF"; SCREEN = "#263238"; CYAN = "#52DAE8"; WHITE = "#F5F5F5";

/* ---------- helpers ---------- */
// rounded rectangle in the x-z plane, from y=0 to y=t
module rrect(w, h, r, t) { translate([0, t, 0]) rotate([90, 0, 0]) linear_extrude(t) offset(r) square([w - 2 * r, h - 2 * r], center = true); }
module rounded_box2d(w, d, r) { offset(r) square([w - 2 * r, d - 2 * r], center = true); }

/* ---------- head ---------- */
module head() {
  difference() {
    union() {
      rrect(head_w, head_w, head_r, D);
      // little ear rings on the sides
      for (s = [-1, 1]) translate([s * head_w / 2, lip + case_d / 2, 0]) rotate([0, 90, 0])
        cylinder(d = ear_d + 6, h = 1.6, center = true);
      // neck collar under the head
      translate([0, (lip + D) / 2, -head_w / 2 - neck_h + 0.01]) linear_extrude(neck_h)
        rounded_box2d(neck_w, neck_d, 5);
    }
    // the case pocket, open at the back
    translate([0, lip, 0]) rrect(case_w + 2 * clr, case_w + 2 * clr, case_r + clr, D);
    // face window with a soft chamfer
    translate([0, -1, 0]) rrect(win, win, win_r, lip + 2);
    translate([0, -0.01, 0]) hull() { rrect(win + 1.6, win + 1.6, win_r + 0.8, 0.01); translate([0, 0.8, 0]) rrect(win, win, win_r, 0.01); }
    // side windows (ears)
    for (s = [-1, 1]) translate([s * head_w / 2, lip + case_d / 2, 0]) rotate([0, 90, 0]) cylinder(d = ear_d, h = 2 * wall + 6, center = true);
    // buttons on top: one long window
    translate([0, lip + (btn_y[0] + btn_y[1]) / 2, head_w / 2 - wall - 1]) hull() for (x = [-16, 16])
      translate([x, 0, 0]) scale([1, (btn_y[1] - btn_y[0]) / 8, 1]) cylinder(d = 8, h = wall + 2);
    // LED hole + antenna socket on top
    translate([0, led_y, head_w / 2 - wall - 1]) cylinder(d = led_hole, h = wall + 2);
    translate([0, led_y, head_w / 2 - socket_h]) cylinder(d = socket_d, h = socket_h + 1);
    // neck: USB-C plug and wires go down through here
    translate([0, (lip + D) / 2, -head_w / 2 - neck_h - 1]) linear_extrude(wall + neck_h + 2)
      rounded_box2d(neck_w - 2 * 2.4, neck_d - 2 * 2.4, 3);
  }
  // corner bosses behind the case: they hold the case against the face and take the back-plate screws
  intersection() {  // keep the bosses inside the rounded shell
  translate([0, case_back - 0.01, 0]) rrect(head_w - 0.4, head_w - 0.4, head_r - 0.2, back_space + 0.01);
  for (b = bosses) translate([b[0], case_back, b[1]]) difference() {
    hull() {
      rotate([-90, 0, 0]) cylinder(d = boss_d, h = back_space - pad_h);
      // tie each boss into the corner walls so it's strong
      translate([sign(b[0]) * 3, 0, sign(b[1]) * 3]) rotate([-90, 0, 0]) cylinder(d = boss_d, h = back_space - pad_h);
    }
    translate([0, back_space - pad_h - 11, 0]) rotate([-90, 0, 0]) cylinder(d = pilot, h = 12);
  }
  }
}

/* ---------- back plate ---------- */
module back_plate() {
  inner = case_w + 2 * clr - 0.6;  // just inside the head's opening
  difference() {
    union() {
      rrect(head_w, head_w, head_r, plate_t);
      // a lip that fits just inside the head, so the plate lines up by itself
      translate([0, -1.2, 0]) difference() {
        rrect(inner, inner, case_r + clr - 0.3, 1.2);
        rrect(case_w + 2 * clr - 4, case_w + 2 * clr - 4, case_r, 1.2);
      }
      // thicker pads around the screws (they meet the head's screw posts)
      intersection() {
        translate([0, -pad_h, 0]) rrect(inner, inner, case_r + clr - 0.3, pad_h + 0.01);
        for (b = bosses) translate([b[0], -pad_h, b[1]]) rotate([-90, 0, 0]) cylinder(d = boss_d + 1, h = pad_h + 0.01);
      }
    }
    // speaker / microphone holes (a round grille)
    for (x = [-12 : 4 : 12], z = [-12 : 4 : 12]) if (x * x + z * z <= 13 * 13)
      translate([x, -2, z]) rotate([-90, 0, 0]) cylinder(d = 2.2, h = plate_t + 4, $fn = 16);
    // screw holes, with a pocket so the heads sit flush
    for (b = bosses) translate([b[0], 0, b[1]]) rotate([-90, 0, 0]) {
      translate([0, 0, -pad_h - 1]) cylinder(d = screw_clear, h = pad_h + plate_t + 2);
      translate([0, 0, plate_t - head_pocket]) cylinder(d = 6.4, h = head_pocket + 1);
    }
  }
}

/* ---------- antenna (clear) ---------- */
// Prints standing up, ball on top. The LED's dome slides into the bore; the
// solid clear plastic above it carries the light up into the ball.
ant_h = 16;      // stalk height above the head
ball_d = 13;
module antenna() {
  difference() {
    union() {
      translate([0, 0, -socket_h]) cylinder(d = socket_d - 0.25, h = socket_h + 0.01);  // peg (press fit)
      cylinder(d1 = 10, d2 = 7, h = ant_h);
      // ball, with a cone underneath so it prints without supports
      hull() {
        translate([0, 0, ant_h + ball_d / 2 - 1.5]) sphere(d = ball_d);
        translate([0, 0, ant_h - 2]) cylinder(d = 7, h = 0.1);
      }
    }
    translate([0, 0, -socket_h - 1]) cylinder(d = 5.6, h = socket_h + 1 + 6.5);  // room for the LED dome
  }
}

/* ---------- fit test: just the front 9 mm of the head ---------- */
module fit_test() {
  intersection() {
    head();
    translate([-50, -1, -head_w / 2]) cube([100, lip + 7.4, 100]);
  }
}

/* ---------- preview extras ---------- */
module preview_case() {
  color(WHITE) translate([0, lip, 0]) rrect(case_w, case_w, case_r, case_d);
  translate([0, lip - 0.05, 0]) {
    color(SCREEN) rrect(40, 40, 3, 0.1);
    color(CYAN) for (x = [-9.5, 9.5]) translate([x, -0.06, 4]) rotate([90, 0, 0]) cylinder(d = 10, h = 0.1);
    color(CYAN) translate([0, -0.06, 0]) rotate([90, 0, 0]) linear_extrude(0.1) intersection() {
      difference() { circle(r = 8.5); circle(r = 6.9); }
      translate([-6.2, -12]) square([12.4, 8]);
    }
  }
}

/* ---------- output (each part placed flat for printing) ---------- */
if (part == "head") rotate([90, 0, 0]) head();                                // face down on the plate
else if (part == "back") translate([0, 0, plate_t]) rotate([-90, 0, 0]) back_plate();  // flat, outside down
else if (part == "antenna") translate([0, 0, socket_h]) antenna();
else if (part == "fit_test") rotate([90, 0, 0]) fit_test();
else if (part == "headonly") { color(YELLOW) head(); preview_case(); }  // preview: back open
else {
  color(YELLOW) head();
  color(YELLOW) translate([0, D, 0]) back_plate();
  color(CLEAR, 0.85) translate([0, led_y, head_w / 2]) antenna();
  preview_case();
}
