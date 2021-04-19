#include <stdio.h>

#include <CL/cl.h>

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <SDL2/SDL.h>

#include <time.h>


#define WIDTH 1000
#define HEIGHT 1000
#define ITEM_COUNT WIDTH * HEIGHT



const char* source = 
"int2 neighbours[8] = {\n"
"	(int2)( 1,  0),\n"
"	(int2)(-1,  0),\n"
"	(int2)( 0,  1),\n"
"	(int2)( 0, -1),\n"
"	(int2)( 1,  1),\n"
"	(int2)(-1,  1),\n"
"	(int2)( 1, -1),\n"
"	(int2)(-1, -1)\n"
"};\n"
"\n"
"kernel void memset(global uchar* read_img, global uchar* write_image, const uint width) {\n"
"	size_t pixel_coord = get_global_id(0) * 3;\n"
"	size_t x = get_global_id(0) % width;\n"
"	size_t y = (get_global_id(0) - x) / width;\n"
"	\n"
"	uchar neighbour_count = 0;\n"
"	for(uchar i = 0; i < 8; i++) {\n"
"		size_t neighbour_index = (((y + neighbours[i].y) * width) + (x + neighbours[i].x)) % get_global_size(0);\n"
"		neighbour_count += (read_img[neighbour_index * 3] > 0);\n"
"	}\n"
"	\n"
"	uchar res = ((read_img[pixel_coord] > 0) && (neighbour_count == 2 || neighbour_count == 3)) ||\n"
"				((read_img[pixel_coord] < 1) && (neighbour_count == 3));\n"

"	write_image[pixel_coord + 0] = res * 255;\n"
"	write_image[pixel_coord + 1] = res * 255;\n"
"	write_image[pixel_coord + 2] = res * 255;\n"
"}\n";

SDL_Window* window;


int main() {

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	window = SDL_CreateWindow("testapp", 0, 0, WIDTH, HEIGHT, SDL_WINDOW_OPENGL);
	SDL_GLContext gl_context = SDL_GL_CreateContext(window);

	SDL_GL_SetSwapInterval(1);

	glewInit();

	glViewport(0, 0, WIDTH, HEIGHT);
	glClearColor(0, 0, 0, 1);

	unsigned int fbo;
	glGenFramebuffers(1, &fbo);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	unsigned int texture;
	glGenTextures(1, &texture);

	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, WIDTH, HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("oh no, framebuffer no worky =(");
		exit(-1);
	}

	unsigned int pbo;
	glGenBuffers(1, &pbo);
	glBindBuffer(GL_ARRAY_BUFFER, pbo);
	glBufferData(GL_ARRAY_BUFFER, WIDTH * HEIGHT * sizeof(cl_uchar) * 3, NULL, GL_STREAM_DRAW);


	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	cl_int errcode = 0;

	// gimme platform
	cl_platform_id platform;
	clGetPlatformIDs(1, &platform, NULL);

	// gimme gpu
	cl_device_id device;
	clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);

	cl_context context = clCreateContext(NULL, 1, &device, 0, 0, &errcode);
	printf("made context\n");

	cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, NULL, &errcode);
	printf("created queue\n");

	cl_program program = clCreateProgramWithSource(context, 1, &source, NULL, &errcode);
	clBuildProgram(program, 1, &device, "-cl-std=CL2.0", NULL, NULL);
	printf("built program\n");

	cl_build_status build_status;
	clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_STATUS, sizeof(cl_build_status), &build_status, NULL);
	if(build_status != CL_BUILD_SUCCESS) {
		size_t len;
		clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
		char build_log[len];
		clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, len, &build_log, NULL);
		printf("%lu, %s\n", len, build_log);
	}


	glBindBuffer(GL_ARRAY_BUFFER, pbo);
	void* img = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE);

	srand(time(NULL));

	for(int i = 0; i < WIDTH * HEIGHT * 3; i+=3) {
		unsigned char val = (rand()%100 > 60) * 255;

		((unsigned char*) img)[i + 0] = val;
		((unsigned char*) img)[i + 1] = val;
		((unsigned char*) img)[i + 2] = val;
	}

	void* img_copy = malloc(WIDTH * HEIGHT * 3);
	memcpy(img_copy, img, WIDTH * HEIGHT * 3);


	cl_mem mem = clCreateBuffer(context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, WIDTH * HEIGHT * 3, img, &errcode);
	if(errcode != 0) {
		printf("Opencl error, line: %i, code: %i\n", __LINE__, errcode);
	}

	cl_mem img_copy_mem = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, WIDTH * HEIGHT * 3, img_copy, &errcode);
	if(errcode != 0) {
		printf("Opencl error, line: %i, code: %i\n", __LINE__, errcode);
	}

	printf("created opengl buffer thing\n");

	cl_kernel kernel = clCreateKernel(program, "memset", &errcode);
	size_t global_work_size = ITEM_COUNT;
	if(errcode != 0) {
		printf("Opencl error, line: %i, code: %i\n", __LINE__, errcode);
	}

	clSetKernelArg(kernel, 1, sizeof(mem), (void*) &mem);
	uint w = WIDTH;
	clSetKernelArg(kernel, 2, sizeof(cl_uint), &w);

	printf("created kernel\n");
	printf("finished setting up!\n");

	// do the opencl

	printf("finished opencl\n");
	
	float off_x = 0;
	float off_y = 0;
	float zoom = 1;

	int left_click = 0;
	int mouse_x = 0;
	int mouse_y = 0;
	int mouse_prev_x = 0;
	int mouse_prev_y = 0;

	float time = 0;
	float time_prev = 0;
	float delta = 0;
	
	cl_bool should_exit = 0;
	while(!should_exit) {
		SDL_GetTicks();
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			if(event.type == SDL_QUIT)
				should_exit = 1;
			else if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
				should_exit = 1;
			else if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
				left_click = 1;
			else if(event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
				left_click = 0;
			else if(event.type == SDL_MOUSEWHEEL) {
				zoom += event.wheel.y * delta * 10.f;
			}
		}

		time = SDL_GetTicks() / 1000.f;
		delta = time - time_prev;
		time_prev = time;

		SDL_GetMouseState(&mouse_x, &mouse_y);

		if(left_click) {
			off_x -= (mouse_x - mouse_prev_x) * delta * 60;
			off_y += (mouse_y - mouse_prev_y) * delta * 60;
		}


		mouse_prev_x = mouse_x;
		mouse_prev_y = mouse_y;

		if(zoom < 0)
			zoom = 0;

		printf("off: %f, %f, %f\n", off_x, off_y, zoom);

		glClear(GL_COLOR_BUFFER_BIT);
		clSetKernelArg(kernel, 0, sizeof(img_copy_mem), &img_copy_mem);
		clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);
		clFinish(queue);

		img = clEnqueueMapBuffer(queue, mem, CL_TRUE, CL_MAP_READ, 0, WIDTH * HEIGHT * 3, 0, NULL, NULL, NULL);

		glUnmapBuffer(GL_ARRAY_BUFFER);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, img);
		memcpy(img_copy, img, WIDTH * HEIGHT * 3);

		clEnqueueWriteBuffer(queue, img_copy_mem, CL_TRUE, 0, WIDTH * HEIGHT * 3, img_copy, 0, NULL, NULL);
	
		// do opengl drawing
		glBlitNamedFramebuffer(fbo, 0, off_x / zoom, off_y / zoom, WIDTH / zoom + off_x / zoom, HEIGHT / zoom + off_y / zoom, 0, 0, WIDTH, HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		SDL_GL_SwapWindow(window);
		SDL_Delay(10);
	}

	glDeleteFramebuffers(1, &fbo);
	glDeleteTextures(1, &texture);
	SDL_GL_DeleteContext(gl_context);

	clReleaseKernel(kernel);
	clReleaseProgram(program);
	clReleaseMemObject(mem);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);
	clReleaseDevice(device);

	return 0;
}
