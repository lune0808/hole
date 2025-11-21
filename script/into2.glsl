// include src/script_include.glsl

vec4 quat(vec3 axis, float angle)
{
	return vec4(sin(0.5 * angle) * axis, cos(0.5 * angle));
}

void init()
{
	beg.q_orientation = quat(X, +PI/2.0);
	end.q_orientation = quat(X, -PI/2.0);

	beg.r_s = 2.0;
	end.r_s = 2.0;

	beg.sphere_pos = 0.0*X;
	end.sphere_pos = 0.0*X;

	beg.dt = 0.030;
	end.dt = 0.020;

	beg.iterations = 4096;
	end.iterations = 16384;

	win.screen_width = 240;
	win.screen_height = 240;
	win.n_frames = 512;
	win.ms_per_frame = 100;
	win.skybox_id = SKYBOX_GENERIC;
	win.fov = PI/3.0;

	vec3 accr_y = normalize(vec3(3.0, 5.0, -2.0));
	vec3 accr_x = normalize(cross(accr_y, X));
	scene.accr_normal = accr_y;
	scene.accr_x = accr_x;
	scene.accr_z = normalize(cross(accr_x, accr_y));
	scene.accr_min_r = 4.5;
	scene.accr_max_r = 40.0;
	scene.accr_height = 32.0;
	scene.accr_light = 64.0;
	scene.accr_light2 = 0.90;
	scene.accr_abso = 0.08;

	scene.red_exponent = 1.0;
	scene.green_exponent = 8.0;
	scene.blue_exponent = 7.0;
}

void loop()
{
	float p = mix(0.0, 1.0, progress);
	vec4 i = mix(beg.q_orientation, end.q_orientation, pow(p, 1.0));
	// scene.q_orientation = normalize(quat(i.xyz, i.w));
	scene.q_orientation = normalize(i);
	// scene.cam_pos = mix(beg.cam_pos, end.cam_pos, p);
	float s = sin(+PI/2 - p*PI);
	float c = cos(+PI/2 - p*PI);
	scene.cam_pos = mix(2.10, 2.035, p) * vec3(0.0, s, -c);
	scene.sch_radius = mix(beg.r_s, end.r_s, p);
	scene.sphere_pos = mix(beg.sphere_pos, end.sphere_pos, p);
	scene.iterations = uint(mix(beg.iterations, end.iterations, p));
	scene.dt = mix(beg.dt, end.dt, p);
}


