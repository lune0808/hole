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
	d2r_dt2 = dr_dt*dr_dt * sch_radius / (rho * r*r) + (rho*r - 0.5*sch_radius) * dphi_dt*dphi_dt;
}

// y = (r, dr/dt, phi)
// dy/dt = (dr/dt, d2r/dt2, dphi/dt)
vec3 differentiate(float b, vec3 y)
{
	vec3 dy_dt;
	ray_accel(y.x, b, y.y, dy_dt.z, dy_dt.y);
	dy_dt.x = y.y;
	return dy_dt;
}

vec3 euler(vec3 y, float b, float h)
{
	vec3 dy_dt = differentiate(b, y);
	return y + h * dy_dt;
}

vec3 rk4(vec3 y, float b, float h)
{
	vec3 k1 = differentiate(b, y             );
	vec3 k2 = differentiate(b, y + 0.5*h * k1);
	vec3 k3 = differentiate(b, y + 0.5*h * k2);
	vec3 k4 = differentiate(b, y +     h * k3);
	float inv6 = 1.0 / 6.0;
	return y + h * inv6 * (k1 + 2.0*k2 + 2.0*k3 + k4);
}

vec3 rotate_quat(vec4 q, vec3 v)
{
	return v + 2.0 * cross(cross(v, q.xyz) + q.w * v, q.xyz);
}

vec3 rodrigues_formula(vec3 axis, float sina, float cosa, vec3 v)
{
	return cosa * v + sina * cross(axis, v) + (1.0 - cosa) * dot(axis, v) * axis;
}

vec3 rotate_axis(vec3 axis, float angle, vec3 v)
{
	return rodrigues_formula(axis, sin(angle), cos(angle), v);
}

void rotate_axes(vec3 axis, float angle, inout vec3 v1, inout vec3 v2)
{
	float sina = sin(angle);
	float cosa = cos(angle);
	v1 = rodrigues_formula(axis, sina, cosa, v1);
	v2 = rodrigues_formula(axis, sina, cosa, v2);
}

vec3 trace(vec3 start_ray)
{
	vec3 ray = start_ray;
	vec3 pos = cam_pos;
	float hit = 0.0;
	vec3 start_radial = pos - sphere_pos;
	vec3 start_radial_n = normalize(start_radial);
	vec3 orbital_axis = normalize(cross(start_radial, ray));
	float r = length(start_radial);
	if (r <= sch_radius) {
		return vec3(0.0);
	}
	float phi = 0;
	float dr_dt = dot(ray, start_radial_n);
	vec3 start_angular_n = cross(orbital_axis, start_radial_n);
	float dphi_dt = 1.0 / r * dot(ray - dr_dt * start_radial_n, start_angular_n);
	float b = r * r * dphi_dt / (1.0 - sch_radius/r);
	const float dt = 20e-3;
	vec3 y = vec3(r, dr_dt, phi);
	vec3 output_color = vec3(1.0);
	for (uint iter = 0; iter < iterations; iter++) {
		r = y.x;
		if (false) { if (r < sch_radius) {
			hit = 1.0;
			break;
		} else if (r < sphere_r) {
			hit = 1.0;
			float phi = y.z;
			dr_dt = y.y;
			dphi_dt = b / (r * r) * (1.0 - sch_radius / r);
			vec3 radial = start_radial_n;
			vec3 angular = start_angular_n;
			rotate_axes(orbital_axis, phi, radial, angular);
			ray = dr_dt * radial + r * dphi_dt * angular;
			ray = reflect(ray, radial);
			dr_dt = dot(ray, radial);
			y = vec3(sphere_r + 1e-6, dr_dt, phi);
			output_color *= vec3(0.9);
		} }
		y = rk4(y, b, dt);
	}
	r = y.x;
	dr_dt = y.y;
	phi = y.z;
	vec3 end_radial  = start_radial_n;
	vec3 end_angular = start_angular_n;
	rotate_axes(orbital_axis, phi, end_radial, end_angular);
	pos = sphere_pos + r * end_radial;
	ray = dr_dt * end_radial + r * dphi_dt * end_angular;
	vec3 sky = texture(skybox, ray).rgb;
	vec3 normal = normalize(ray);
	float diffuse = max(0.0, dot(normal, normalize(light_pos - pos)));
	vec3 ambient = vec3(0.03);
	// float cos_dev = dot(normal, start_ray);
	// return vec3(cos_dev);
	return ambient + hit * diffuse * output_color * sky + (1.0-hit) * sky;
}

vec4 color(ivec2 coord)
{
	vec2 pixel = vec2(coord.x * inv_screen_width - 0.5, coord.y * inv_screen_width - 0.5);
	// if fov = pi/3, focal length = root(3)/2 = 0.866, and camera faces -z
	vec3 start_ray = normalize(vec3(pixel, -0.866));
	return vec4(trace(start_ray), 1.0);
}

void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	imageStore(screen, coord, color(coord));
}

