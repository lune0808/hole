#version 430 core

uniform layout(binding=0) sampler2DArray screen0;
uniform layout(binding=1) sampler2DArray screen1;
uniform layout(location=2) float frame;
uniform layout(location=3) float select;
uniform layout(binding=4) samplerCube skybox;
uniform layout(location=5) vec3 exponents;
in vec2 uv;
out vec4 f_color;


float sgn(float x)
{
	return uintBitsToFloat(0x3f800000 | (floatBitsToUint(x) & (1u << 31)));
}

vec3 light_shift(float intensity)
{
	return pow(vec3(intensity), exponents);
}

void main()
{
	vec3 coord = vec3(uv, frame);
	vec4 color0 = texture(screen0, coord);
	vec4 color1 = texture(screen1, coord);
	vec4 color = mix(color0, color1, select);
	vec3 ray = vec3(color.xy, sgn(color.z) * sqrt(1.0 - dot(color.xy, color.xy)));
	float transmittance = abs(color.z);
	// transmittance = abs(color.z) - 0.5;
	float light = color.w;
	vec3 ambient = vec3(0.05);
	vec3 sky = texture(skybox, ray).rgb;
	f_color = vec4(ambient + transmittance * sky + light_shift(light), 1.0);
}

