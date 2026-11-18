// Rory the Robot - body (v1), under the v3 head (rory_head.scad)
//
// Inside, front to back:
//   chest wall (camera window)  ->  camera board (Waveshare ESP32-S3-CAM-OV3660,
//   37 x 37 mm, camera folded onto its front, facing out through the chest)
//   ->  board holder plate  ->  battery (up to 10 x 30 x 41 mm, e.g. 102540)
//   ->  back panel with Rory's two USB-C charging ports.
// Underneath the battery: two USB-C extension cables (male ends in the head and
// the camera board, female ends sticking out through the back panel).
// On top: a neck plug that the head's neck collar slides onto.
//
// Parts (pick one, render with F6, export STL):
//   part = "shell"    the body                         almond yellow
//   part = "holder"   plate behind the camera board    any color (hidden)
//   part = "back"     back panel with the 2 USB ports  almond yellow
//   part = "feet"     two feet (print in tan, glue on) tan   (or leave them on the shell)
//   part = "front_test"  just the chest + board pins (~30 min): check the camera lines up
//   part = "all"      everything together, with the head
//
// Hardware: 4 x M3 x 12 mm screws (back panel), flush like the head's.

use <rory_head.scad>

part = "all";

/* ---------- what goes inside (mm) ---------- */
board = 37;            // camera board, square
board_hole = 32.6;     // mounting-hole spacing
board_pin = 2.0;       // locating pins that go into its holes
cam_stack = 7;         // from the board's front face to the front of the folded camera (microSD slot + camera)
board_t = 1.6;         // board thickness
back_parts = 6.5;      // tallest parts on the board's back (its white sockets and USB-C)
lens_down = 9;         // lens center below the board's top edge (centered left-right)
pwr_x = 11.2;          // PWR button: right of center (seen from the front)...
pwr_up = 1.6;          // ...and this far above the board's bottom edge
usb_drop = 9;          // room under the board for its right-angle USB-C plug
bat = [30.5, 10.5, 41.5];   // battery space: width, thickness, height (fits a 102540 or smaller)
port = [10.4, 19, 5.9];     // USB-C extension's female end: width, length, thickness (+0.4)
port_x = 13;                // the two ports sit this far left/right of center

/* ---------- body ---------- */
wall = 2.4;
in_w = 41;                         // inside width
W = in_w + 2 * wall;               // 45.8 outside
corner_r = 4;
plate_t = 1.2;                     // board holder
panel_t = 4;                       // back panel (flush screw pockets)
screw_pocket = 2.9;

y_cam = wall;                              // front of the folded camera (touches the chest wall)
y_pcb = y_cam + cam_stack;                 // board's front face
y_plate = y_pcb + board_t + back_parts;    // holder plate (front face)
y_bat = y_plate + plate_t;                 // battery front
y_back = y_bat + bat[1] + 0.8;             // back of the shell (back panel goes on here)
D = y_back + panel_t;                      // total depth

z_port = wall;                             // ports lie on the floor
z_board = wall + usb_drop;                 // board's bottom edge
z_bat = z_port + port[2] + 0.8;            // battery sits just above the port cradles
z_top_in = max(z_board + board, z_bat + bat[2]) + 0.6;
H = z_top_in + wall;                       // shell height
zc = z_board + board / 2;                  // board center
z_lens = z_board + board - lens_down;

// head placement: its neck collar sits on top of the body, centered front-to-back
head_D = 35.5;                             // head depth incl. back plate
head_y = -(head_D - D) / 2;                // where the head's front is, in body coords
// the neck plug goes inside the head's collar opening (18 mm wide, from 2.6 mm behind its face)
plug = [17.4, 5.5];                        // width, height
plug_y = [head_y + 3.0, min(y_back - 0.3, head_y + 31.0)];
plug_wall = 1.6;
// neck bolts: an M3 x 8 screw through each side of the head's collar into the plug
bolt_y = head_y + 28;                      // (same spot as in rory_head.scad)
bolt_z = 2.75;                             // above the body's top
bolt_boss = 4.6;                           // the plug is solid from here outward, around the bolts

// back panel screws: into bosses on the side walls, near the back
screws = [for (sx = [-1, 1], z = [z_bat + 8, z_top_in - 4]) [sx * (in_w / 2 - 1.9), z]];
boss_d = 6.5;  boss_len = 9;  pilot = 2.5;  screw_clear = 3.4;

$fn = 48;
YELLOW = "#E9C46A"; TAN = "#CEA466"; DARK = "#263238"; GREY = "#9AA5AB";

/* ---------- helpers ---------- */
module rrect_xz(w, h, r, t) { translate([0, t, 0]) rotate([90, 0, 0]) linear_extrude(t) offset(r) square([w - 2 * r, h - 2 * r], center = true); }
module rbox(x0, x1, y0, y1, z0, z1, r = 1) {  // box rounded in x-y
  translate([0, 0, z0]) linear_extrude(z1 - z0) translate([(x0 + x1) / 2, (y0 + y1) / 2])
    offset(r) square([x1 - x0 - 2 * r, y1 - y0 - 2 * r], center = true);
}

/* ---------- shell ---------- */
module shell() {
  difference() {
    union() {
      // outer box, rounded corners seen from the front
      translate([0, 0, H / 2]) rrect_xz(W, H, corner_r, y_back);
      // neck plug on top
      rbox(-plug[0] / 2, plug[0] / 2, plug_y[0], plug_y[1], H - 0.01, H + plug[1], 2);
      // arms, molded onto her sides (nothing to pull off)
      for (sx = [-1, 1]) arm(sx);
    }
    // inside, open at the back
    translate([0, wall, H / 2]) rrect_xz(in_w, H - 2 * wall, 1.5, y_back);
    // neck hollow: cables and LED wires go down from the head
    rbox(-plug[0] / 2 + plug_wall, plug[0] / 2 - plug_wall, plug_y[0] + plug_wall, plug_y[1] - plug_wall, H - wall - 1, H + plug[1] + 1, 1);
    // pilot holes for the neck bolts
    for (sx = [-1, 1]) translate([sx * (plug[0] / 2 + 1), bolt_y, H + bolt_z]) rotate([0, -sx * 90, 0]) cylinder(d = pilot, h = plug[0] / 2 - bolt_boss + 2);
    // camera window in the chest, softly chamfered
    translate([0, -1, z_lens]) rotate([-90, 0, 0]) cylinder(d = 7, h = wall + 2);
    translate([0, -0.01, z_lens]) rotate([-90, 0, 0]) cylinder(d1 = 9, d2 = 7, h = 1);
    // pinhole for the camera board's PWR button (press with a pen)
    translate([pwr_x, -1, z_board + pwr_up]) rotate([-90, 0, 0]) cylinder(d = 3.2, h = wall + 2);
  }
  // solid blocks inside the neck plug around the neck bolts
  for (sx = [-1, 1]) difference() {
    translate([sx > 0 ? bolt_boss : -plug[0] / 2 + plug_wall - 0.01, bolt_y - 3, H]) cube([plug[0] / 2 - plug_wall - bolt_boss + 0.01, 6, plug[1]]);
    translate([sx * (plug[0] / 2 + 1), bolt_y, H + bolt_z]) rotate([0, -sx * 90, 0]) cylinder(d = pilot, h = plug[0] / 2 - bolt_boss + 2);
  }
  // posts that hold the camera board, with pins into its corner holes
  for (sx = [-1, 1], sz = [-1, 1]) translate([sx * board_hole / 2, wall - 0.01, zc + sz * board_hole / 2]) rotate([-90, 0, 0]) {
    cylinder(d = 5, h = cam_stack + 0.01);
    cylinder(d = board_pin, h = cam_stack + board_t + 0.6);
  }
  // cradles that hold the two USB-C female ends (ports flush with the back panel)
  for (sx = [-1, 1]) translate([sx * port_x, 0, 0]) difference() {
    rbox(-port[0] / 2 - 1.2, port[0] / 2 + 1.2, D - port[1], y_back, wall - 0.01, z_port + port[2] * 0.7, 0.5);
    rbox(-port[0] / 2, port[0] / 2, D - port[1] - 1, y_back + 1, z_port, z_port + port[2] + 1, 0.3);
  }
  // screw bosses for the back panel
  for (s = screws) difference() {
    hull() {
      translate([s[0], y_back - boss_len, s[1]]) rotate([-90, 0, 0]) cylinder(d = boss_d, h = boss_len);
      translate([sign(s[0]) * (in_w / 2), y_back - boss_len, s[1] - boss_d / 2]) cube([0.01, boss_len, boss_d]);
    }
    translate([s[0], y_back - boss_len - 1, s[1]]) rotate([-90, 0, 0]) cylinder(d = pilot, h = boss_len + 2);
  }
  feet();
}

// a little arm on each side: shoulder near the top, hand down by her hip
module arm(sx) {
  t = 3.2;                       // how far it stands out from her side
  sy = D / 2;                    // shoulder
  sz = H - 10;
  hy = D / 2 - 3;                // hand
  hz = H - 34;
  k = sx > 0 ? -1 : 1;           // after the rotation below, 2D x becomes height (sign depends on the side)
  translate([sx * (W / 2 - 0.5), 0, 0]) rotate([0, sx * 90, 0]) linear_extrude(t + 0.5) {
    translate([k * sz, sy]) circle(d = 11);
    hull() { translate([k * sz, sy]) circle(d = 8); translate([k * hz, hy]) circle(d = 7.5); }
    translate([k * hz, hy]) circle(d = 11);
  }
}

module feet() {
  for (sx = [-1, 1]) translate([sx * 12.5, D / 2 - 1, 0.01]) mirror([0, 0, 1]) hull() {
    scale([1, 1.6, 1]) cylinder(d = 14, h = 0.5);
    translate([0, 0, 3]) scale([0.9, 1.5, 1]) cylinder(d = 14, h = 1);
  }
}

/* ---------- board holder (sits behind the camera board) ---------- */
// A flat plate that keeps the battery off the board's plugs; 4 little fingers
// press on the board's corners so it can't wobble off its pins.
module holder() {
  pw = in_w - 2 * 1.6;                        // narrower than the inside: room for wires at the sides
  pz0 = z_board + 0.3;
  pz1 = z_board + board - 0.3;
  difference() {
    translate([0, 0, 0]) rbox(-pw / 2, pw / 2, 0, pz1 - pz0, 0, plate_t, 2);
    // notch at the top right for the LED wires to reach the board's UART plug
    translate([pw / 2 - 6, pz1 - pz0 - 19, -1]) cube([7, 10, plate_t + 2]);
  }
  for (sx = [-1, 1], sz = [-1, 1]) translate([sx * 17, (zc + sz * 17) - pz0, plate_t - 0.01])
    cylinder(d = 3, h = back_parts + 0.01);
}
module holder_placed() {  // in body coordinates
  pz0 = z_board + 0.3;
  translate([0, y_plate + plate_t, pz0]) rotate([90, 0, 0]) holder();
}

/* ---------- back panel ---------- */
module back_panel() {
  difference() {
    union() {
      translate([0, 0, H / 2]) rrect_xz(W, H, corner_r, panel_t);
      // lip that fits inside the shell so the panel lines up by itself
      translate([0, -1.2, H / 2]) difference() {
        rrect_xz(in_w - 0.6, H - 2 * wall - 0.6, 1.2, 1.2);
        rrect_xz(in_w - 4, H - 2 * wall - 4, 0.5, 1.2);
      }
      // pads that keep the battery from rattling
      for (x = [-9, 9]) translate([x - 3, -(y_back - (y_bat + bat[1])) + 0.2, z_bat + bat[2] / 2 - 10]) cube([6, y_back - (y_bat + bat[1]) - 0.2 + 0.01, 20]);
    }
    // the two USB-C ports pass through
    for (sx = [-1, 1]) translate([sx * port_x, -2, z_port + port[2] / 2]) rotate([-90, 0, 0])
      linear_extrude(panel_t + 4) offset(0.8) square([port[0] - 1.6, port[2] - 1.6], center = true);
    // screw holes with pockets, so the heads sit flush
    for (s = screws) translate([s[0], 0, s[1]]) rotate([-90, 0, 0]) {
      translate([0, 0, -2]) cylinder(d = screw_clear, h = panel_t + 4);
      translate([0, 0, panel_t - screw_pocket]) cylinder(d = 6.4, h = screw_pocket + 1);
    }
    // clear the screw bosses with the lip
    for (s = screws) translate([s[0], -1.3, s[1]]) rotate([-90, 0, 0]) cylinder(d = boss_d + 1, h = 1.31);
  }
}

/* ---------- preview extras ---------- */
module preview_inside() {
  // camera board, camera, battery, USB ends
  color("#1B1B1B") translate([-board / 2, y_pcb, z_board]) cube([board, board_t, board]);
  color("#111") translate([-4.25, y_cam, z_lens - 4.25]) cube([8.5, cam_stack, 8.5]);
  color("#8899AA") translate([-bat[0] / 2 + 0.25, y_bat + 0.25, z_bat]) cube([bat[0] - 0.5, bat[1] - 0.5, bat[2] - 0.5]);
  color("#333") for (sx = [-1, 1]) translate([sx * port_x - port[0] / 2 + 0.2, D - port[1], z_port + 0.2]) cube([port[0] - 0.4, port[1], port[2] - 0.4]);
}

/* ---------- output ---------- */
if (part == "shell") rotate([90, 0, 0]) translate([0, 0, 0]) shell();          // chest down on the plate
else if (part == "holder") holder();
else if (part == "back") translate([0, 0, panel_t]) rotate([-90, 0, 0]) back_panel();  // outside down
else if (part == "feet") feet();
else if (part == "front_test") rotate([90, 0, 0]) intersection() {  // chest down
  shell();
  translate([-50, -1, -10]) cube([100, y_pcb + board_t + 2.5, 100]);
}
else if (part == "open") { color(YELLOW) shell(); color(GREY) holder_placed(); preview_inside(); }
else {
  color(YELLOW) shell();
  color(YELLOW) translate([0, y_back, 0]) back_panel();
  // the head on top
  translate([0, head_y, H + 6 + 26.8]) { color(YELLOW) head(); color(YELLOW) translate([0, 31.5, 0]) back_plate(); color("#CFEFFF") translate([0, 27.4, 26.8]) antenna(); preview_case(); }
}
