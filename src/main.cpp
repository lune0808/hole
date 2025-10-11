#include <cstdio>
#include <cassert>
#include <glm/glm.hpp>
#include "window.hpp"
#include "shader.hpp"

struct camera_t
{
	float fov;
	float width;
	float height;
	glm::vec3 position;
	glm::vec3 direction;
};

int main()
{
	const int width = 800;
	const int height = 600;
	window win(width, height);
	camera_t camera{
		0.78f, float(width), float(height),
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f,-1.0f },
	};
	while (win) {
		win.draw();
	}
}

