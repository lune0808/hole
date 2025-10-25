#version 450 core

struct scene_state {
	vec4 q_orientation;

	vec3 cam_pos;
	float sch_radius;

	vec3 sphere_pos;
	float focal_length;

	uint iterations;
	float dt;
	float inv_screen_width;
	float accr_light;

	vec3 accr_normal;
	float accr_height;

	vec3 accr_x;
	float accr_min_r;

	vec3 accr_z;
	float accr_max_r;

	float accr_abso;
	float accr_light2;
	float red_exponent;
	float green_exponent;

	float blue_exponent;
};

#define PI 3.1415927
