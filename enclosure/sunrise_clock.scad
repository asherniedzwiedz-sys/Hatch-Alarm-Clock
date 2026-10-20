// Sunrise clock enclosure in the shape of the Hatch sleep clocks: an arch with a thin rim, a
// fabric-covered front that glows above a soft "horizon", a dot-matrix clock behind the fabric
// and a dome knob on top. Fully parametric (OpenSCAD 2021.01+).
// Axes: X = width, Y = depth (front face at y = 0), Z = height (bottom at z = 0).
// One part:   openscad -o shell.stl -D 'part="shell"' sunrise_clock.scad
// Everything: python build.py   (exports STLs, checks manifold, bed fit and clearances)

/* [Part] */
part = "assembly";  // [assembly, front, exploded, shell, frame, diffuser, light_box, back_cover, speaker_lid, knob, intersect]
check = ["frame", "shell"];  // part = "intersect": two parts, in assembled position
explode = 40;

/* [Body] */
W = 184; H = 140;  // Hatch Restore-size arch (7.24 in wide)
D = 72;            // 10 mm deeper than Hatch's 62 to fit the rear-facing 57 mm speaker
wall = 2.4;
r_bottom = 10;     // bottom corner radius; the top is a half circle of radius W/2
edge = 1;          // chamfer on the front and back outside edges
clr = 0.5;         // clearance between mating parts
bed = 220;

/* [Front: rim, fabric frame, diffuser] */
fabric_t = 0.6;   // linen wrapped around the frame and glued on its back
rim_t = 1.6;      // front lip, holds the frame from the front
rim_in = 4.5;     // visible rim width
frame_t = 3.2;
win_inset = 10;   // light window, from the outside edge
horizon_z = 55;   // where the glow fades out
soft = 12;        // window + diffuser continue this far below the horizon for the fade
diff_t = 2; diff_lap = 3;
rib_d = 1.2;      // crush ribs reach rib_d/2 - clr = 0.1 mm into the diffuser: press fit

/* [Light box] */
led_gap = 20;  // diffuser back face -> LED face
plate_t = 2.5; box_wall = 2;
baffle_gap = 8;  // horizon baffle stops short of the diffuser so light spills into a soft edge
led_count = 40; led_per_m = 100;  // BTF-Lighting WS2812B 100 LED/m, 10 mm wide; 60/m fits only ~29 LEDs
strip_w = 10; channel_d = 1; row_pitch = 15;
box_posts = [[18, 59], [W - 18, 59]];

/* [Clock display] */
clock_size = [40.6, 20.3];  // HT16K33 16x8: two 0.8" 8x8 matrices
clock_z = 24;               // centre height
clock_sleeve = 3;  // shallow rim: the Adafruit FeatherWing PCB is wider than its LEDs

/* [Sleep radar] */
radar_size = [22, 16];  // HLK-LD2410C board: measure yours; antenna side faces the fabric
radar_xz = [44, 24];    // lower left, clear of the clock, LEDs and speaker magnet
radar_sleeve = 8;

/* [Speaker + electronics] */
spk_d = 51; grille_d = 44;  // Gikfun 2" 4 ohm 3 W (EK1725)
chamber_in = 64; chamber_wall = 2; chamber_depth = 36;
pcb_holes = [22, 60]; pcb_center = [142.5, 48]; pcb_standoff = 5;  // devkit board beside the chamber
usbc = [10, 4.2]; usbc_xz = [30, 12];
vent_n = 6; vent = [3, 20]; vent_z = 98;

/* [Top controls] */
ctrl_y = 40;  // behind the light box
enc_d = 7.5; btn_d = 12; btn_dx = 28;  // buttons either side of the knob, square to the curve
lux_d = 6; lux_y = 60;
knob_d = 21; knob_h = 7;  // low dome, like the original

/* [Fasteners + feet] */
back_t = 3;
insert_d = 4.0; insert_len = 5;  // M3 heat-set insert hole
screw_d = 3.4; head_d = 6.5;
frame_screws = [[13, 13], [W - 13, 13]];
back_bosses = [[11, 78], [W - 11, 78], [52, 6], [W - 52, 6]];
foot_d = 11; foot_depth = 1.2;

$fn = 64;
eps = 0.01;
embed = 1;  // unioned features sink this far into what they join (no near-coincident faces)

// ---- Derived ----
z0 = H - W / 2;                    // centre of the top half circle
frame_in = wall + clr + fabric_t;  // frame outline inset, fabric included
frame_y0 = rim_t + fabric_t;
frame_y1 = frame_y0 + frame_t;     // frame back = diffuser back
plate_y0 = frame_y1 + led_gap;
plate_y1 = plate_y0 + plate_t;
shell_y1 = D - back_t;
cav_r = W / 2 - win_inset;
pitch = 1000 / led_per_m;
row_zs = [for (z = [horizon_z + 10 : row_pitch : z0 + cav_r - strip_w]) z];
function row_cap(z) = let(h = z + strip_w / 2 - z0) max(0, floor(2 * (sqrt(max(cav_r * cav_r - h * h, 0)) - 7) / pitch));
function sum(v, i = 0) = i < len(v) ? v[i] + sum(v, i + 1) : 0;
caps = [for (z = row_zs) row_cap(z)];
base_n = [for (c = caps) floor(c * led_count / sum(caps))];
row_n = [for (i = [0 : len(caps) - 1]) base_n[i] + (i < led_count - sum(base_n) ? 1 : 0)];  // spread evenly
ch_x0 = W / 2 - chamber_in / 2 - chamber_wall;
ch_z0 = wall + clr;
ch_y0 = shell_y1 - chamber_depth;
spk_z = ch_z0 + chamber_wall + chamber_in / 2;
pcbs = [for (i = [-1, 1], j = [-1, 1]) pcb_center + [i * pcb_holes[0] / 2, j * pcb_holes[1] / 2]];
controls = [[0, enc_d], [-btn_dx, btn_d], [btn_dx, btn_d]];

echo(str("LED rows, bottom to top: ", row_n, " (", led_per_m, " LED/m)"));
assert(z0 >= r_bottom, "arch needs H >= W/2 + r_bottom");
assert(sum(caps) >= led_count, str("only ", sum(caps), " LEDs fit the dome: raise led_per_m or lower led_count"));
assert(min([for (i = [0 : len(caps) - 1]) caps[i] - row_n[i]]) >= 0, "LED row overfilled");
assert(rim_in > frame_in + 0.5, "rim must overlap the fabric frame");
assert(rib_d / 2 > clr, "crush ribs must reach past the clearance");
assert(ch_y0 > plate_y1 + 3, "speaker chamber hits the light box");
assert(chamber_in >= spk_d + 2 * clr + 4, "speaker seat ring needs a 2 mm lip");
assert(clock_z + clock_size[1] / 2 + 4 < horizon_z - soft - diff_lap, "clock overlaps the light window");
assert(radar_xz[0] + radar_size[0] / 2 + 2.1 < W / 2 - clock_size[0] / 2 - 2.1, "radar pocket hits the clock sleeve");
assert(radar_xz[0] - radar_size[0] / 2 - 2.1 > frame_screws[0][0] + 4, "radar pocket hits the frame screw boss");
assert(max(W, H) <= bed, "part larger than the print bed");

// ---- 2D outlines in the (x, z) front plane ----
module arch(inset = 0) {  // convex: half circle on top, rounded bottom corners
  r = W / 2 - inset;
  rb = max(r_bottom - inset, 0.5);
  hull() {
    translate([W / 2, z0]) intersection() { circle(r = r, $fn = 192); translate([-r, 0]) square([2 * r, r]); }
    for (x = [inset + rb, W - inset - rb]) translate([x, inset + rb]) circle(r = rb);
  }
}
module above(z) { translate([-1, z]) square([W + 2, H + 2]); }
module window2d() { intersection() { arch(win_inset); above(horizon_z - soft); } }
module diffuser2d() { offset(r = diff_lap) window2d(); }
module cavity2d() { intersection() { arch(win_inset); above(horizon_z); } }
module radar2d(grow = 0) { translate(radar_xz) offset(delta = grow) square(radar_size, center = true); }
module clock2d(grow = 0) { translate([W / 2, clock_z]) offset(delta = grow) square(clock_size, center = true); }

// ---- Helpers ----
module slab(y0, y1) {  // extrude a 2D (x, z) shape along +Y from y0 to y1
  translate([0, y1, 0]) rotate([90, 0, 0]) linear_extrude(y1 - y0) children();
}
module chamfered(y0, y1, front = true) {  // outer body with a 45 deg chamfer on its front or back face
  hull() {
    if (front) { slab(y0, y0 + eps) arch(edge); slab(y0 + edge, y1) arch(0); }
    else { slab(y0, y1 - edge) arch(0); slab(y1 - eps, y1) arch(edge); }
  }
}
module along_y(p, y) { translate([p[0], y, p[1]]) rotate([-90, 0, 0]) children(); }  // cylinder axis +Y
module mirror_x() { children(); translate([W, 0, 0]) mirror([1, 0, 0]) children(); }
module on_arc(dx, y) {  // radial placement on the curved top, dx from the centre line
  translate([W / 2, y, z0]) rotate([0, asin(dx / (W / 2)), 0]) children();
}

// ---- Shell: rim + walls, open at the back (prints back rim down) ----
module frame_boss(p) {  // corner column from the back rim (the print bed) to the frame: no overhang anywhere
  len = shell_y1 - frame_y1 - 4;
  hull() {
    along_y(p, frame_y1 + 4) cylinder(d = 9, h = len);
    translate([wall - embed, frame_y1 + 4, p[1] - 3]) cube([1 + embed, len, 6]);
    translate([p[0] - 3, frame_y1 + 4, wall - embed]) cube([6, len, 1 + embed]);
  }
}

module shell() {
  difference() {
    union() {
      difference() {
        chamfered(0, shell_y1, true);
        slab(rim_t, shell_y1 + 1) arch(wall);
        slab(-1, rim_t + 1) arch(rim_in);
      }
      mirror_x() frame_boss(frame_screws[0]);
      for (p = back_bosses) along_y(p, shell_y1 - 10) cylinder(d = 9, h = 10);
    }
    for (c = controls) on_arc(c[0], ctrl_y) translate([0, 0, W / 2 - wall - 2]) cylinder(d = c[1], h = wall + 4);
    translate([W / 2, lux_y, H - wall - 2]) cylinder(d = lux_d, h = wall + 4);
    for (p = frame_screws) {  // M3x10 countersunk; longer would push through the frame face
      along_y(p, frame_y1 + 3) cylinder(d = screw_d, h = 7);
      along_y(p, frame_y1 + 10 - (head_d - screw_d) / 2) cylinder(d1 = screw_d, d2 = head_d, h = (head_d - screw_d) / 2 + eps);
      along_y(p, frame_y1 + 10) cylinder(d = head_d, h = shell_y1);  // driver channel from the open back
    }
    for (p = back_bosses) along_y(p, shell_y1 - insert_len - 1) cylinder(d = insert_d, h = insert_len + 2);
    for (x = [30, W - 30], y = [12, shell_y1 - 12]) translate([x, y, -eps]) cylinder(d = foot_d, h = foot_depth);
  }
}

// ---- Fabric frame: linen wraps its edge, diffuser presses into its back, clock behind the fabric ----
module frame() {
  difference() {
    union() {
      slab(frame_y0, frame_y1) arch(frame_in);
      slab(frame_y1 - embed, frame_y1 + clock_sleeve) difference() { clock2d(clr + 1.6); clock2d(clr); }
      slab(frame_y1 - embed, frame_y1 + radar_sleeve) difference() { radar2d(clr + 1.6); radar2d(clr); }  // radar looks through the frame
      for (p = frame_screws) along_y(p, frame_y1 - embed) cylinder(d = 8, h = 4 + embed);
    }
    slab(frame_y0 - 1, frame_y1 + 1) window2d();
    slab(frame_y1 - diff_t, frame_y1 + 1) offset(delta = clr) diffuser2d();
    slab(frame_y0 - 1, frame_y1 + clock_sleeve + 1) clock2d(clr);
    for (p = frame_screws) along_y(p, frame_y1 + 4 - insert_len) cylinder(d = insert_d, h = insert_len + 1);
  }
  rp = cav_r + diff_lap + clr;  // crush ribs on the pocket edge
  for (a = [25, 90, 155]) along_y([W / 2 + rp * cos(a), z0 + rp * sin(a)], frame_y1 - diff_t - eps) cylinder(d = rib_d, h = diff_t, $fn = 12);
  for (x = [W * 0.3, W * 0.7]) along_y([x, horizon_z - soft - diff_lap - clr], frame_y1 - diff_t - eps) cylinder(d = rib_d, h = diff_t, $fn = 12);
}

module diffuser() { slab(frame_y1 - diff_t, frame_y1) diffuser2d(); }

// ---- Light box: LED plate + walls + horizon baffle, screwed to the back-cover posts ----
module light_box() {
  difference() {
    union() {
      slab(plate_y0, plate_y1) offset(r = box_wall) cavity2d();
      difference() {
        slab(frame_y1, plate_y0 + embed) difference() { offset(r = box_wall) cavity2d(); cavity2d(); }
        slab(frame_y1 - 1, frame_y1 + baffle_gap) translate([0, horizon_z - box_wall - 1]) square([W, box_wall + 1 + eps]);
      }
    }
    slab(plate_y0 - 1, plate_y0 + channel_d) for (i = [0 : len(row_zs) - 1])  // strip rows
      translate([W / 2 - (row_n[i] * pitch + 2) / 2, row_zs[i] - (strip_w + clr) / 2]) square([row_n[i] * pitch + 2, strip_w + clr]);
    slab(plate_y0 - 1, plate_y1 + 1) for (i = [0 : len(row_zs) - 1], s = [-1, 1])  // jumper holes at row ends
      translate([W / 2 + s * ((row_n[i] * pitch + 2) / 2 + 3), row_zs[i]]) circle(d = 4);
    for (p = box_posts) along_y(p, plate_y0 - eps) {
      translate([0, 0, -1]) cylinder(d = screw_d, h = plate_t + 2);
      cylinder(d1 = head_d, d2 = screw_d, h = (head_d - screw_d) / 2);
    }
  }
}

// ---- Back cover: speaker chamber, devkit standoffs, light-box posts ----
module back_cover() {
  difference() {
    union() {
      chamfered(shell_y1, D, false);
      difference() {
        translate([ch_x0, ch_y0, ch_z0]) cube([chamber_in + 2 * chamber_wall, chamber_depth + embed, chamber_in + 2 * chamber_wall]);
        translate([ch_x0 + chamber_wall, ch_y0 - 1, ch_z0 + chamber_wall]) cube([chamber_in, chamber_depth + 1, chamber_in]);
      }
      along_y([W / 2, spk_z], shell_y1 - 3) difference() {  // seat that centres the driver
        cylinder(d = chamber_in + embed, h = 3 + embed);  // merges into the chamber walls (no tangent edges)
        translate([0, 0, -1]) cylinder(d = spk_d + 2 * clr, h = 5 + embed);
      }
      for (p = box_posts) along_y(p, plate_y1) cylinder(d1 = 8, d2 = 11, h = shell_y1 - plate_y1 + embed);
      for (p = pcbs) along_y(p, shell_y1 - pcb_standoff) cylinder(d = 8, h = pcb_standoff + embed);
    }
    for (p = box_posts) along_y(p, plate_y1 - eps) cylinder(d = insert_d, h = insert_len + 1);
    for (p = pcbs) along_y(p, shell_y1 - pcb_standoff - eps) cylinder(d = insert_d, h = insert_len + 1);
    for (p = back_bosses) {
      along_y(p, shell_y1 - 1) cylinder(d = screw_d, h = back_t + 2);
      along_y(p, D - (head_d - screw_d) / 2 + eps) cylinder(d1 = screw_d, d2 = head_d, h = (head_d - screw_d) / 2);
    }
    along_y([W / 2, spk_z], shell_y1 - 1) linear_extrude(back_t + 2) intersection() {
      circle(d = grille_d);
      for (k = [-6 : 6]) translate([-grille_d / 2, k * 4 - 1]) square([grille_d, 2]);
    }
    along_y(usbc_xz, shell_y1 - 1) linear_extrude(back_t + 2)
      hull() for (s = [-1, 1]) translate([s * (usbc[0] - usbc[1]) / 2, 0]) circle(d = usbc[1]);
    for (i = [0 : vent_n - 1])
      translate([W / 2 + (i - (vent_n - 1) / 2) * 7 - vent[0] / 2, shell_y1 - 1, vent_z]) cube([vent[0], back_t + 2, vent[1]]);
  }
}

module speaker_lid() {  // lip rests on the chamber's front edge, plug presses inside
  difference() {
    union() {
      translate([ch_x0, ch_y0 - 2, ch_z0]) cube([chamber_in + 2 * chamber_wall, 2, chamber_in + 2 * chamber_wall]);
      translate([ch_x0 + chamber_wall + clr, ch_y0 - embed, ch_z0 + chamber_wall + clr]) cube([chamber_in - 2 * clr, 3 + embed, chamber_in - 2 * clr]);
    }
    along_y([W / 2, ch_z0 + 12], ch_y0 - 3) cylinder(d = 4, h = 7);  // speaker wire, seal with glue
  }
}

module knob() {  // dome cap for the EC11 encoder (6 mm D shaft)
  difference() {
    hull() {
      cylinder(d = knob_d, h = 1);
      translate([0, 0, knob_h - knob_d / 6]) scale([1, 1, 1 / 3]) sphere(d = knob_d);
    }
    translate([0, 0, -eps]) linear_extrude(knob_h - 2) intersection() {
      circle(d = 6.1);
      translate([-3.05, -3.05]) square([6.1, 4.6]);
    }
  }
}

// ---- Layout ----
module assembled(name) {
  if (name == "shell") shell();
  else if (name == "frame") frame();
  else if (name == "diffuser") diffuser();
  else if (name == "light_box") light_box();
  else if (name == "back_cover") back_cover();
  else if (name == "speaker_lid") speaker_lid();
  else if (name == "knob") translate([W / 2, ctrl_y, H + 0.6]) knob();
}

module printable(name) {  // flat on the bed, the side each part is designed to print on
  if (name == "shell") translate([0, 0, shell_y1]) rotate([-90, 0, 0]) shell();                // back rim down
  else if (name == "frame") translate([0, H, -frame_y0]) rotate([90, 0, 0]) frame();           // face down
  else if (name == "diffuser") translate([0, H, -(frame_y1 - diff_t)]) rotate([90, 0, 0]) diffuser();
  else if (name == "light_box") translate([0, 0, plate_y1]) rotate([-90, 0, 0]) light_box();   // plate down
  else if (name == "back_cover") translate([0, 0, D]) rotate([-90, 0, 0]) back_cover();        // outside down
  else if (name == "speaker_lid") translate([0, H, -(ch_y0 - 2)]) rotate([90, 0, 0]) speaker_lid();
  else if (name == "knob") knob();
}

// 3x5 digit font, one 3-bit number per row (4 = left dot); same glyphs as renderClock() in src/Logic.h
font = [[7, 5, 5, 5, 7], [2, 6, 2, 2, 7], [7, 1, 7, 4, 7], [7, 1, 7, 1, 7], [5, 5, 7, 1, 1],
        [7, 4, 7, 1, 7], [7, 4, 7, 5, 7], [7, 1, 1, 1, 1], [7, 5, 7, 5, 7], [7, 5, 7, 1, 7]];
module dot(c, r) { translate([W / 2 + (c - 7.5) * 2.54, frame_y0 - 0.3, clock_z + (3.5 - r) * 2.54]) rotate([90, 0, 0]) cylinder(d = 1.7, h = 0.2, $fn = 12); }
module clock_dots(d) {  // d = 4 digits, -1 = blank
  for (k = [0 : 3], r = [0 : 4], b = [0 : 2]) if (d[k] >= 0 && floor(font[d[k]][r] / pow(2, 2 - b)) % 2 == 1) dot(k * 4 + b, r + 1);
  for (r = [2, 4]) dot(7, r);
}

putty = "#e6e0d4"; linen = "#d9ccb8";
module view(e) {  // explodes toward the back: every part goes in from behind the rim
  color(putty) translate([0, -e, 0]) shell();
  color(putty) translate([0, -e, e * 0.6]) assembled("knob");
  color(linen) frame();
  color("#fff8ee", 0.6) translate([0, 0.5 * e, 0]) diffuser();
  color("#ffffff") translate([0, 1.2 * e, 0]) light_box();
  color("#cfc8bb") translate([0, 2.2 * e, 0]) speaker_lid();
  color(putty) translate([0, 3 * e, 0]) back_cover();
}

if (part == "assembly") view(0);
else if (part == "front") {  // on the nightstand with the light on: glow above a soft horizon, clock dots below
  view(0);
  color("#ffcf8a") slab(frame_y0 - 0.5, frame_y0 - 0.4) intersection() { window2d(); above(horizon_z); }
  color("#ead2ae") slab(frame_y0 - 0.5, frame_y0 - 0.4) intersection() { window2d(); translate([0, horizon_z - soft]) square([W, soft]); }
  color("#fff4e0") clock_dots([-1, 6, 3, 0]);
}
else if (part == "exploded") view(explode);
else if (part == "intersect") intersection() { assembled(check[0]); assembled(check[1]); }
else printable(part);
