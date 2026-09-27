/*
 * 0 A.D. HarmonyOS entry shim.
 * SDL3 calls SDL_main on a dedicated thread. The engine itself is built as a
 * shared library exposing pyrogenesis_main(); we invoke it and then block,
 * because returning from the main thread makes a HarmonyOS app appear to crash.
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

extern int pyrogenesis_main(int argc, char* argv[]);
extern void ohos_bootstrap_assets(void);

int SDL_main(int argc, char* argv[])
{
	ohos_bootstrap_assets();
	int rc = pyrogenesis_main(argc, argv);
	(void)rc;

	for (;;)
		SDL_Delay(1000);

	return rc;
}
