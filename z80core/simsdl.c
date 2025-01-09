/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 2025 Thomas Eberhardt
 */

/*
 *	This module contains the SDL2 integration for the simulator.
 */

#ifdef WANT_SDL

#include <stdbool.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_main.h>

#include "simsdl.h"
#include "simmain.h"

#define MAX_WINDOWS 5

static int sim_thread_func(void *data);

typedef struct args {
	int argc;
	char **argv;
} args_t;

typedef struct window {
	bool in_use;
	bool is_new;
	bool quit;
	win_funcs_t *funcs;
} window_t;

static window_t win[MAX_WINDOWS];
static bool sim_finished;	/* simulator thread finished flag */

int main(int argc, char *argv[])
{
	SDL_Event event;
	bool quit = false, tick;
	SDL_Thread *sim_thread;
	uint64_t t1, t2;
	int i, status;
	args_t args = {argc, argv};

	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		fprintf(stderr, "Can't initialize SDL: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	sim_finished = false;
	sim_thread = SDL_CreateThread(sim_thread_func, "Simulator", &args);
	if (sim_thread == NULL) {
		fprintf(stderr, "Can't create simulator thread: %s\n", SDL_GetError());
		SDL_Quit();
		return EXIT_FAILURE;
	}

	tick = false;
	t1 = SDL_GetTicks64() + 1000;
	while (!quit) {
		/* process event queue */
		while (SDL_PollEvent(&event) != 0) {
			if (event.type == SDL_QUIT)
				quit = true;

			for (i = 0; i < MAX_WINDOWS; i++)
				if (win[i].in_use)
					(*win[i].funcs->event)(&event);
		}

		/* open/close/draw windows */
		for (i = 0; i < MAX_WINDOWS; i++)
			if (win[i].in_use) {
				if (win[i].quit) {
					(*win[i].funcs->close)();
					win[i].in_use = false;
				} else {
					if (win[i].is_new) {
						(*win[i].funcs->open)();
						win[i].is_new = false;
					}
					(*win[i].funcs->draw)(tick);
				}
			}

		/* update seconds tick */
		tick = false;
		t2 = SDL_GetTicks64();
		if (t2 >= t1) {
			tick = true;
			t1 = t2 + 1000;
		}

		if (sim_finished)
			quit = true;
	}

	SDL_WaitThread(sim_thread, &status);

	for (i = 0; i < MAX_WINDOWS; i++)
		if (win[i].in_use)
			(*win[i].funcs->close)();

	SDL_Quit();

	return status;
}

/* this is called from the simulator thread */
int simsdl_create(win_funcs_t *funcs)
{
	int i;

	for (i = 0; i < MAX_WINDOWS; i++)
		if (!win[i].in_use) {
			win[i].is_new = true;
			win[i].quit = false;
			win[i].funcs = funcs;
			win[i].in_use = true;
			break;
		}

	if (i == MAX_WINDOWS) {
		fprintf(stderr, "No more window slots left\n");
		i = -1;
	}

	return i;
}

/* this is called from the simulator thread */
void simsdl_destroy(int i)
{
	if (i >= 0 && i < MAX_WINDOWS)
		win[i].quit = true;
}

/* this thread runs the simulator */
static int sim_thread_func(void *data)
{
	args_t *args = (args_t *) data;
	int status;

	status = sim_main(args->argc, args->argv);
	sim_finished = true;

	return status;
}

#endif /* WANT_SDL */
