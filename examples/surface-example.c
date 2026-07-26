#include <gamesh.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>

#define perror_exit(...) \
	printf("%s:%d ", __FILE__, __LINE__),\
	printf(__VA_ARGS__),\
	putchar('\n'), \
	exit(EXIT_FAILURE)

int main(int argc, char **argv)
{
	if (argc < 2)
		return EXIT_FAILURE;

	int surface_id = gamesh_create_render_surface();

	SDL_Surface *local_png = SDL_LoadPNG(argv[1]);
	if (!local_png)
		perror_exit("Couldn't find %s", argv[1]);

	gamesh_shared_buffer_t *shared_png = gamesh_create_shared_buffer(local_png);
	if (!shared_png)
		perror_exit("gamesh_create_shared_buffer");

	SDL_DestroySurface(local_png);
	int buffer_id = gamesh_add_surface_buffer(surface_id, shared_png);
}
