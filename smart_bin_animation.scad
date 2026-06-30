// Smart Delivery Bin - Full Mechanical & Electronic Animation
// Open this file in OpenSCAD and click: View -> Animate
// Set FPS: 30, Steps: 100

/* --- Parameters --- */
width = 150;
depth = 110;
height = 110;
wall = 1.6;
oled_w = 26.5; oled_h = 14.5; oled_z = 90; 
slit_w = 25; slit_h = 2; slit_z = 75; 
solenoid_w = 27; solenoid_d = 15;
sensor_hole_d = 16; sensor_spacing = 25.5;
hinge_r = 5; hinge_hole_r = 1.5; hinge_w = 15;
hinge_d = 10;

// --- Animation Math ---
angle = 110 * (0.5 - 0.5 * cos($t * 360));
// Tongue only retracts precisely when the hook physically touches it (angle 6.6 to 0.8)
tongue_x = (angle <= 6.6 && angle > 0.8) ? 10 * ((6.6 - angle) / 5.8) : 
           (angle <= 0.8 && angle > 0) ? angle * 12.5 : 0;

/* --- Dummy Electronic Components --- */
module dummy_esp32() {
    // Half-size Breadboard (82 x 55 x 8.5)
    color("White") translate([0, 0, -8.5]) cube([82, 55, 8.5]); 
    
    // ESP32 Dev Board
    color("black") translate([15, (55-28)/2, 0]) cube([52, 28, 2]); 
    color("silver") translate([15 + 5, (55-28)/2 + 5, 2]) cube([15, 16, 3]); 
    
    // MB102 Breadboard Power Supply
    color("Black") translate([-20, (55-53)/2, 0]) cube([32, 53, 1.5]); // PCB
    color("Black") translate([-22, 55/2 - 4.5, 1.5]) cube([15, 9, 11]); // Barrel Jack
    color("Silver") translate([-5, 55/2 - 7, 1.5]) cube([14, 14, 6]); // USB Port
    color("White") translate([-10, 55/2 + 10, 1.5]) cube([6, 6, 6]); // Power Button
}

module dummy_relay() {
    color("Navy") cube([43, 17, 1.5]); 
    color("blue") translate([5, 1, 1.5]) cube([15, 12, 12]); 
    color("green") translate([25, 1, 1.5]) cube([10, 8, 8]); 
}

module dummy_oled() {
    color("Navy") cube([27, 27, 1.5]); 
    color("black") translate([(27-oled_w)/2, (27-oled_h)/2, -2]) cube([oled_w, oled_h, 2.1]); 
}

module dummy_keypad() {
    color("Black") cube([77, 70, 1]);
    color("White") translate([0, -35, 0]) cube([20, 35, 0.5]); 
}

module dummy_pushbutton(pressed) {
    color("Black") cube([10, 10, 5]); // Base
    color("Silver") {
        translate([-1, 2, 0]) cube([12, 1, 1]); // Pins
        translate([-1, 7, 0]) cube([12, 1, 1]);
    }
    // The yellow cap compresses dynamically based on the lid pressing it!
    color("Yellow") translate([5, 5, 5]) cylinder(d=6, h=7 - pressed, $fn=16); 
}

module dummy_hcsr04() {
    color("Navy") cube([45, 20, 1.5]); 
    color("silver") translate([45/2 - sensor_spacing/2, 10, 1.5]) cylinder(d=16, h=10, $fn=32); 
    color("silver") translate([45/2 + sensor_spacing/2, 10, 1.5]) cylinder(d=16, h=10, $fn=32); 
}

// HORIZONTAL Solenoid Lock
module dummy_solenoid(tx) {
    color("silver") {
        cube([27, 15, 15]); // Horizontal Body
        translate([0, 0, -7]) cube([27, 2, 29]); // Side flanges
        // Tongue pointing LEFT (-X)
        // Retracts into the body to the RIGHT (+X)
        translate([-10 + tx, 5, 15/2 - 5]) cube([10, 5, 10]); 
    }
}

/* --- Mechanical Parts --- */
module hinge_knuckle() {
    color("Orange")
    difference() {
        cylinder(r=hinge_r, h=hinge_w, $fn=32);
        translate([0,0,-1]) cylinder(r=hinge_hole_r, h=hinge_w+2, $fn=32);
    }
}

module main_box() {
    color("Goldenrod")
    union() {
        difference() {
            cube([width, depth, height]);
            // Inner hollow cavity
            translate([wall, wall, wall]) cube([width - 2*wall, depth - 2*wall, height]); 
            translate([width/2 - slit_w/2, -1, slit_z]) cube([slit_w, wall+2, slit_h]);
            // Wire Routing Hole (Moved near the top left wall to avoid the breadboard)
            translate([-1, depth/2, height - 20]) rotate([0, 90, 0]) cylinder(d=20, h=wall+2, $fn=32);
            
            // Delete the entire right wall for animation visibility!
            translate([width - wall - 1, -1, -1]) cube([wall + 3, depth + 2, height + 2]);
        }
        
        // Box Hinges
        translate([width/4 - hinge_w/2, depth + 3, height - 3]) rotate([0, 90, 0]) hinge_knuckle();
        translate([3*width/4 - hinge_w/2, depth + 3, height - 3]) rotate([0, 90, 0]) hinge_knuckle();
        
        // Push Button Pocket (Limit Switch)
        translate([20, depth - wall - 12, height - 14]) {
            cube([17, 12, 3]); 
            translate([0, 0, 3]) cube([2, 12, 11]); 
            translate([15, 0, 3]) cube([2, 12, 11]); 
        }
        
        // Latch Catch for Lid-Mounted Solenoid
        translate([width/2 - 15, wall, height - 19]) {
            difference() {
                union() {
                    cube([30, 11, 19]);
                    hull() {
                        cube([30, 11, 0.1]);
                        translate([0, 0, -11]) cube([30, 0.1, 0.1]);
                    }
                }
                translate([10, -1, 1]) cube([10, 13, 12]);
            }
        }
    }
    
    // Virtual Electronic Components attached to the box
    translate([-2, depth/2 + 20, height/2]) rotate([0, -90, 0]) translate([-82/2, -55/2, 0]) dummy_esp32();
    translate([-1.5, depth/2 - 30, height/2]) rotate([0, -90, 0]) translate([-43/2, -17/2, 0]) dummy_relay();
    
    // OLED screen is INSIDE the box (per user request)
    translate([-1.5, depth/2 - 60, height/2]) rotate([0, -90, 0]) translate([-27/2, -27/2, 0]) dummy_oled();
    translate([width/2, -1, slit_z - 35]) rotate([90, 0, 0]) translate([-77/2, -70/2, 0]) dummy_keypad();
    
    // The Limit Switch (Push Button)
    // The cap mathematically compresses by up to 1mm exactly when the heavy lid swings down onto it
    button_pressed = max(0, 1 - 10 * sin(angle));
    translate([23.5, depth - wall - 11, height - 11]) dummy_pushbutton(button_pressed);
}

module lid() {
    color("Gold")
    union() {
        difference() {
            cube([width, depth, wall]);
        }
        translate([wall + 0.5, wall + 0.5, wall - 0.1])
            difference() {
                cube([width - 2*wall - 1, depth - 2*wall - 1, 5.1]);
                translate([wall, wall, -1]) cube([width - 4*wall - 1, depth - 4*wall - 1, 7.1]); 
            }
            
        translate([width/4 + hinge_w/2 + 0.5, depth + 3, 5]) rotate([0, 90, 0]) hinge_knuckle();
        translate([width/4 - hinge_w*1.5 - 0.5, depth + 3, 5]) rotate([0, 90, 0]) hinge_knuckle();
        translate([3*width/4 + hinge_w/2 + 0.5, depth + 3, 5]) rotate([0, 90, 0]) hinge_knuckle();
        translate([3*width/4 - hinge_w*1.5 - 0.5, depth + 3, 5]) rotate([0, 90, 0]) hinge_knuckle();
        
        // Solenoid Mounting Block
        translate([width/2 - 15, wall + 6, wall]) cube([30, 35, 5]);
    }
    
    // The actual Solenoid Lock (now mounted to the bottom of the lid!)
    translate([width/2 + 2.5, wall + 12, wall + 5]) rotate([0, 0, 90]) dummy_solenoid(tongue_x);
    
    // Virtual HC-SR04 Sensor attached to the lid
    translate([width/2, depth/2, wall]) translate([-45/2, -10, 0]) dummy_hcsr04();
}

// --- Render ---
main_box();

translate([0, depth + 3, height - 3])
    rotate([-angle, 0, 0])
    translate([0, -(depth + 3), 5])
        translate([width, 0, 0]) rotate([0, 180, 0]) lid();
