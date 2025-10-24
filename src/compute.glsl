#version 430 core

layout(local_size_x = 4, local_size_y = 4, local_size_z = 1) in;
uniform layout(binding=0,r11f_g11f_b10f) writeonly restrict image2D screen;
layout(std430,binding=1) readonly restrict buffer scene
{
	vec4 q_orientation;
	vec3 cam_pos;
	float inv_screen_width;
	vec3 sphere_pos;
	float focal_length;
	float sch_radius;
	uint iterations;
};
uniform layout(location=2) samplerCube skybox;

const vec3 light_pos = vec3(-1.0, 2.0, 1.0);

void ray_accel(float r, float b, float dr_dt, out float dphi_dt, out float d2r_dt2)
{
	float rho = 1.0 - sch_radius / r;
	float rm2 = 1.0f / (r * r);
	dphi_dt = b * rm2 * rho;
	d2r_dt2 = sch_radius * rho * rm2 * (1.0 - b * b * rho * rm2)
		+ (r * rho - 0.5 * sch_radius) * dphi_dt * dphi_dt;
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

const float inv6 = 1.0 / 6.0;

vec3 rk4(vec3 y, float b, float h)
{
	vec3 k1 = differentiate(b, y             );
	vec3 k2 = differentiate(b, y + 0.5*h * k1);
	vec3 k3 = differentiate(b, y + 0.5*h * k2);
	vec3 k4 = differentiate(b, y +     h * k3);
	return y + h * inv6 * (k1 + 2.0*k2 + 2.0*k3 + k4);
}

const float accretion_min = 4.5f;
const float accretion_max = 35.0f;
const vec3 accretion_normal = normalize(vec3(0.1, 0.9, -0.1));
const vec3 X = vec3(1.0, 0.0, 0.0);
const vec3 accretion_x = normalize(cross(accretion_normal, X));
const vec3 accretion_z = normalize(cross(accretion_x, accretion_normal));
const float accretion_height = 3.5;

// Taken from http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl.
vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void integrate_intensity(float r, float phi, float y, inout float i, inout float transmittance, float h)
{
	if (r < accretion_min || r > accretion_max || abs(y) > accretion_height) {
		return;
	}
	const float r0 = -1.0 * accretion_min;
	const float y0 = accretion_height / ((accretion_max - r0) * (accretion_max - r0));
	float y_bound = y0 * (r - r0) * (r - r0);
	float y_modulate = 1.0 - smoothstep(0.0, y_bound*y_bound, y*y);
	const float two_pi = 6.2831853;
	// i = mod(phi, two_pi) / two_pi;
	float r_modulate = 1.0 - smoothstep(accretion_min, accretion_max, r);
	float in_disk = y_modulate * r_modulate;
	float l1 = 0.65;
	float l0 = l1 * (1.0 - accretion_min * 0.85 / r);
	float a = 1.0;
	float density = in_disk * r_modulate * a * a;
	float local_light = density * r_modulate / l0;
	float local_absorbancy = density * 4.0;
	i += h * transmittance * local_light;
	transmittance *= exp(h * -local_absorbancy);
	// t*exp(x) = t*(1+x+o(x))
	// transmittance += h * -local_absorbancy * transmittance;
}

vec3 rodrigues_formula(vec3 axis, float sina, float cosa, vec3 v)
{
	return cosa * v + sina * cross(axis, v) + (1.0 - cosa) * dot(axis, v) * axis;
}

vec3 rotate_axis(vec3 axis, float angle, vec3 v)
{
	return rodrigues_formula(axis, sin(angle), cos(angle), v);
}

float dt_scale(float x)
{
	return 20.0 * x;
	return 6.5;
	return min(5.0*x, x * x + 1.0);
}

vec3 light_shift(float intensity)
{
	float r0 = 1.0;
	float g0 = 3.0;
	float b0 = 9.0;
	float r = pow(intensity, r0);
	float g = pow(intensity, g0);
	float b = pow(intensity, b0);
	return vec3(r, g, b);
}

vec3 trace(vec3 start_ray)
{
	vec3 ray = start_ray;
	vec3 pos = cam_pos;
	vec3 start_radial = pos - sphere_pos;
	vec3 start_radial_n = normalize(start_radial);
	vec3 orbital_axis = normalize(cross(start_radial, ray));
	float r = length(start_radial);
	// no matter how you integrate, from an observer,
	// nothing reaches the event horizon
	// so we flush photons who are orbiting too close
	// inside, which also reduces the repetitions we see
	// in the photon sphere
	float r_limit = sch_radius + 1e-4;
	if (r <= r_limit) {
		return vec3(0.0);
	}
	float phi = 0;

	// we know where the ray is going towards,
	// but we don't know how much 'distance' it
	// travels during dt along that direction,
	// so we calculate initial conditions based
	// on the conservation of specific quantities
	// in a way that is independent of this length
	// with <l.u,ur>/<l.u,u0>=<u,ur>/<u,u0> (ray=l.u)
	vec3 start_angular_n = cross(orbital_axis, start_radial_n);
	float dev_radial = dot(ray, start_radial_n);
	float dev_angular = dot(ray, start_angular_n); // always >= 0
	float dphi_dt;
	float dr_dt;
	float rho = 1.0 - sch_radius / r;
	// compute dr/dphi or dphi/dr whichever doesn't blow up
	if (dev_angular > 0.707) {
		float looking_at = abs(dev_radial) / dev_angular;
		dphi_dt = inversesqrt(1.0 + (r * looking_at / rho) * (r * looking_at / rho));
		dr_dt = sign(dev_radial) * r * dphi_dt * looking_at;
	} else {
		float looking_at = dev_angular / abs(dev_radial);
		dr_dt = sign(dev_radial) * rho * inversesqrt(1.0 + (looking_at * rho / r) * (looking_at * rho / r));
		dphi_dt = abs(dr_dt) * looking_at / r;
	}

	float b = r * r * dphi_dt / rho;
	vec3 y = vec3(r, dr_dt, phi);
	float light = 0.0;
	float transmittance = 1.0;

	for (uint iter = 0; iter < iterations; iter++) {
		const float dt = dt_scale(y.x / sch_radius) / float(iterations);
		y = rk4(y, b, dt);
		r = y.x;
		if (abs(r) <= r_limit) {
			transmittance = 0.0;
			break;
		}
		if (r > max(accretion_max * 3.0, 10.0 * sch_radius) && y.y > 0.0) {
			break;
		}
		if (transmittance < 1e-4) {
			break;
		}
		phi = y.z;
		rho = 1.0 - sch_radius / r;
		float rm3 = 1.0f / (r * r * r);
		float ds = rho * dt * sqrt(1.0 + b * b * rm3 * sch_radius);
		vec3 radial = rotate_axis(orbital_axis, phi, start_radial_n);
		// world pos = mass origin + r * radial
		float disk_angle = atan(dot(radial, accretion_z), dot(radial, accretion_x));
		float ydisk = r * dot(radial, accretion_normal);
		integrate_intensity(r, disk_angle, ydisk, light, transmittance, ds);
	}

	r = y.x;
	dr_dt = y.y;
	phi = y.z;
	vec3 end_radial  = rotate_axis(orbital_axis, phi, start_radial_n);
	vec3 end_angular = cross(orbital_axis, end_radial);
	pos = sphere_pos + r * end_radial;
	float d2r_dt2;
	ray_accel(r, b, dr_dt, dphi_dt, d2r_dt2);
	ray = dr_dt * end_radial + r * dphi_dt * end_angular;
	vec3 sky = texture(skybox, ray).rgb;
	vec3 ambient = vec3(0.05);
	vec3 emission = light_shift(light);
	return ambient + transmittance * sky + emission;
}

vec3 rotate_quat(vec4 q, vec3 v)
{
	return v + 2.0 * cross(q.xyz, q.w * v + cross(q.xyz, v));
}

vec4 color(ivec2 coord)
{
	vec2 pixel = vec2(coord.x * inv_screen_width - 0.5, coord.y * inv_screen_width - 0.5);
	vec3 start_ray = normalize(rotate_quat(q_orientation, vec3(pixel, -focal_length)));
	return vec4(trace(start_ray), 1.0);
}

void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	imageStore(screen, coord, color(coord));
}

