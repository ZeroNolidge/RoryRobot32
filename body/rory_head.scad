// Rory the Robot - head (v3), for the Waveshare ESP32-S3-Touch-LCD-1.54
//
// The board stays in its own white 46 x 46 x 22.5 mm case. The case slides
// into the head from the back, the screen becomes Rory's face, and a solid
// back plate screws on with 4 flush screws in the corners. A thin space behind
// the case holds the antenna LED module (standing upright, LED legs bent so
// the LED points up into the clear antenna). The open neck underneath gives
// the USB-C port room for a cable (straight or flat 90-degree) and lets the LED
// wires go down into the body.
//
// v3.1: no visible neck - a short collar slides down into a socket in the body,
//       and 2 screws from inside the body go up into the head's bottom.
//       (The collar's front moved back a little so the body's front wall stays solid.)
// v3 (after the first test print):
//  - nothing sticks into the case's path any more (screws are in the corners)
//  - much shallower: 7 mm behind the case instead of 16
//  - the neck opening sits right under the USB-C port
//  - big side windows: the speaker grille and microphone are on the case's sides
//
// Parts (pick one, render with F6, export STL):
//   part = "head"      the head shell                      almond yellow
//   part = "back"      solid back plate, flush screws      almond yellow
//   part = "antenna"   antenna: stalk + ball               clear
//   part = "fit_test"  the front 9 mm only, ~20 min - checks the case fits
//   part = "all"       everything together, for looking at

part = "all";

/* ---------- the Waveshare case (mm) ---------- */
case_w = 46;     // width and height
case_d = 22.5;   // depth, front to back
case_r = 5.8;    // corner radius (front view)
// Openings - measured from the case FRONT (the screen side):
btn_x = [-11, 0, 11]; // the three buttons on top (measured from photos)...
btn_y = 11.8;         // ...their centers, from the case front
btn_d = 8.5;          // hole size (the buttons are ~6 mm)
bottom_mic = [19.4, 20];  // pinhole mic on the bottom: x from center (either side), y from the front
usb_y = [2, 20.5];    // USB-C (and microSD slot) on the bottom: the neck is open over all of it
side_win = [17, 34];  // side windows: depth x height (speaker grille + microphone)

/* ---------- Rory's head ---------- */
clr   = 0.4;   // clearance around the case (0.6 if tight, 0.25 if loose)
wall  = 3.4;   // side walls
head_r = 4;    // outside corner radius: a bit squarer, which leaves room for the corner screws
lip   = 1.6;   // frame in front of the screen
win   = 40.6;  // face window (the black glass is ~40 mm)
win_r = 3;
back_space = 7;  // behind the case: LED module standing upright + wires
plate_t = 4;     // back plate
screw_pocket = 2.9;  // the screw heads sink this deep (M3 socket heads are 3 mm tall)
holder_xz = 16;      // the back plate's 4 holding posts, near the case's corners
holder_gap = 0.3;    // leaves the posts just short of the case (no rattling, no squeezing)

head_w = case_w + 2 * (clr + wall);   // 53.6 mm
pocket_w = case_w + 2 * clr;
pocket_r = case_r + clr;
case_back = lip + case_d + clr;       // where the case's back sits
D = case_back + back_space;           // head depth; the back plate goes on behind

// corner screws: in the solid corners, halfway between the pocket and the outside
diag_in = (pocket_w / 2 - pocket_r) * sqrt(2) + pocket_r;
diag_out = (head_w / 2 - head_r) * sqrt(2) + head_r;
screw_xz = (diag_in + diag_out) / 2 / sqrt(2);
screws = [for (sx = [-1, 1], sz = [-1, 1]) [sx * screw_xz, sz * screw_xz]];
pilot = 2.5;  screw_clear = 3.4;  screw_len = 12;

// antenna / LED
led_y = case_back + 2.9;           // as far forward as the LED rim allows, behind the case
led_hole = 5.1;                     // 5 mm LED, press fit from inside (its rim stops it)
socket_d = 8.0;  socket_h = 1.6;    // the antenna's peg sits in this (stays clear of the back edge)

// neck (open underneath: USB-C plug, microSD, LED wires)
neck_in_w = 18;                     // fits a USB-C plug, straight or flat 90-degree
neck_y = [5.4, D];                  // from just in front of the angled USB-C plug to the back
neck_wall = 2.4;  neck_h = 3;   // short collar: it sits down in the body's socket
lock_x = 16;  lock_y = 19.5;  lock_depth = 3.0;  // 2 screws come up from inside the body into the head's bottom here

$fn = 64;
YELLOW = "#E9C46A"; CLEAR = "#CFEFFF"; SCREEN = "#263238"; CYAN = "#52DAE8"; WHITE = "#F5F5F5";

/* ---------- helpers ---------- */
// rounded rectangle in the x-z plane, from y=0 to y=t
module rrect(w, h, r, t) { translate([0, t, 0]) rotate([90, 0, 0]) linear_extrude(t) offset(r) square([w - 2 * r, h - 2 * r], center = true); }
// rounded rectangle in the x-y plane (for the neck)
module rbox2d(x0, x1, y0, y1, r) { translate([(x0 + x1) / 2, (y0 + y1) / 2]) offset(r) square([x1 - x0 - 2 * r, y1 - y0 - 2 * r], center = true); }
// stadium (rounded slot) in the y-z plane, for the side windows
module side_slot(dy, dz, len) { rotate([0, 90, 0]) linear_extrude(len, center = true) offset(dy / 2 - 0.01) square([dz - dy, 0.02], center = true); }

/* ---------- head ---------- */
module head() {
  difference() {
    union() {
      rrect(head_w, head_w, head_r, D);
      // neck collar under the head (it will sit in the body)
      translate([0, 0, -head_w / 2 - neck_h + 0.01]) linear_extrude(neck_h)
        rbox2d(-neck_in_w / 2 - neck_wall, neck_in_w / 2 + neck_wall, neck_y[0] - neck_wall, D, 2);
    }
    // the case pocket, straight through from the back - nothing in the way
    translate([0, lip, 0]) rrect(pocket_w, pocket_w, pocket_r, D);
    // face window with a soft chamfer
    translate([0, -1, 0]) rrect(win, win, win_r, lip + 2);
    translate([0, -0.01, 0]) hull() { rrect(win + 1.6, win + 1.6, win_r + 0.8, 0.01); translate([0, 0.8, 0]) rrect(win, win, win_r, 0.01); }
    // big side windows (speaker grille and microphone on one side, the other side to match)
    translate([0, lip + case_d / 2, 0]) side_slot(side_win[0], side_win[1], head_w + 2);
    // the three buttons on top: a round hole for each
    for (x = btn_x) translate([x, lip + btn_y, head_w / 2 - wall - 1]) cylinder(d = btn_d, h = wall + 2);
    // microphone pinhole on the bottom (one corner; a hole on both sides so either way round works)
    for (sx = [-1, 1]) translate([sx * bottom_mic[0], lip + bottom_mic[1], -head_w / 2 - 1]) cylinder(d = 3, h = wall + 2);
    // LED hole + antenna socket on top
    translate([0, led_y, head_w / 2 - wall - 1]) cylinder(d = led_hole, h = wall + 2);
    translate([0, led_y, head_w / 2 - socket_h]) cylinder(d = socket_d, h = socket_h + 1);
    // neck: open from just in front of the USB-C port all the way back
    translate([0, 0, -head_w / 2 - neck_h - 1]) linear_extrude(wall + neck_h + 2)
      rbox2d(-neck_in_w / 2, neck_in_w / 2, neck_y[0], D + 1, 2);
    // pilot holes underneath: 2 screws from inside the body lock the head on
    for (sx = [-1, 1]) translate([sx * lock_x, lock_y, -head_w / 2 - 1]) cylinder(d = pilot, h = lock_depth + 1);
    // corner screw holes for the back plate
    for (s = screws) translate([s[0], D - screw_len, s[1]]) rotate([-90, 0, 0]) cylinder(d = pilot, h = screw_len + 1);
  }
}

/* ---------- back plate (solid; screw heads sit flush) ---------- */
module back_plate() {
  difference() {
    union() {
      rrect(head_w, head_w, head_r, plate_t);
      // a lip that fits just inside the head so the plate lines up by itself
      translate([0, -1.2, 0]) difference() {
        rrect(pocket_w - 0.6, pocket_w - 0.6, pocket_r - 0.3, 1.2);
        rrect(pocket_w - 4, pocket_w - 4, pocket_r - 1.5, 1.2);
        // gap at the bottom so the wires can go down the neck
        translate([-neck_in_w / 2, -1, -head_w / 2]) cube([neck_in_w, 3, 6]);
      }
      // posts that press the case gently against the face (clear of the LED
      // module in the top middle and the wires in the bottom middle)
      for (x = [-1, 1], z = [-1, 1]) translate([x * holder_xz, -(back_space - holder_gap), z * holder_xz])
        rotate([-90, 0, 0]) cylinder(d = 6, h = back_space - holder_gap + 0.01);
    }
    for (s = screws) translate([s[0], 0, s[1]]) rotate([-90, 0, 0]) {
      translate([0, 0, -1]) cylinder(d = screw_clear, h = plate_t + 2);
      translate([0, 0, plate_t - screw_pocket]) cylinder(d = 6.4, h = screw_pocket + 1);
    }
  }
}

/* ---------- antenna (clear) ---------- */
// Prints standing up. The LED's dome slides into the bore; the clear plastic
// above it carries the light up into the ball.
ant_h = 16;  ball_d = 13;
module antenna() {
  difference() {
    union() {
      translate([0, 0, -socket_h]) cylinder(d = socket_d - 0.25, h = socket_h + 0.01);  // peg (press fit)
      cylinder(d1 = 10, d2 = 7, h = ant_h);
      hull() {  // ball, with a cone underneath so it prints without supports
        translate([0, 0, ant_h + ball_d / 2 - 1.5]) sphere(d = ball_d);
        translate([0, 0, ant_h - 2]) cylinder(d = 7, h = 0.1);
      }
    }
    translate([0, 0, -socket_h - 1]) cylinder(d = 5.6, h = socket_h + 1 + 6.5);  // room for the LED dome
  }
}

/* ---------- fit test: the front 9 mm of the head ---------- */
module fit_test() {
  intersection() { head(); translate([-50, -1, -head_w / 2]) cube([100, lip + 7.4, 100]); }
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
if (part == "head") rotate([90, 0, 0]) head();                                  // face down on the plate
else if (part == "back") translate([0, 0, plate_t]) rotate([-90, 0, 0]) back_plate();  // outside down
else if (part == "antenna") translate([0, 0, socket_h]) antenna();
else if (part == "fit_test") rotate([90, 0, 0]) fit_test();
else if (part == "headonly") { color(YELLOW) head(); preview_case(); }          // preview: back open
else {
  color(YELLOW) head();
  color(YELLOW) translate([0, D, 0]) back_plate();
  color(CLEAR, 0.85) translate([0, led_y, head_w / 2]) antenna();
  preview_case();
}
