// Boîtier imprimable pour Wemos D1 mini + shield WeMos RFM69 Gateway v1.0b (ch2i)
//
// Deux pièces assemblées par 2 vis M3 x 10 DIN 912 (CHC), vissées directement
// dans le plastique, têtes noyées sous le boîtier (invisibles posé sur un meuble).
// Aucune pièce ne travaille en flexion (PLA cassant).
//
//  - le FOND reçoit la pile Wemos + shield, posée verticalement. Le bord du
//    shield vient contre la paroi côté antenne, le bout de la Wemos contre un
//    bloc de butée au fond (la carte est tenue en longueur même sans couvercle).
//    L'embase carrée du SMA traverse la paroi par une encoche ouverte vers le
//    haut, fermée par le couvercle qui vient au ras du dessus de l'embase ;
//  - le COUVERCLE porte, côté antenne, une lèvre rigide qui s'engage dans une
//    rainure de la paroi ; côté opposé, une cloison transversale descend sur le
//    bloc de butée et reçoit les 2 vis, passées par-dessous à travers le bloc.
//  Montage (verrou coulissant, rien ne s'incline) : poser le couvercle à plat,
//  décalé de `slide` mm vers le côté opposé à l'antenne, le descendre, le pousser
//  vers l'antenne (la lèvre entre dans la rainure), visser les 2 vis.
//
// Prise micro-USB : un tunnel ouvert en façade ET par-dessous reçoit la
// surmoulure de la fiche ; il se termine par une cloison fine au ras du bord
// de la Wemos, percée seulement du petit trou de la prise micro-USB.
//
// Repère : X = longueur (paroi antenne / USB en X=0), Y = largeur (Y=0 côté des
// broches TX..5V de la Wemos), Z = hauteur. Origine au coin intérieur du fond.
//
// Sources des cotes :
//  - Wemos D1 mini : empreinte KiCad officielle WEMOS_D1_mini_light
//  - shield : fichier Eagle hallard/WeMos-RFM69 (contour 39,62 x 25,65 ;
//    SMA à 3,75 du bord côté TX ; LEDs et bouton sur la face intérieure)
//  - mesuré : longueur de la pile hors SMA 41,0 ; écart entre cartes 11,5 ;
//    dessus de l'embase SMA à 3,8 au-dessus du shield
//
// Variante « sonde » (probe = true) : le fond est percé d'un trou dans la grande
// paroi opposée au SMA, entre les deux cartes, pour le câble de la sonde de
// température DS18B20 optionnelle. Le câble arrive sous le shield ; ses 3 fils
// passent par le dessous du shield dans le connecteur I2C (près du SMA) et sont
// soudés sur le dessus. Le couvercle est le même dans les deux versions.
//
// Impression : PLA ou PETG, buse 0,4, couches 0,2, SANS support.
//   openscad -D 'part="base"' -o fond.stl boitier.scad
//   openscad -D 'part="base"' -D 'probe=true' -o fond-sonde.stl boitier.scad
//   openscad -D 'part="lid"'  -o couvercle.stl boitier.scad  (pivoté à 180°, face lisse au plateau)

part = "assembly";  // "base", "lid", "print", "assembly", "exploded", "section"
probe = false;      // true : fond percé pour le câble de la sonde DS18B20

/* ========================================================================
   1. COTES DES CARTES
   ======================================================================== */

stack_l = 41.0;          // longueur de la pile montée, hors SMA (mesuré)

// --- Wemos D1 mini (carte du bas, composants vers le haut) ---
wemos_l = 34.2;
wemos_w = 25.5;          // mesuré
wemos_t = 1.0;
under_clear = 3.0;       // pattes des connecteurs sous la Wemos (≈ 2,3)
wemos_notch_w = 2.54;    // encoche du bouton reset (côté Y=max, bord USB)
wemos_notch_l = 6.9;
// Trous de fixation de la Wemos (relevés sur photo, à vérifier) :
// [X depuis le bord USB, Y depuis le bord côté TX]
wemos_holes = [[2.8, 3.2], [wemos_l - 3.0, wemos_w - 2.8]];
wemos_boss_d = 5.0;      // bossage sous chaque trou
wemos_screw_d = 1.5;     // avant-trou pour petite vis autotaraudeuse
wemos_screw_depth = 4.0; // profondeur depuis le dessus du bossage

// Prise micro-USB (face supérieure, bord X=0)
usb_y = 12.0;            // centre de la prise depuis le bord Y=0
usb_h = 2.8;             // hauteur du connecteur au-dessus du PCB
usb_overhang = 0.0;      // débord de la prise au-delà du PCB (au ras sur la D1 mini)

// Tunnel USB (ouvert en façade et par-dessous) pour la surmoulure de la fiche
plug_w = 12.5;           // largeur
plug_up = 5.0;           // hauteur au-dessus de l'axe de la prise
// Cloison au ras de la Wemos, percée du trou de la prise
usb_wall_t = 0.8;        // épaisseur (la fiche doit encore s'enfoncer complètement)
usb_hole = [8.4, 3.4];   // trou pour la partie métallique de la fiche (6,9 x 1,9 + jeu)

// --- Empilement ---
stack_gap = 11.5;        // face supérieure Wemos -> face inférieure shield (mesuré)

// --- Shield RFM69 (carte du haut) ---
shield_l = 39.62;
shield_w = 25.5;         // mesuré
shield_t = 1.6;
shield_dy = -0.03;
shield_notch_w = 1.9;    // encoche côté Y=max sur la partie qui déborde
shield_notch_l = 14.7;
top_clear = 2.0;         // face extérieure : pattes ≈ 1,1 ; ATSHA204A (si soudé) 1,75

// Connecteur SMA (bord X=0 du shield)
sma_y = 3.75;            // centre depuis le bord Y=0 du shield
sma_top = 3.8;           // dessus de l'embase carrée au-dessus de la face supérieure du shield (mesuré)
sma_body = 6.5;          // côté de l'embase carrée (mesuré)
sma_thread_d = 6.35;     // filetage 1/4"

// Connecteur I2C (4 trous 3V3 / GND / SCL / SDA, relevés sur photo) : à 2,5 du
// bord côté antenne, trous de 9,4 (SDA) à 17,0 (3V3) depuis le bord côté SMA
i2c_x = 2.5;
i2c_y = [9.4, 17.0];

// Câble de la sonde DS18B20 (gaine ronde Ø 4 sur la plupart des sondes étanches)
probe_cable_d = 4.0;
probe_hole_x = 3.6;      // axe du trou depuis la paroi côté antenne

leds_y = [7.1, 18.5];    // LEDs WS2812 (face intérieure du shield), pour l'aperçu

pad = 3.0;               // appuis (plots du fond, colonnes du couvercle)

/* ========================================================================
   2. VISSERIE : M3 DIN 912 (tête cylindrique à six pans creux)
   ======================================================================== */
screw_l = 10;            // longueur de vis (4, 5, 6, 8 ou 10 disponibles)
screw_head_d = 5.5;      // tête DIN 912 M3 : Ø 5,5 x 3
screw_head_h = 3.0;
screw_clear_d = 3.4;     // passage dans le fond
screw_pilot_d = 2.8;     // avant-trou dans la cloison (la vis taraude le plastique ; 2,5 : colonnes fendues)
head_seat = 1.5;         // épaisseur sous la tête
post_d = 8.0;            // épaisseur de la cloison à vis du couvercle (≥ 2,6 de matière autour du trou)
stop_h = 1.2;            // le bloc de butée dépasse de ce jeu au-dessus de la Wemos

/* ========================================================================
   3. RÉGLAGES D'IMPRESSION
   ======================================================================== */
wall = 1.8;              // parois (≈ 4 périmètres de 0,45)
floor_t = 1.6;
lid_t = 2.0;
clr = 0.2;               // jeu cartes / grandes parois (largeur)
clr_l = 0.15;            // jeu shield / paroi côté antenne
stop_gap = 0.15;         // jeu bout de la Wemos / bloc de butée
fit = 0.2;               // jeu entre pièces
corner_r = 2.0;
inner_t = 1.2;           // parois du tunnel USB
skirt_h = 2.4;           // jupe de centrage du couvercle
skirt_t = 1.2;
slide = 1.6;             // course du verrou coulissant
tab_t = 2.0;             // épaisseur de la patte qui porte la lèvre
lip_y = [9.5, 20.0];     // étendue (Y) de la lèvre côté antenne
lip_in = 1.0;            // engagement de la lèvre dans la paroi
lip_h = 1.4;             // épaisseur de la lèvre
$fn = 48;

/* ========================================================================
   Géométrie dérivée
   ======================================================================== */
eps = 0.01;
sh_x0 = clr_l;                                 // bord du shield contre la paroi côté antenne
wemos_x0 = sh_x0 + stack_l - wemos_l;
wemos_y0 = clr + max(0, -shield_dy);
sh_y0 = wemos_y0 + shield_dy;
stack_end = sh_x0 + stack_l;                   // bout de la Wemos

in_w = max(wemos_y0 + wemos_w, sh_y0 + shield_w) + clr;
stop_x = stack_end + stop_gap;                 // face du bloc de butée
post_x = stop_x + post_d / 2;                  // axe des vis
post_ys = [fit + post_d / 2, in_w - fit - post_d / 2];
in_l = stop_x + post_d + slide + fit;          // place pour poser le couvercle décalé

wemos_z = under_clear;
wemos_top = wemos_z + wemos_t;
shield_z = wemos_top + stack_gap;              // face inférieure du shield
shield_top = shield_z + shield_t;
sma_z = shield_top + sma_top - sma_body / 2;   // axe du SMA
sma_yc = sh_y0 + sma_y;
in_h = max(shield_top + top_clear, shield_top + sma_top);  // couvercle au ras de l'embase SMA
sma_slot_z = shield_top + sma_top - sma_body - 0.9;        // bas de l'encoche SMA

out_l = in_l + 2 * wall;
out_w = in_w + 2 * wall;

usb_yc = wemos_y0 + usb_y;
usb_zc = wemos_top + usb_h / 2;
usb_wall_x1 = wemos_x0 - usb_overhang - 0.2;   // face arrière de la cloison (côté prise)
usb_wall_x0 = usb_wall_x1 - usb_wall_t;        // face avant (côté fiche)
tun_top = usb_zc + plug_up;                    // plafond du tunnel
probe_z = (wemos_top + shield_z) / 2;          // axe du trou de la sonde (entre les cartes)
probe_hole_d = probe_cable_d + 0.5;

boss_top = wemos_top + stop_h;                 // dessus du bloc de butée
head_z = boss_top - head_seat;                 // face d'appui sous la tête
post_bot = boss_top + fit;                     // bas de la cloison du couvercle
screw_bite = head_z + screw_l - post_bot;      // longueur vissée dans la cloison

skirt_b = in_h - skirt_h;
lip_z0 = shield_top + 0.4;                     // bas de la patte de la lèvre
lip_z = [lip_z0, lip_z0 + lip_h];              // lèvre (dans la rainure)

assert(tun_top + inner_t < shield_z - 0.5, "le tunnel USB touche le shield");
assert(skirt_b > shield_top + 1.2, "la jupe touche les soudures du shield");
assert(wemos_z - wemos_screw_depth >= -floor_t + 0.6, "avant-trou Wemos trop profond (perce le fond)");
assert(head_z - screw_head_h >= -floor_t + 0.3, "tête de vis non noyée");
assert(screw_bite >= 6, "vis trop courte");
assert(screw_bite + 1 < in_h - post_bot, "vis trop longue pour la cloison");
assert(slide > lip_in + 0.3, "course insuffisante pour dégager la lèvre");
assert(lip_z[1] + 0.2 < in_h - 1.2, "rainure trop près du haut de la paroi");
assert(probe_hole_x + probe_hole_d / 2 < wemos_x0 - 0.3, "trou de sonde en face de la Wemos");
assert(probe_hole_x - probe_hole_d / 2 > 0.8, "trou de sonde trop près de la paroi côté antenne");
assert(usb_yc + plug_w / 2 + inner_t < in_w - 2, "pas de place entre le tunnel USB et la paroi pour le câble");

/* ========================================================================
   FOND
   ======================================================================== */
module base() {
  difference() {
    union() {
      difference() {
        translate([-wall, -wall, -floor_t]) rounded_box(out_l, out_w, floor_t + in_h, corner_r);
        cube([in_l, in_w, in_h + 1]);
      }
      // appuis de la Wemos : 2 bossages sous ses trous de fixation + 1 plot
      for (h = wemos_holes) translate([wemos_x0 + h[0], wemos_y0 + h[1], -eps])
        cylinder(d = wemos_boss_d, h = wemos_z + eps);
      translate([wemos_x0 + wemos_l - pad, wemos_y0, -eps]) cube([pad, pad, wemos_z + eps]);
      // tunnel USB : parois, plafond et cloison du fond
      translate([-eps, usb_yc - plug_w / 2 - inner_t, -eps])
        cube([usb_wall_x1 + eps, plug_w + 2 * inner_t, tun_top + inner_t + eps]);
      // bloc de butée de la Wemos, traversé par les vis
      translate([stop_x, -eps, -eps]) cube([in_l - stop_x + eps, in_w + 2 * eps, boss_top + eps]);
    }
    // tunnel USB, ouvert en façade et par-dessous
    translate([-wall - 1, usb_yc - plug_w / 2, -floor_t - 1])
      cube([wall + 1 + usb_wall_x0, plug_w, tun_top + floor_t + 1]);
    // petit trou de la prise dans la cloison
    translate([usb_wall_x0 - 1, 0, 0]) rotate([90, 0, 90]) linear_extrude(usb_wall_t + 2)
      translate([usb_yc, usb_zc]) rounded_rect(usb_hole[0], usb_hole[1], 0.8);
    // encoche carrée de l'embase du SMA, ouverte vers le haut
    translate([-wall - 1, sma_yc - sma_body / 2 - 0.25, sma_slot_z])
      cube([wall + 1 + sh_x0 + eps, sma_body + 0.5, in_h]);
    // rainure qui reçoit la lèvre du couvercle
    translate([-lip_in - 0.1, lip_y[0] - 0.3, lip_z[0] - 0.2])
      cube([lip_in + 0.1 + eps, lip_y[1] - lip_y[0] + 0.6, lip_z[1] - lip_z[0] + 0.4]);
    // avant-trous des vis de la Wemos
    for (h = wemos_holes) translate([wemos_x0 + h[0], wemos_y0 + h[1], wemos_z - wemos_screw_depth])
      cylinder(d = wemos_screw_d, h = wemos_screw_depth + 1, $fn = 24);
    // variante sonde : passage du câble dans la grande paroi opposée au SMA
    if (probe) translate([probe_hole_x, in_w - eps, probe_z]) rotate([-90, 0, 0])
      cylinder(d = probe_hole_d, h = wall + 1, $fn = 32);
    // passages des vis et logements des têtes
    for (y = post_ys) translate([post_x, y, 0]) {
      translate([0, 0, -floor_t - 1]) cylinder(d = screw_clear_d, h = boss_top + floor_t + 2);
      translate([0, 0, -floor_t - 1]) cylinder(d = screw_head_d + 0.5, h = head_z + floor_t + 1);
    }
  }
}

/* ========================================================================
   COUVERCLE (modélisé en position montée)
   ======================================================================== */
module lid() {
  difference() {
    union() {
      translate([-wall, -wall, in_h]) rounded_box(out_l, out_w, lid_t, corner_r);
      // jupe de centrage le long des grandes parois
      for (y = [fit, in_w - fit - skirt_t])
        translate([fit + tab_t, y, skirt_b]) cube([stop_x - fit - tab_t + eps, skirt_t, skirt_h + eps]);
      // patte et lèvre côté antenne
      translate([fit, lip_y[0], lip_z0]) cube([tab_t, lip_y[1] - lip_y[0], in_h - lip_z0 + eps]);
      translate([-lip_in, lip_y[0], lip_z[0]]) cube([lip_in + fit + eps, lip_y[1] - lip_y[0], lip_h]);
      // colonnes qui plaquent le shield (pas sur le coin du SMA)
      for (p = [[sh_x0, sh_y0 + shield_w - shield_notch_w - pad],
                [sh_x0 + shield_l - pad, sh_y0],
                [sh_x0 + shield_l - pad, sh_y0 + shield_w - pad]])
        translate([p[0], p[1], shield_top + 0.1]) cube([pad, pad, in_h - shield_top - 0.1 + eps]);
      // cloison transversale qui reçoit les vis
      translate([stop_x, fit, post_bot]) cube([post_d, in_w - 2 * fit, in_h - post_bot + eps]);
    }
    // avant-trous des vis
    for (y = post_ys) translate([post_x, y, post_bot - 1]) cylinder(d = screw_pilot_d, h = screw_bite + 1.5);
  }
}

/* ========================================================================
   Outils et aperçu
   ======================================================================== */
module rounded_box(l, w, h, r) {
  hull() for (x = [r, l - r], y = [r, w - r]) translate([x, y, 0]) cylinder(r = r, h = h);
}

module rounded_rect(w, h, r) {
  offset(r) square([w - 2 * r, h - 2 * r], center = true);
}

module boards() {
  color("royalblue") translate([wemos_x0, wemos_y0, wemos_z]) difference() {
    cube([wemos_l, wemos_w, wemos_t]);
    translate([-1, wemos_w - wemos_notch_w, -1]) cube([wemos_notch_l + 1, wemos_notch_w + 1, wemos_t + 2]);
    for (h = wemos_holes) translate([h[0], h[1], -1]) cylinder(d = 2.2, h = wemos_t + 2);
  }
  color("silver") translate([wemos_x0 - usb_overhang, usb_yc - 3.75, wemos_top]) cube([5.5, 7.5, usb_h]);
  color("black") for (y = [wemos_y0 + 0.1, wemos_y0 + wemos_w - 2.64])
    translate([wemos_x0 + 8.21 - 1.27, y, wemos_top]) cube([20.32, 2.54, stack_gap]);
  color("seagreen") translate([sh_x0, sh_y0, shield_z]) difference() {
    cube([shield_l, shield_w, shield_t]);
    translate([-1, shield_w - shield_notch_w, -1]) cube([shield_notch_l + 1, shield_notch_w + 1, shield_t + 2]);
  }
  color("dimgray") translate([sh_x0 + 9.27, sh_y0 + 4.8, shield_z - 1.8]) cube([16, 16, 1.8]);  // RFM69
  color("white") for (y = leds_y) translate([sh_x0 + 35.7 - 2.5, sh_y0 + y - 2.5, shield_z - 1.6]) cube([5, 5, 1.6]);
  color("black") for (i = [0:3]) translate([sh_x0 + i2c_x, sh_y0 + i2c_y[0] + i * 2.54, shield_z - 0.01])
    cylinder(d = 1.6, h = shield_t + 0.02, $fn = 12);
  if (probe) color("dimgray") {  // câble de la sonde (aperçu)
    translate([probe_hole_x, in_w + wall + 15, probe_z]) rotate([90, 0, 0]) cylinder(d = probe_cable_d, h = 15 + wall + 4);
  }
  color("gold") {
    translate([sh_x0 - 2.6, sma_yc - sma_body / 2, sma_z - sma_body / 2]) cube([2.6 + 1, sma_body, sma_body]);
    translate([sh_x0 - 2.6, sma_yc, sma_z]) rotate([0, -90, 0]) cylinder(d = sma_thread_d, h = 9.7);
  }
}

module screws() color("dimgray") for (y = post_ys) translate([post_x, y, head_z - screw_head_h]) {
  difference() { cylinder(d = screw_head_d, h = screw_head_h); translate([0, 0, -1]) cylinder(d = 2.5, h = 2.5, $fn = 6); }
  translate([0, 0, screw_head_h]) cylinder(d = 3, h = screw_l);
}

// Couvercle pivoté de 180° autour de X (et non pas en miroir !) pour l'impression
module lid_print() translate([0, in_w, in_h + lid_t]) rotate([180, 0, 0]) lid();

sec_y = usb_yc;          // plan de coupe de part="section"

if (part == "base") base();
if (part == "lid") lid_print();
if (part == "assembly") { color("gold") base(); color("orange") lid(); boards(); screws(); }
if (part == "exploded") {
  color("gold") base(); boards();
  translate([slide, 0, 24]) color("orange", 0.9) lid();
  translate([0, 0, -14]) screws();
}
if (part == "section") difference() {
  union() { color("gold") base(); color("orange") lid(); boards(); screws(); }
  translate([-20, sec_y - 100, -10]) cube([100, 100, 100]);
}
if (part == "print") { base(); translate([0, out_w + 6, 0]) lid_print(); }

echo(str("Dimensions extérieures : ", out_l, " x ", out_w, " x ", floor_t + in_h + lid_t,
         " mm (hors filetage d'antenne) ; vis M3 x ", screw_l, " vissée sur ", screw_bite, " mm"));
