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

	int surface_id = gamesh_create_render_surface();

	SDL_Surface *local_pngs[] = {
		SDL_LoadPNG(argv[1]),
		SDL_LoadPNG(argv[2]),
	};
	if (!local_pngs[0])
		perror_exit("Couldn't find %s", argv[1]);
	if (!local_pngs[1])
		perror_exit("Couldn't find %s", argv[2]);

	gamesh_shared_buffer_t *shared_pngs[] = {
		gamesh_create_shared_buffer(local_pngs[0]),
		gamesh_create_shared_buffer(local_pngs[1]),
	};
	if (!(shared_pngs[0] && shared_pngs[1]))
		perror_exit("gamesh_create_shared_buffer");

	SDL_DestroySurface(local_pngs[0]);
	SDL_DestroySurface(local_pngs[1]);

	// the first buffer_id added will automatically be shown
	int buffer_ids[] = {
		gamesh_add_surface_buffer(shared_pngs[0]),
		gamesh_add_surface_buffer(shared_pngs[1]),
	};

	int current = 0;
	while (1) {
		if (gamesh_set_surface_buffer(surface_id, buffer_ids[current]) < 0)
			break;

		sleep(1);

		current = !current;
	}
}
