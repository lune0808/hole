// include src/script_include.glsl

vec4 quat(vec3 axis, float angle)
{
	return vec4(sin(0.5 * angle) * axis, cos(0.5 * angle));
}

vec3 rotate_quat(vec4 q, vec3 v)
{
	return v + 2.0 * cross(q.xyz, q.w * v + cross(q.xyz, v));
}

const float dt = 0.20;

void init()
{
	beg.q_orientation = quat(Y, 0.0);
	end.q_orientation = quat(Y, PI/6.0);

	beg.cam_pos = -4.0*X + 32.0*Z + 8.0*Y;
	end.cam_pos = -4.0*X + 4.0*Z;

	beg.r_s = 2.0;
	end.r_s = 2.0;

	beg.sphere_pos = 2.0*X - 2.0*Z;
	end.sphere_pos = 2.0*X - 2.0*Z;

	beg.dt = dt;
	end.dt = dt;

	beg.iterations = 512;
	end.iterations = 512;

	win.screen_width = 1280;
	win.screen_height = 720;
	win.n_frames = 128;
	win.ms_per_frame = 33;
	win.skybox_id = SKYBOX_GENERIC;
	win.fov = PI/3.0;
}

void loop()
{
	scene.q_orientation = mix(beg.q_orientation, end.q_orientation, progress);
	scene.cam_pos = mix(beg.cam_pos, end.cam_pos, progress);
	scene.sch_radius = mix(beg.r_s, end.r_s, progress);
	scene.sphere_pos = mix(beg.sphere_pos, end.sphere_pos, progress);
	scene.iterations = uint(mix(beg.iterations, end.iterations, progress));
	scene.dt = mix(beg.dt, end.dt, progress);
}


