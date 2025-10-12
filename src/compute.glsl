#version 430 core

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
uniform layout(binding=0,rgba32f) writeonly restrict image2D screen;
layout(std430,binding=1) readonly restrict buffer scene
{
	vec3 cam_right;
	float inv_screen_width;
	vec3 cam_up;
	float focal_length;
	vec3 cam_pos;
};
uniform layout(location=2) samplerCube skybox;

const vec3 light_pos = vec3(-1.0, 2.0, 1.0);
const vec3 sphere_pos = vec3(0.5, 0.0, -1.0);
const float sphere_r = 0.2;

const float dl = 0.1;
const uint iterations = 100;

vec4 color(ivec2 coord)
{
	vec2 pixel = vec2(coord.x * inv_screen_width - 0.5, coord.y * inv_screen_width - 0.5);
	// if fov = pi/3, focal length = root(3)/2 = 0.866, and camera faces -z
	vec3 ray = normalize(vec3(pixel, -0.866));
	vec3 pos = cam_pos;
	float hit = 0.0;
	for (uint iter = 0; iter < iterations; iter++) {
		vec3 diff = sphere_pos - pos;
		if (dot(diff, diff) <= sphere_r*sphere_r) {
			hit = 1.0;
			break;
		}
		vec3 dr_over_dl = 0.007 / dot(diff, diff) * normalize(diff);
		ray = normalize(ray + dl * dr_over_dl);
		pos = pos + dl * ray;
	}
	return vec4(hit * vec3(0.3, 0.1, 0.03) + (1.0-hit) * texture(skybox, ray).rgb, 1.0);
}

void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	imageStore(screen, coord, color(coord));
}

