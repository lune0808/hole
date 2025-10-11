#version 430 core

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
uniform layout(binding=0,rgba32f) writeonly restrict image2D screen;
layout(std430,binding=1) readonly restrict buffer scene
{
	float screen_width;
};

const vec3 cam_pos = vec3(0.0, 0.0, 1.0);
const vec3 light_pos = vec3(-1.0, 2.0, 1.0);
const vec3 sphere_pos = vec3(0.5, 0.0, -1.0);
const float sphere_r = 0.5;

vec4 color(ivec2 coord)
{
	vec2 pixel = vec2(coord.x / screen_width - 0.5, coord.y / screen_width - 0.5);
	// if fov = pi/3, focal length = root(3)/2 = 0.866, and camera faces -z
	vec3 ray = normalize(vec3(pixel, -0.866));
	vec3 diff = sphere_pos - cam_pos;
	float proj = dot(ray, diff);
	float discr = proj * proj - (dot(diff, diff) - sphere_r * sphere_r);
	if (discr >= 0.0) {
		float t = proj - sqrt(discr);
		vec3 collision = cam_pos + t * ray;
		vec3 normal = (1.0 / sphere_r) * (collision - sphere_pos);
		vec3 incident = normalize(light_pos - collision);
		float diffuse = dot(normal, incident);
		return diffuse * vec4(0.8, 0.4, 0.3, 0.0) + vec4(0.1, 0.1, 0.1, 1.0);
	} else {
		return vec4(0.0);
	}
}

void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	imageStore(screen, coord, color(coord));
}

