// Smart Delivery Bin - Full Mechanical & Electronic Animation
// Open this file in OpenSCAD and click: View -> Animate
// Set FPS: 30, Steps: 100

/* --- Parameters --- */
width = 200; depth = 150; height = 150; wall = 3;
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
            // Inner hollow cavity (Starts at Z=-1 to remove the floor!)
            translate([wall, wall, -1]) cube([width - 2*wall, depth - 2*wall, height + 2]); 
            translate([width/2 - slit_w/2, -1, slit_z]) cube([slit_w, wall+2, slit_h]);
            // Wire Routing Hole (Moved near the top left wall to avoid the breadboard)
            translate([-1, depth/2, height - 20]) rotate([0, 90, 0]) cylinder(d=20, h=wall+2, $fn=32);
            
            // Delete the entire right wall for animation visibility!
            translate([width - wall - 1, -1, -1]) cube([wall + 3, depth + 2, height + 2]);
        }
        translate([width/4 - hinge_w/2, depth, height]) difference() {
            translate([0, -hinge_d/2 + 0.5, -hinge_d/2]) cube([hinge_w, hinge_d/2, hinge_d]);
            rotate([0, 90, 0]) cylinder(d=hinge_d, h=hinge_w, $fn=32);
        }
        translate([3*width/4 - hinge_w/2, depth, height]) difference() {
            translate([0, -hinge_d/2 + 0.5, -hinge_d/2]) cube([hinge_w, hinge_d/2, hinge_d]);
            rotate([0, 90, 0]) cylinder(d=hinge_d, h=hinge_w, $fn=32);
        }
        
        // Push Button Pocket (Limit Switch)
        translate([20, wall, height - 14]) {
            cube([17, 12, 3]); 
            translate([0, 0, 3]) cube([2, 12, 11]); 
            translate([15, 0, 3]) cube([2, 12, 11]); 
        }
    }
    
    // Virtual Electronic Components attached to the box
    translate([-2, depth/2 + 20, height/2]) rotate([0, -90, 0]) translate([-82/2, -55/2, 0]) dummy_esp32();
    translate([-1.5, depth/2 - 30, height/2]) rotate([0, -90, 0]) translate([-43/2, -17/2, 0]) dummy_relay();
    
    // OLED screen is INSIDE the box (per user request)
    translate([-1.5, depth/2 - 60, height/2]) rotate([0, -90, 0]) translate([-27/2, -27/2, 0]) dummy_oled();
    translate([width/2, -1, slit_z - 35]) rotate([90, 0, 0]) translate([-77/2, -70/2, 0]) dummy_keypad();
    
    // The Solenoid Lock (Lowered by 5mm to prevent its metal flange from smashing into the lid!)
    translate([width/2 + 2, wall, height - 26.5]) dummy_solenoid(tongue_x);
    
    // The Limit Switch (Push Button)
    // The cap mathematically compresses by up to 1mm exactly when the heavy lid swings down onto it
    button_pressed = max(0, 1 - 146 * sin(angle));
    translate([23.5, wall + 1, height - 11]) dummy_pushbutton(button_pressed);
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
        translate([width/4 + hinge_w/2 + 0.5, depth, 0]) rotate([0, 90, 0]) hinge_knuckle();
        translate([width/4 - hinge_w*1.5 - 0.5, depth, 0]) rotate([0, 90, 0]) hinge_knuckle();
        translate([3*width/4 + hinge_w/2 + 0.5, depth, 0]) rotate([0, 90, 0]) hinge_knuckle();
        translate([3*width/4 - hinge_w*1.5 - 0.5, depth, 0]) rotate([0, 90, 0]) hinge_knuckle();
        
        // Latch Hook (Shifted right in CAD, so when the lid flips over, it ends up on the left!)
        translate([width/2, wall + 0.5, wall - 1]) {
            difference() {
                // Elongated to 31mm so it reaches the lowered Solenoid lock
                cube([20, 10, 31]);
                // Hole must be open on the LEFT in CAD, so it is open on the RIGHT when flipped!
                translate([-1, 2, 14]) cube([17, 6, 12]); 
            }
        }
    }
    
    // Virtual HC-SR04 Sensor attached to the lid
    // Pointing INTO the box (+Z in local lid space, which becomes DOWN when closed)
    translate([width/2, depth/2, wall]) translate([-45/2, -10, 0]) dummy_hcsr04();
}

// --- Render ---
main_box();

translate([0, depth, height])
    rotate([-angle, 0, 0])
    translate([0, -depth, 0])
        translate([width, 0, 0]) rotate([0, 180, 0]) lid();
