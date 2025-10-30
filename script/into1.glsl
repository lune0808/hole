// include src/script_include.glsl

vec4 quat(vec3 axis, float angle)
{
	return vec4(sin(0.5 * angle) * axis, cos(0.5 * angle));
}

void init()
{
	beg.q_orientation = vec4(Y, 0.0);
	end.q_orientation = vec4(Y, PI/2.0);

	beg.cam_pos = +64.0*Z;
	end.cam_pos = +1e-3*Z;

	beg.r_s = 2.0;
	end.r_s = 2.0;

	beg.sphere_pos = 2.0*X;
	end.sphere_pos = 2.0*X;

	beg.dt = 1.00;
	end.dt = 0.05;

	beg.iterations = 512;
	end.iterations = 4096;

	win.screen_width = 800;
	win.screen_height = 600;
	win.n_frames = 256;
	win.ms_per_frame = 33;
	win.skybox_id = SKYBOX_GENERIC;
	win.fov = PI/3.0;

	vec3 accr_y = normalize(vec3(0.1, 0.9, -0.1));
	vec3 accr_x = normalize(cross(accr_y, X));
	scene.accr_normal = accr_y;
	scene.accr_x = accr_x;
	scene.accr_z = normalize(cross(accr_x, accr_y));
	scene.accr_min_r = 4.5;
	scene.accr_max_r = 35.0;
	scene.accr_height = 1.8;
	scene.accr_light = 2.0;
	scene.accr_light2 = 0.85;
	scene.accr_abso = 0.55;

	scene.red_exponent   = 1.10;
	scene.green_exponent = 1.09;
	scene.blue_exponent  = 3.80;
}

void loop()
{
	vec4 i = mix(beg.q_orientation, end.q_orientation, pow(progress, 8.0));
	scene.q_orientation = normalize(quat(i.xyz, i.w));
	scene.cam_pos = mix(beg.cam_pos, end.cam_pos, progress);
	scene.sch_radius = mix(beg.r_s, end.r_s, progress);
	scene.sphere_pos = mix(beg.sphere_pos, end.sphere_pos, progress);
	scene.iterations = uint(mix(beg.iterations, end.iterations, progress));
	scene.dt = mix(beg.dt, end.dt, progress);
}


