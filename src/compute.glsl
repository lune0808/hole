#version 430 core

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
uniform layout(binding=0,rgba32f) writeonly restrict image2D screen;


vec4 color(ivec2 coord)
{
	ivec2 off = coord - ivec2(400, 300);
	int r = 200;
	if (dot(off, off) < r * r) {
		return vec4(0.0, 1.0, 1.0, 1.0);
	} else {
		return vec4(0.0);
	}
}

void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	imageStore(screen, coord, color(coord));
}

