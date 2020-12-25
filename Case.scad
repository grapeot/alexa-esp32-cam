total_size = [ 50, 30, 25 ];
wall_depth = [ 2, 2, 2 ];
screen_hole_size = [24, 36, 10];
slider_gap_depth = 1;
epsilon = 0.01;
margin = 0.8;

module case(with_antenna_hole=false) {
    $fn=100;

    // cube body
    translate([0, 0, (total_size[2] + wall_depth[2]) / 2])
    rotate([180, 0, 0])
    difference() {
        cube(total_size + wall_depth * 2, center=true);
        // center hole
        cube(total_size, center=true);
        // slider gap
        translate([ wall_depth[0] / 2 + epsilon / 2, 0, -total_size[2] / 2 + wall_depth[2] / 2 + epsilon / 2 ])
        cube([ total_size[0] + wall_depth[0] + epsilon, total_size[1] + 2 * slider_gap_depth + epsilon, slider_gap_depth ], center=true);
        // remove the bottom 
        translate([ 0, 0, -total_size[2] / 2 - wall_depth[2] / 2 - epsilon ])
        cube([ total_size[0], total_size[1], wall_depth[2] + 3 * epsilon], center=true);
        // remove the edge which might cause the bridging issue
        translate([ total_size[0] / 2 + wall_depth[0] / 2 + epsilon / 2, 0, -total_size[2] / 2 - wall_depth[2] / 2 - epsilon ])
        cube([ wall_depth[0] + 2 * epsilon, total_size[1] + epsilon, wall_depth[2] * 2 + 3 * epsilon], center=true);
        // remove the hole for the micro USB
        // translate([-total_size[0] / 2, 0, -5])
        // cube([10, 13, 7], center=true);
        // remove the hole for the camera
        translate([10, 0, (total_size[2] + wall_depth[2]) / 2]) 
        // cylinder(r=8/2, h=10, center=true);
        cube([8.5, 8.5, 10], center=true);

        if (with_antenna_hole) {
            translate([total_size[0]/2, total_size[1]/2-5, 0])
            rotate([0, 90, 0])
            cylinder(r=6.5/2, h=10, center=true);
        }
    }
}

module slider() {
    // slider
    translate([ -40, 50, 0 ])
    cube([ total_size[0] + wall_depth[0] - margin - 10, total_size[1] + 2 * slider_gap_depth - margin, slider_gap_depth - 0.2 ]);
}

case(true);
slider();