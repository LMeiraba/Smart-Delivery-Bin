// Smart Delivery Bin - Lid Only
// All dimensions in millimeters (mm)

/* --- Parameters --- */
width = 150;
depth = 110;
height = 110;
wall = 1.6;

// Hinge Parameters
hinge_r = 5;
hinge_hole_r = 1.5; // 3mm diameter hole 
hinge_w = 15;

/* --- Modules --- */

module hinge_knuckle() {
    difference() {
        cylinder(r=hinge_r, h=hinge_w, $fn=32);
        translate([0,0,-1]) cylinder(r=hinge_hole_r, h=hinge_w+2, $fn=32);
    }
}

module lid() {
    union() {
        difference() {
            // Lid Base 
            cube([width, depth, wall]);
        }
        
        // Inner lip to align the lid
        translate([wall + 0.5, wall + 0.5, wall - 0.1])
            difference() {
                cube([width - 2*wall - 1, depth - 2*wall - 1, 5.1]);
                translate([wall, wall, -1]) cube([width - 4*wall - 1, depth - 4*wall - 1, 7.1]); 
            }
            
        // Lid Hinges
        // Shifted to depth + 3 to align with the box hinges
        translate([width/4 + hinge_w/2 + 0.5, depth + 3, 5]) rotate([0, 90, 0]) hinge_knuckle();
        translate([width/4 - hinge_w*1.5 - 0.5, depth + 3, 5]) rotate([0, 90, 0]) hinge_knuckle();
        
        translate([3*width/4 + hinge_w/2 + 0.5, depth + 3, 5]) rotate([0, 90, 0]) hinge_knuckle();
        translate([3*width/4 - hinge_w*1.5 - 0.5, depth + 3, 5]) rotate([0, 90, 0]) hinge_knuckle();
        
        // Solenoid Mounting Block (Now pointing UP in local space so the lid is flat on the bottom!)
        translate([width/2 - 15, wall + 6, wall]) cube([30, 35, 5]);
    }
}

// Render the lid
lid();
