// Smart Delivery Bin - Main Enclosure and Lid (Printable Mechanics)
// All dimensions in millimeters (mm)

/* --- Parameters --- */
width = 200;
depth = 150;
height = 150;
wall = 3;

// OLED Cutout
oled_w = 26.5;
oled_h = 14.5;
oled_z = 90; 

// Keypad Ribbon Slit
slit_w = 25;
slit_h = 2;
slit_z = 75; 

// Solenoid Mount
solenoid_w = 27;
solenoid_d = 15;

// HC-SR04 Sensor Holes
sensor_hole_d = 16;
sensor_spacing = 25.5;

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

module main_box() {
    union() {
        difference() {
            // Outer solid box
            cube([width, depth, height]);
            
            // Inner hollow cavity
            translate([wall, wall, wall])
                cube([width - 2*wall, depth - 2*wall, height]); 
                
            // Keypad Ribbon Slit
            translate([width/2 - slit_w/2, -1, slit_z])
                cube([slit_w, wall+2, slit_h]);
                
            // Wire Routing Hole (Left Wall)
            // Allows wires to pass from the outside ESP32 to the inside lock/sensor
            translate([-1, depth/2, height - 20])
                rotate([0, 90, 0])
                cylinder(d=20, h=wall+2, $fn=32);
        }
        
        // The metal 12V Latching Solenoid will be mounted on the lid now!
        
        // Latch Catch for Lid-Mounted Solenoid
        // This block catches the slanted tongue when it springs out!
        translate([width/2 - 15, wall, height - 25]) {
            difference() {
                union() {
                    // Solid catch block attached to the front wall
                    cube([30, 11, 25]);
                    // 45-degree wedge underneath so it prints perfectly without supports!
                    hull() {
                        cube([30, 11, 0.1]);
                        translate([0, 0, -11]) cube([30, 0.1, 0.1]);
                    }
                }
                // Rectangular hole for the tongue to snap into
                translate([10, -1, 7]) cube([10, 13, 12]);
            }
        }
            
        // Box Hinges (Back wall, top edge)
        // Overlap perfectly with the back wall
        translate([width/4 - hinge_w/2, depth, height]) rotate([0, 90, 0]) hinge_knuckle();
        translate([3*width/4 - hinge_w/2, depth, height]) rotate([0, 90, 0]) hinge_knuckle();
        
        // Push Button Pocket (Limit Switch)
        // A U-shaped pocket that bridges perfectly when printed upside down!
        // Gives a 13x12mm recessed platform so a 12mm button sticks up exactly 1mm to be pressed by the lid.
        // Moved to the back wall so the heavy lid acts as a lever and easily depresses it!
        translate([20, depth - wall - 12, height - 14]) {
            cube([17, 12, 3]); // The shelf (bridges the gap)
            translate([0, 0, 3]) cube([2, 12, 11]); // Left pocket wall
            translate([15, 0, 3]) cube([2, 12, 11]); // Right pocket wall
        }
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
            
        // Lid Hinges (Back wall)
        translate([width/4 + hinge_w/2 + 0.5, depth, 0]) rotate([0, 90, 0]) hinge_knuckle();
        translate([width/4 - hinge_w*1.5 - 0.5, depth, 0]) rotate([0, 90, 0]) hinge_knuckle();
        
        translate([3*width/4 + hinge_w/2 + 0.5, depth, 0]) rotate([0, 90, 0]) hinge_knuckle();
        translate([3*width/4 - hinge_w*1.5 - 0.5, depth, 0]) rotate([0, 90, 0]) hinge_knuckle();
        
        // Solenoid Mounting Block (Lowers the solenoid for a stronger catch)
        translate([width/2 - 15, wall + 6, -5]) cube([30, 35, 5]);
    }
}

// --- Render Setup ---
main_box();

// Move lid to the side for printing
translate([0, depth + 15, 0])
    lid();
