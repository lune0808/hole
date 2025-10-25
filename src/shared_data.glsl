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
};

#define PI 3.1415927
