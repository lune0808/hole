// include src/shared_data.glsl
// include inc/skybox_id.hpp

struct scene_settings {
	vec4 q_orientation;

	vec3 cam_pos;
	float r_s;

	vec3 sphere_pos;
	float dt;

	float iterations;
};

struct window_settings {
	int screen_width;
	int screen_height;
	uint n_frames;
	uint ms_per_frame;

	uint skybox_id;
	float fov;
};

uniform layout(location=2) float progress;

layout(std430,binding=0) restrict buffer scene_settings_buf
{
	scene_settings beg;
	scene_settings end;
	window_settings win;
};

layout(std430,binding=1) writeonly restrict buffer scene_spec
{
	scene_state scene;
};

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

const vec3 X = vec3(1.0, 0.0, 0.0);
const vec3 Y = vec3(0.0, 1.0, 0.0);
const vec3 Z = vec3(0.0, 0.0, 1.0);

