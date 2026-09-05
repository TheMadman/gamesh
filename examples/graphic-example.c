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
	if (argc < 2)
		return EXIT_FAILURE;

	SDL_Surface *local_png = SDL_LoadPNG(argv[1]);
	if (!local_png)
		perror_exit("Couldn't find %s", argv[1]);

	gamesh_graphic_t graphic = gamesh_create_graphic_from(
		-1,
		-1,
		local_png->w,
		local_png->h,
		local_png
	);
	if (!graphic.buffer)
		perror_exit("gamesh_create_graphic_from");

	SDL_DestroySurface(local_png);

	if (gamesh_graphic_commit(graphic) < 0)
		perror_exit("gamesh_graphic_commit");
}
