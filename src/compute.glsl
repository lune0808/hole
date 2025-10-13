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
	float sch_radius;
	uint iterations;
};
uniform layout(location=2) samplerCube skybox;

const vec3 light_pos = vec3(-1.0, 2.0, 1.0);
const vec3 sphere_pos = vec3(0.5, 0.0, -1.0);
const float sphere_r = 0.2;

void ray_accel(float r, float b, float dr_dt, out float dphi_dt, out float d2r_dt2)
{
	float rho = 1.0 - sch_radius / r;
	dphi_dt = b / (r*r) * rho;
	// d2r_dt2 = -sch_radius / b * dphi_dt + (r - 2.5 * sch_radius) * dphi_dt * dphi_dt;
	d2r_dt2 = dr_dt*dr_dt * sch_radius / (rho * r*r) + (rho*r - 0.5*sch_radius) * dphi_dt*dphi_dt;
}

vec3 rotate_quat(vec4 q, vec3 v)
{
	return v + 2.0 * cross(cross(v, q.xyz) + q.w * v, q.xyz);
}

vec4 color(ivec2 coord)
{
	vec2 pixel = vec2(coord.x * inv_screen_width - 0.5, coord.y * inv_screen_width - 0.5);
	// if fov = pi/3, focal length = root(3)/2 = 0.866, and camera faces -z
	vec3 start_ray = normalize(vec3(pixel, -0.866));
	vec3 ray = start_ray;
	vec3 pos = cam_pos;
	float hit = 0.0;
	vec3 start_radial = pos - sphere_pos;
	vec3 start_radial_n = normalize(start_radial);
	vec3 orbital_axis = normalize(cross(start_radial, ray)); float r = length(start_radial);
	if (r <= sch_radius) {
		return vec4(0.0);
	}
	float phi = 0;
	float dr_dt = dot(ray, start_radial_n);
	vec3 start_angular_n = cross(orbital_axis, start_radial_n);
	float dphi_dt = 1.0 / r * dot(ray - dr_dt * start_radial_n, start_angular_n);
	float b = r * r * dphi_dt / (1.0 - sch_radius/r);
	// float sgn = sign(dot(ray, start_radial));
	// float dr_dt = sgn * (sch_radius/r - 1.0) * sqrt(1.0 + b*b / (r*r) * (sch_radius/r - 1.0);
	vec3 output_color = vec3(0.0);
	const float dt = 5e-3;
	float rmax = max(sch_radius, sphere_r);
	for (uint iter = 0; iter < iterations; iter++) {
		if (abs(r) < rmax) {
			hit = 1.0;
			output_color = vec3(1.0, 0.0, 0.0);
			break;
		}
		float dr2_dt2;
		ray_accel(r, b, dr_dt, dphi_dt, dr2_dt2);
		r     += dt * dr_dt;
		dr_dt += dt * dr2_dt2;
		phi   += dt * dphi_dt;
	}
	float s = sin(phi * 0.5);
	float c = cos(phi * 0.5);
	vec4 q = vec4(s * orbital_axis, c);
	vec3 end_radial  = rotate_quat(q, start_radial_n );
	vec3 end_angular = rotate_quat(q, start_angular_n);
	pos = sphere_pos + r * end_radial;
	ray = dr_dt * end_radial + r * dphi_dt * end_angular;
	float d = dot(normalize(ray), start_ray);
	// d = dot(normalize(pos - cam_pos), start_ray);
	// d = dot(normalize(ray), start_ray);
	// return vec4((normalize(ray)-start_ray), 1.0);
	// return vec4(vec3(phi/1.57, phi/3.14, phi/6.28).rrr, 1.0);
	// return vec4(output_color, 1.0);
	return vec4(vec3(hit), 1.0);
	// return vec4(vec3(0.0, 1.0-d, d), 1.0);
	return vec4(hit * output_color + (1.0-hit) * vec3(0.0, 1.0-d, d), 1.0);
	// return vec4(hit * output_color + (1.0-hit) * texture(skybox, ray).rgb, 1.0);
}

void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	imageStore(screen, coord, color(coord));
}

