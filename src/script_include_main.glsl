// included at the end of script

void main()
{
	if (progress == -1.0) {
		init();
	} else {
		scene.inv_screen_width = 1.0 / float(win.screen_width);
		scene.focal_length = 0.5 / tan(0.5 * win.fov);
		loop();
	}
}

