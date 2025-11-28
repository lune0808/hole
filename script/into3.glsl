// include src/script_include.glsl

vec4 quat(vec3 axis, float angle)
{
	return vec4(sin(0.5 * angle) * axis, cos(0.5 * angle));
}

void init()
{
	beg.q_orientation = vec4(Y, 0.0);
	end.q_orientation = vec4(Y, +PI/2.0);

	beg.cam_pos = -1e-0*X - 2.0*X;
	end.cam_pos = -1e-3*X - 2.0*X;

	beg.r_s = 2.0;
	end.r_s = 2.0;

	beg.sphere_pos = 0.0*X;
	end.sphere_pos = 0.0*X;

	beg.dt = 0.050;
	end.dt = 0.100;

	beg.iterations = 2048;
	end.iterations = 32768;

	win.screen_width = 1600;
	win.screen_height = 900;
	win.n_frames = 256;
	win.ms_per_frame = 40;
	win.skybox_id = SKYBOX_GENERIC;
	win.fov = PI/2.5;

	vec3 accr_y = normalize(vec3(8.0, 4.0, -2.0));
	vec3 accr_x = normalize(cross(accr_y, X));
	scene.accr_normal = accr_y;
	scene.accr_x = accr_x;
	scene.accr_z = normalize(cross(accr_x, accr_y));
	scene.accr_min_r = 4.5;
	scene.accr_max_r = 40.0;
	scene.accr_height = 32.0;
	scene.accr_light = 80.0;
	scene.accr_light2 = 0.90;
	scene.accr_abso = 0.08;

	scene.red_exponent = 2.0;
	scene.green_exponent = 4.0;
	scene.blue_exponent = 3.0;
}

vec4 quat_mul(vec4 a, vec4 b)
{
	vec3 va = a.xyz;
	vec3 vb = b.xyz;
	return vec4(a.w*vb + b.w*va + cross(va, vb), a.w*b.w - dot(va, vb));
}

void loop()
{
	float p = mix(0.0, 1.0, progress);
	scene.q_orientation = normalize(quat_mul(
		quat_mul(quat(Z, -PI/8.0), quat(Y, PI/8.0)),
		quat(Y, mix(beg.q_orientation.w, end.q_orientation.w, pow(p, 0.2)))
	));
	scene.cam_pos = mix(beg.cam_pos, end.cam_pos, pow(p, 0.03));
	scene.sch_radius = mix(beg.r_s, end.r_s, p);
	scene.sphere_pos = mix(beg.sphere_pos, end.sphere_pos, p);
	scene.iterations = uint(mix(beg.iterations, end.iterations, p));
	scene.dt = mix(beg.dt, end.dt, p);
}


