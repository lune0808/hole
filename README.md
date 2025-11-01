ray tracer with relativistic rays - aka black hole renderer

requires:
* OpenGL version >= 4.5
* C++ version >= 20
* [liburing](https://github.com/axboe/liburing.git)

build (linux):
$ make [-jN] # if you have N cores

run (slow):
$ bin/main <path-to-script>.glsl
OR
$ bin/main <path-to-script>.glsl -o <output-path>
OR
$ bin/main <path-to-script>.glsl -r <crashed-output-path>
OR
$ bin/main -i <input-path>
