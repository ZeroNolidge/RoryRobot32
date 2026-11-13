// Rory the Robot - printable body for the Waveshare ESP32-S3-Touch-LCD-1.54
//
// The board comes in its own white 46 x 46 x 22.5 mm case. That whole case
// slides into Rory's head from the back, the screen becomes her face, and a
// back cover screws on. About 10 cm tall.
//
// Parts (export each as an STL - see body/README.md):
//   part = "body"   head + body + feet + antenna, one piece  (peach)
//   part = "cover"  back cover with speaker holes            (peach)
//   part = "arm"    print 2                                  (peach or tan)
//   part = "all"    everything assembled, for looking at
//
// Screws: 4 x M3 x 12 mm pan-head self-tapping screws (2 for the arms,
// 2 for the cover). The arm screws double as Rory's shoulder bolts.

part = "all";

/* ---------- the Waveshare case (from the outline drawing, mm) ---------- */
case_w = 46;      // width and height of the white case
case_d = 22.5;    // depth front to back
case_r = 5.8;     // corner radius (front view)
// Openings in the case - check these against the real board and adjust:
btn_y   = [4, 16.4];   // button area, measured from the case FRONT (buttons sit ~9.6 mm from one face)
usb_y   = [3, 19.5];   // USB-C area on the bottom, from the case front
ear_d   = 15;          // side windows (microSD slot + side hole)

/* ---------- Rory ---------- */
clr  = 0.4;    // clearance around the case
wall = 3.2;    // shell wall thickness
lip  = 1.6;    // front frame thickness in front of the case
win  = 40.6;   // front window (the case's black glass is ~40 mm)
win_r = 3;

head_w = case_w + 2 * (clr + wall);   // 53.2
head_r = case_r + clr + wall;
D      = lip + case_d + clr;          // depth of head & body; the back cover sits on this plane

body_w = 44;  body_h = 26;  body_r = 6;  body_inset = 1.5;
foot_h = 6;
z_body = foot_h;
z_head = foot_h + body_h;
head_cz = z_head + head_w / 2;
z_top  = z_head + head_w;

arm_z  = z_body + body_h - 8;         // shoulder pivot height
arm_y  = (body_inset + D) / 2;
cover_screw = [[-10.5, z_body + body_h - 10], [10.5, z_body + body_h - 10]];
hook_x = [5, 16];                     // the cover's two hooks over the head top
pilot  = 2.5;                         // pilot hole for M3 self-tapping screws
screw_clear = 3.4;

$fn = 64;

/* ---------- colors (Rory in the app) ---------- */
PEACH = "#FFCC80"; TAN = "#CEA466"; SCREEN = "#263238"; CYAN = "#52DAE8";
PALE = "#E4F1F9"; TEAL = "#1E9186"; YELLOW = "#F9E53A"; BALL = "#B6ECF2"; RED = "#C62828";

/* ---------- helpers ---------- */
// rounded rectangle in the x-z plane, centered on x/z, from y=0 to y=t
module rrect(w, h, r, t) {
  translate([0, t, 0]) rotate([90, 0, 0])
    linear_extrude(t) offset(r) square([w - 2 * r, h - 2 * r], center = true);
}
// same, but extruded from a 2D shape given as children
module xz(t) { translate([0, t, 0]) rotate([90, 0, 0]) linear_extrude(t) children(); }

module body2d() {  // rounded bottom corners, square top where it meets the head
  hull() {
    for (s = [-1, 1]) translate([s * (body_w / 2 - body_r), -body_h / 2 + body_r]) circle(body_r);
    translate([-body_w / 2, body_h / 2 - 1]) square([body_w, 1 + 3]);  // overlaps into the head
  }
}

/* ---------- main piece ---------- */
module head_shell() {
  translate([0, 0, head_cz]) difference() {
    union() {
      rrect(head_w, head_w, head_r, D);
      // ear rings
      for (s = [-1, 1]) translate([s * head_w / 2, lip + case_d / 2, 0]) rotate([0, 90, 0])
        cylinder(d = ear_d + 6, h = 1.6, center = true);
    }
    // the case pocket, open at the back
    translate([0, lip, 0]) rrect(case_w + 2 * clr, case_w + 2 * clr, case_r + clr, D);
    // face window, with a soft chamfer
    translate([0, -1, 0]) rrect(win, win, win_r, lip + 2);
    translate([0, -0.01, 0]) hull() {
      rrect(win + 1.6, win + 1.6, win_r + 0.8, 0.01);
      translate([0, 0.8, 0]) rrect(win, win, win_r, 0.01);
    }
    // ears: side windows
    for (s = [-1, 1]) translate([s * head_w / 2, lip + case_d / 2, 0]) rotate([0, 90, 0])
      cylinder(d = ear_d, h = 2 * wall + 6, center = true);
  }
}

module antenna() {
  // flat-backed so it prints lying on its back without much support
  hull() for (y = [18.5, D - 2.5]) translate([0, y, z_top - 1]) cylinder(d = 5, h = 8);
  translate([0, D - 5.5, z_top + 11]) sphere(d = 11);
}

module feet() {
  for (s = [-1, 1]) translate([s * 14.5, 10, 0]) hull() {
    scale([1, 1.75, 1]) cylinder(d = 14, h = 1);
    translate([0, 0, foot_h - 2]) scale([0.85, 1.6, 1]) cylinder(d = 14, h = 2 + 1);
  }
}

module chest_panel() {
  pz = z_body + body_h * 0.5;
  translate([0, body_inset, pz]) difference() {
    union() {
      translate([0, 0.01, 0]) mirror([0, 1, 0]) rrect(26, 14, 3, 0.8);
      for (x = [-5.5, 5.5]) translate([x, -0.8, 2.6]) scale([1, 0.5, 1]) sphere(d = 5);
    }
    // little slot like the app's chest panel
    translate([0, -0.8, -3.2]) hull() for (x = [-6, 6]) translate([x, 0, 0]) rotate([90, 0, 0])
      cylinder(d = 2, h = 1.2, center = true);
  }
}

module main_piece() {
  difference() {
    union() {
      head_shell();
      translate([0, body_inset, z_body + body_h / 2]) xz(D - body_inset) body2d();
      feet();
      antenna();
      chest_panel();
    }
    // buttons on top: one long window over all three
    translate([0, 0, z_top - wall - 1]) hull() for (x = [-16, 16])
      translate([x, lip + (btn_y[0] + btn_y[1]) / 2, 0])
        scale([1, (btn_y[1] - btn_y[0]) / 8, 1]) cylinder(d = 8, h = wall + 2);
    // USB-C tunnel: plug the cable in from underneath, between the feet
    translate([-7, lip + usb_y[0], -1]) cube([14, usb_y[1] - usb_y[0], z_head + wall + 1]);
    // grooves for the cover's hooks
    for (s = [-1, 1]) translate([s > 0 ? hook_x[0] - 0.3 : -hook_x[1] - 0.3, D - 5.3, z_top - 1.6])
      cube([hook_x[1] - hook_x[0] + 0.6, 2.2, 3]);
    // shoulder screws
    for (s = [-1, 1]) translate([s * body_w / 2, arm_y, arm_z]) rotate([0, 90, 0])
      cylinder(d = pilot, h = 20, center = true);
    // cover screws
    for (p = cover_screw) translate([p[0], D - 10, p[1]]) rotate([-90, 0, 0]) cylinder(d = pilot, h = 11);
  }
}

/* ---------- back cover ---------- */
module cover2d() {
  translate([0, head_cz]) offset(head_r) square([head_w - 2 * head_r, head_w - 2 * head_r], center = true);
  translate([-15, cover_screw[0][1] - 6]) square([30, z_head + 6 - (cover_screw[0][1] - 6)]);
  // plate reaches up to join the hooks
  for (s = [-1, 1]) translate([s > 0 ? hook_x[0] : -hook_x[1], z_top - 6]) square([hook_x[1] - hook_x[0], 8]);
}

module cover() {
  t = 2.4;
  difference() {
    union() {
      translate([0, D, 0]) xz(t) cover2d();
      // hooks that slip over the top of the head into the grooves
      for (s = [-1, 1]) translate([s > 0 ? hook_x[0] : -hook_x[1], 0, 0]) {
        translate([0, D - 5, z_top]) cube([hook_x[1] - hook_x[0], 5 + t, 2]);
        translate([0, D - 5, z_top - 1.3]) cube([hook_x[1] - hook_x[0], 1.6, 1.4]);
      }
      // two pads that hold the case snug against the face
      for (x = [-15, 15]) translate([x - 3, D - 0.3, head_cz - 3]) cube([6, 0.4, 6]);
    }
    // speaker / microphone holes
    for (x = [-12 : 4 : 12], z = [-12 : 4 : 12])
      if (x * x + z * z <= 13 * 13) translate([x, D - 1, head_cz + z]) rotate([-90, 0, 0]) cylinder(d = 2.2, h = 5, $fn = 16);
    // screw holes
    for (p = cover_screw) translate([p[0], D - 1, p[1]]) rotate([-90, 0, 0]) cylinder(d = screw_clear, h = 5);
    // room for the antenna
    translate([-3.5, D - 6, z_top - 2]) cube([7, 10, 6]);
  }
}

/* ---------- arm (modelled flat, for printing) ---------- */
module arm2d() {
  circle(d = 11);
  hull() { circle(d = 8); translate([0, -14]) circle(d = 8); }
  translate([0, -16]) circle(d = 12);
}
module arm() {
  difference() {
    union() {
      linear_extrude(5) arm2d();
      translate([0, 0, -0.8]) cylinder(d = 8, h = 0.8);  // spacer so the arm swings freely
    }
    translate([0, 0, -2]) cylinder(d = screw_clear, h = 10);
  }
}
module arm_placed(s) {  // s = -1 left, +1 right; arm hangs down beside the body
  mirror([s < 0 ? 1 : 0, 0, 0]) translate([body_w / 2 + 0.8, arm_y, arm_z]) rotate([90, 0, 90]) arm();
}

/* ---------- preview extras (not printed) ---------- */
module preview_face() {  // Rory's resting face, like the app
  translate([0, lip + 0.05, head_cz]) {
    color(SCREEN) rrect(40, 40, 3, 0.4);
    translate([0, -0.12, 0]) xz(0.1) {
      color("white") for (x = [-9.5, 9.5]) translate([x, 4]) circle(d = 11.6);
    }
    translate([0, -0.22, 0]) xz(0.1) {
      color(CYAN) for (x = [-9.5, 9.5]) translate([x, 4]) circle(d = 10);
    }
    translate([0, -0.32, 0]) xz(0.1) {
      color("white") for (x = [-9.5, 9.5]) translate([x + 2.4, 6.4]) circle(d = 3);
      color(CYAN) translate([0, 0]) intersection() {   // smile
        difference() { circle(r = 8.5); circle(r = 6.9); }
        translate([-6.2, -12]) square([12.4, 8]);
      }
    }
  }
}
module preview_paint() {  // suggested paint colors on the one-piece body
  pz = z_body + body_h * 0.5;
  color(PALE) translate([0, body_inset - 0.86, pz]) xz(0.05) difference() {
    offset(2.8) square([25.4 - 5.6, 13.4 - 5.6], center = true);
    hull() for (x = [-6, 6]) translate([x, -3.2]) circle(d = 2);
  }
  color("#8A9BA5") translate([0, body_inset - 0.82, pz]) xz(0.05) hull() for (x = [-6, 6]) translate([x, -3.2]) circle(d = 2);
  color(TEAL) translate([-5.5, body_inset - 0.8, pz + 2.6]) scale([1, 0.52, 1]) sphere(d = 5.1);
  color(YELLOW) translate([5.5, body_inset - 0.8, pz + 2.6]) scale([1, 0.52, 1]) sphere(d = 5.1);
  color(BALL) translate([0, D - 5.5, z_top + 11]) sphere(d = 11.2);
  color(TAN) feet();
  color("#D9A93A") for (s = [-1, 1]) translate([s * (body_w / 2 + 6.6), arm_y, arm_z]) rotate([0, 90, 0])
    cylinder(d = 5.5, h = 1.6, center = true);  // screw heads = shoulder bolts
}

/* ---------- output ---------- */
if (part == "body") main_piece();
else if (part == "cover") rotate([-90, 0, 0]) translate([0, -D - 2.4, 0]) cover();  // printed flat
else if (part == "arm") arm();
else {
  color(PEACH) main_piece();
  color(PEACH) cover();
  color(TAN) for (s = [-1, 1]) arm_placed(s);
  preview_face();
  preview_paint();
}
