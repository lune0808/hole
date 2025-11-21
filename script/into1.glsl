// include src/script_include.glsl

vec4 quat(vec3 axis, float angle)
{
	return vec4(sin(0.5 * angle) * axis, cos(0.5 * angle));
}

void init()
{
	beg.q_orientation = vec4(Y, 0.0);
	end.q_orientation = vec4(Y, PI/2.0);

	beg.cam_pos = -3e-2*X + +128.0*Z + 15.0*Y;
	end.cam_pos = -3e-2*X;

	beg.r_s = 2.0;
	end.r_s = 2.0;

	beg.sphere_pos = 2.0*X;
	end.sphere_pos = 2.0*X;

	beg.dt = 0.80;
	end.dt = 0.10;

	beg.iterations = 512;
	end.iterations = 2048;

	win.screen_width = 1280;
	win.screen_height = 720;
	win.n_frames = 1024;
	win.ms_per_frame = 25;
	win.skybox_id = SKYBOX_GENERIC;
	win.fov = PI/3.0;

	scene.accr_hide = true;
	scene.red_exponent = 1.0;
	scene.green_exponent = 1.0;
	scene.blue_exponent = 1.0;
}

void loop()
{
	float p = mix(0.0, 1.0, progress);
	vec4 i = mix(beg.q_orientation, end.q_orientation, pow(p, 20.0));
	scene.q_orientation = normalize(quat(i.xyz, i.w));
	scene.cam_pos = mix(beg.cam_pos, end.cam_pos, p);
	scene.sch_radius = mix(beg.r_s, end.r_s, p);
	scene.sphere_pos = mix(beg.sphere_pos, end.sphere_pos, p);
	scene.iterations = uint(mix(beg.iterations, end.iterations, p));
	scene.dt = mix(beg.dt, end.dt, p);
}


