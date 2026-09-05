#include <gamesh.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL3/SDL.h>

#define perror_exit(...) \
	printf("%s:%d ", __FILE__, __LINE__),\
	printf(__VA_ARGS__),\
	putchar('\n'), \
	exit(EXIT_FAILURE)

int main(int argc, char **argv)
{
	if (argc < 3)
		return EXIT_FAILURE;
	argv++;

	// creates a surface at the top left of the window
	int surface_id = gamesh_create_render_surface(0, 0, 640, 480);

	gamesh_shared_buffer_t *shared_pngs[2] = { 0 };
	int buffer_ids[2] = { -1, -1 };
	for (int i = 0; i < 2; i++) {
		SDL_Surface *local_png = SDL_LoadPNG(argv[i]);
		if (!local_png)
			perror_exit("Couldn't find %s", argv[i]);
		shared_pngs[i] = gamesh_create_shared_buffer(local_png);
		if (!shared_pngs[i])
			perror_exit("gamesh_create_shared_buffer");
		SDL_DestroySurface(local_png);

		buffer_ids[i] = gamesh_add_buffer(shared_pngs[i]);
		if (buffer_ids[i] < 0)
			perror_exit("gamesh_add_buffer");
	}

	int current = 0;
	while (1) {
		if (gamesh_set_surface_buffer(surface_id, buffer_ids[current]) < 0)
			break;
		sleep(1);
		current = !current;
	}
}
