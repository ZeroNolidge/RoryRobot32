// Rory the Robot - body (v1), under the v3 head (rory_head.scad)
//
// Inside, front to back:
//   chest wall (camera window)  ->  camera board (Waveshare ESP32-S3-CAM-OV3660,
//   37 x 37 mm, camera folded onto its front, facing out through the chest)
//   ->  board holder plate  ->  battery, lying sideways (up to 40 wide x 10 thick x 30 tall, e.g. 102540)
//   ->  back panel with Rory's two USB-C charging ports.
// Underneath the battery: two USB-C extension cables (male ends in the head and
// the camera board, female ends sticking out through the back panel).
// On top: a socket that the head's neck collar slides down into (no visible neck),
// locked by 2 short screws from inside the body up into the head's bottom.
//
// Parts (pick one, render with F6, export STL):
//   part = "shell"    the body                         almond yellow
//   part = "holder"   plate behind the camera board    any color (hidden)
//   part = "back"     back panel with the 2 USB ports  almond yellow
//   part = "feet"     her two big feet, glued on underneath  tan
//   part = "panel"    chest panel with two buttons           pale blue (paint the buttons teal and yellow)
//   part = "hands"    her two hands, glued onto her arms     tan (or brown)
//   part = "front_test"  just the chest + board pins (~30 min): check the camera lines up
//   part = "all"      everything together, with the head
//
// Hardware: 4 x M3 x 12 mm screws (back panel), flush like the head's;
// 2 x M3 x 6 from inside up into the head (lock it on); superglue for the hands and feet.

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
usb_drop = 6.2;        // room under the board for its slim right-angle USB-C plug (NFHK, 3.5 mm thick)
bat = [41, 10.5, 25.5];     // battery space, lying sideways: width, thickness, height (fits a 102540)
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
y_back = y_bat + bat[1] + 2.3;             // 2.3 mm channel behind the battery for the head's flat USB cable             // back of the shell (back panel goes on here)
D = y_back + panel_t;                      // total depth

z_port = wall;                             // ports lie on the floor
z_board = wall + usb_drop;                 // board's bottom edge
z_bat = z_port + port[2] + 0.8;            // battery sits just above the port cradles
collar = [23.4, 3.3];                      // the head's neck collar slides into a socket this wide and deep
H = max(z_board + board, z_bat + bat[2]) + 0.6 + collar[1];  // shell height: the collar clears the board
z_top_in = H - wall;
zc = z_board + board / 2;                  // board center
z_lens = z_board + board - lens_down;
lens_x = 0;                                 // camera left/right of center (0 = centered)

// head placement: the head sits right on the body, its collar down in the socket
head_D = 35.5;                             // head depth incl. back plate
head_y = -(head_D - D) / 2;                // where the head's front is, in body coords
socket_y0 = head_y + 2.7;                  // socket front (the head's collar starts 3 mm behind its face)
head_lock = [16, 19.5];                     // 2 screws from inside go up into the head's bottom (x, y)
lock_boss_t = 0.8;                         // extra thickness under the top wall there
head_mic = [19.4, 21.6];                   // the head's bottom microphone pinholes (x, y)

// back panel screws: into bosses on the side walls, near the back
screws = [for (sx = [-1, 1], z = [z_bat + bat[2] + 3.8, z_top_in - 3.5]) [sx * (in_w / 2 - 1.9), z]];  // above the battery
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
      // arms, molded onto her sides (nothing to pull off)
      for (sx = [-1, 1]) arm(sx);
    }
    // inside, open at the back
    translate([0, wall, H / 2]) rrect_xz(in_w, H - 2 * wall, 1.5, y_back);
    // socket for the head's neck collar (open into the body: cables and LED wires come down through it)
    rbox(-collar[0] / 2, collar[0] / 2, socket_y0, y_back + 1, H - collar[1], H + 1, 2.2);
    // the 2 head-lock screws go up through here
    for (sx = [-1, 1]) translate([sx * head_lock[0], head_y + head_lock[1], H - wall - lock_boss_t - 1]) cylinder(d = screw_clear, h = wall + lock_boss_t + 2);
    // little grooves so the head's bottom microphone holes aren't blocked
    for (sx = [-1, 1]) translate([sx > 0 ? head_mic[0] - 3 : -W / 2 - 1, head_y + head_mic[1] - 1.5, H - 1]) cube([W / 2 - head_mic[0] + 4, 3, 2]);
    arm_holes();
    // (no recess for the chest panel: the chest prints face-down, so it stays flat.
    //  The panel glues on flat, lined up by its camera hole; paint a gold dot above it.)
    // camera window in the chest, softly chamfered
    translate([lens_x, -1, z_lens]) rotate([-90, 0, 0]) cylinder(d = 7, h = wall + 2);
    translate([lens_x, -0.01, z_lens]) rotate([-90, 0, 0]) cylinder(d1 = 9, d2 = 7, h = 1);
    // pinholes for the camera board's PWR (right) and BOOT (left) buttons - press with a pen
    for (x = [pwr_x, -pwr_x]) translate([x, -1, z_board + pwr_up]) rotate([-90, 0, 0]) cylinder(d = 3.2, h = wall + 2);
  }
  // a little extra thickness under the top wall around the head-lock screws
  for (sx = [-1, 1]) difference() {
    translate([sx * head_lock[0], head_y + head_lock[1], H - wall - lock_boss_t]) cylinder(d = 7, h = lock_boss_t + 0.01);
    translate([sx * head_lock[0], head_y + head_lock[1], H - wall - lock_boss_t - 1]) cylinder(d = screw_clear, h = lock_boss_t + 2);
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
  feet_pegs();
}

// Arms and feet, shaped like Rory in the app: chunky arms hanging at her
// sides with a bolt at each shoulder, tan hands, and big tan feet that stick
// out at the front. The arms are part of the body (nothing to pull off); the
// hands and feet print separately in tan and glue on.
arm_w = 11;  arm_out = 7;        // arm width (front to back) and how far it stands out (outer edges line up with the head)
hand_h = 12;                     // hand height (the tan part at the bottom of each arm)
hand_z = 2;                      // bottom of the hands
arm_z0 = hand_z + hand_h;        // where the yellow arm starts
arm_y = D / 2 - 2;               // a little forward of center, so they show from the front
peg = [3.5, 2.5];                // glue pegs: diameter, length
module arm_shape(z0, z1, r) {    // rounded slab in y-z, extruded outward along +x from x=0
  hull() for (y = [-arm_w / 2 + r, arm_w / 2 - r], z = [z0 + r, z1 - r]) translate([0, y, z]) rotate([0, 90, 0]) cylinder(r = r, h = arm_out);
}
module arm(sx) {
  mirror([sx < 0 ? 1 : 0, 0, 0]) translate([W / 2 - 0.5, arm_y, 0]) {
    arm_shape(arm_z0, H - 3, 3.5);
    translate([0.5, 0, 0]) arm_shape(arm_z0, H - 3, 3.5);
    // shoulder bolt (paint it gold)
    translate([arm_out + 0.5, 0, H - 9]) scale([0.45, 1, 1]) sphere(d = 6);
  }
}
module arm_holes() {             // hole in the bottom of each arm for the hand's peg
  for (sx = [-1, 1]) translate([sx * (W / 2 + arm_out / 2), arm_y, arm_z0 - 0.01]) cylinder(d = peg[0] + 0.3, h = peg[1] + 0.3);
}
// one hand: a rounded tan block with a peg on top (prints lying on its flat side)
module hand() {
  difference() {
    hull() {
      for (y = [-arm_w / 2 + 3.5, arm_w / 2 - 3.5]) translate([0, y, hand_h - 3.5]) rotate([0, 90, 0]) cylinder(r = 3.5, h = arm_out + 0.5, center = true);
      for (y = [-arm_w / 2 + 4.5, arm_w / 2 - 4.5]) translate([0, y, 4.5]) scale([0.62, 1, 1]) sphere(r = 4.5);
    }
  }
  translate([0, 0, hand_h - 0.01]) cylinder(d = peg[0], h = peg[1]);
}
module hands_placed() {
  for (sx = [-1, 1]) translate([sx * (W / 2 + arm_out / 2), arm_y, hand_z]) hand();
}

// big oval feet that stick out at the front (tan); glue onto pegs under the body
foot = [21, 32, 11];             // width, length, height
foot_x = 11.3;  foot_y = D / 2 - 8;     // centers (forward, so the toes stick out like in the app)
module foot() {                  // flat top at z = 0 (glues to the body), FLAT bottom so she stands steadily
  r = 3;                         // rounded edge around the sides
  difference() {
    hull() {
      translate([0, 0, -0.5]) scale([foot[0] / 2, foot[1] / 2, 0.5]) cylinder(r = 1, h = 1, $fn = 64);
      // soft bulge around the middle, like the app
      translate([0, 0, -foot[2] * 0.55]) scale([foot[0] / 2 + 0.6, foot[1] / 2 + 0.6, 1]) cylinder(r = 1, h = 0.01, $fn = 64);
      // flat sole, a bit smaller so the edge looks rounded
      translate([0, 0, -foot[2]]) scale([foot[0] / 2 - r * 0.6, foot[1] / 2 - r * 0.6, 1]) cylinder(r = 1, h = 0.01, $fn = 64);
    }
    for (y = [-7, 7]) translate([0, y, -peg[1] - 0.3]) cylinder(d = peg[0] + 0.3, h = peg[1] + 1);  // peg holes
  }
}
module feet_placed() { for (sx = [-1, 1]) translate([sx * foot_x, foot_y, 0]) foot(); }
module feet_pegs() {   // on the body's underside
  for (sx = [-1, 1], y = [-7, 7]) translate([sx * foot_x, foot_y + y, -peg[1]]) cylinder(d = peg[0], h = peg[1] + 0.01);
}

// Rory's chest panel, like the app: pale blue, with a teal and a yellow button
// (paint them, or print the bumps' tops in color) and a little slot. Her camera
// looks out between the two buttons. Glue it into the shallow recess on her chest.
panel = [30, 22, 1.6];            // width, height, thickness
panel_z = z_lens - 5;             // panel center height
module chest_panel() {            // back face at y = 0, front face at y = -panel[2]
  difference() {
    union() {
      translate([0, -panel[2], 0]) rrect_xz(panel[0], panel[1], 3, panel[2]);
      intersection() {  // teal and yellow buttons (only on the front)
        for (x = [-8, 8]) translate([x, -panel[2], 5]) scale([1, 0.6, 1]) sphere(d = 6);
        translate([-50, -10, -50]) cube([100, 10, 100]);
      }
    }
    translate([0, -5, 5]) rotate([-90, 0, 0]) cylinder(d = 7.4, h = 10);                 // camera window
    translate([0, -panel[2] - 1, -5]) hull() for (x = [-6, 6]) translate([x, 0, 0]) rotate([-90, 0, 0]) cylinder(d = 2.4, h = 1.6);  // slot (0.6 deep)
  }
}
module chest_panel_placed() { translate([lens_x, 0, panel_z]) chest_panel(); }

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
      // pads that keep the battery from rattling (on the right: the head's USB cable runs down the left)
      for (x = [3, 11]) translate([x - 3, -(y_back - (y_bat + bat[1])) + 0.2, z_bat + bat[2] / 2 - 10]) cube([6, y_back - (y_bat + bat[1]) - 0.2 + 0.01, 20]);
    }
    // the two USB-C ports pass through
    for (sx = [-1, 1]) translate([sx * port_x, -2, z_port + port[2] / 2]) rotate([-90, 0, 0])
      linear_extrude(panel_t + 4) offset(0.8) square([port[0] - 1.6, port[2] - 1.6], center = true);
    // no lip where the head's collar comes down
    translate([-collar[0] / 2 - 0.5, -2, H - collar[1] - 0.5]) cube([collar[0] + 1, 2.01, collar[1] + 1]);
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
else if (part == "panel") rotate([-90, 0, 0]) chest_panel();  // pale blue: flat back down, buttons up
else if (part == "feet") for (sx = [-1, 1]) translate([sx * (foot[0] / 2 + 3), 0, 0]) rotate([180, 0, 0]) foot();  // tan: flat side down
else if (part == "hands") for (i = [0, 1]) translate([0, i * (arm_w + 6), (arm_out + 0.5) / 2]) rotate([0, 90, 0]) hand();  // tan: lying on a flat side
else if (part == "front_test") rotate([90, 0, 0]) intersection() {  // chest down
  shell();
  translate([-W / 2, -1, 0]) cube([W, y_pcb + board_t + 2.5, 100]);  // chest only: no arms or feet pegs
}
else if (part == "open") { color(YELLOW) shell(); color(GREY) holder_placed(); preview_inside(); }
else {
  color(YELLOW) shell();
  color(YELLOW) translate([0, y_back, 0]) back_panel();
  color(TAN) hands_placed();
  color(TAN) feet_placed();
  color("#E4F1F9") chest_panel_placed();
  // the head on top
  translate([0, head_y, H + 26.8]) { color(YELLOW) head(); color(YELLOW) translate([0, 31.5, 0]) back_plate(); color("#CFEFFF") translate([0, 27.4, 26.8]) antenna(); preview_case(); }
}
