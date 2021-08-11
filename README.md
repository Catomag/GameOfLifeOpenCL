

A C implementation of [conways game of life](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life)

It's written in C and uses glut, glew, opengl, [OpenCL](https://en.wikipedia.org/wiki/OpenCL) and [SDL](https://en.wikipedia.org/wiki/Simple_DirectMedia_Layer).

The actual magic in this repo is the OpenCL library which simulates a large board with each pixel acting as a cell. The GPU processes each pixel individually which makes the simulation fast and efficient. Requires a modern graphics card 

## Screenshots
![fullscale](/screenshot115.png)
![medium](/screenshot116.png)
![look at them up close!](/screenshot117.png)
