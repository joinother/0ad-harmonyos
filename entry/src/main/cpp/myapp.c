/* Minimal SDL3 GPU clear test for HarmonyOS (exercises the Vulkan backend). */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define FORMATS (SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB)

static SDL_Window *window = NULL;
static SDL_GPUDevice *gpu = NULL;
static Uint64 frames = 0;
static Uint64 start = 0;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    window = SDL_CreateWindow("0ad-gpu-test", 1280, 800,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        SDL_Log("CreateWindow failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    gpu = SDL_CreateGPUDevice(FORMATS, true, NULL);
    if (!gpu) {
        SDL_Log("CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Log("[gpu-test] backend driver = %s", SDL_GetGPUDeviceDriver(gpu));
    if (!SDL_ClaimWindowForGPUDevice(gpu, window)) {
        SDL_Log("ClaimWindow failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    start = SDL_GetTicks();
    SDL_Log("[gpu-test] init OK");
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE)
        return SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    SDL_GPUCommandBuffer *cmdbuf = SDL_AcquireGPUCommandBuffer(gpu);
    if (!cmdbuf) {
        SDL_Log("AcquireCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_GPUTexture *tex = NULL;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, window, &tex, NULL, NULL)) {
        SDL_Log("AcquireSwapchain failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cmdbuf);
        return SDL_APP_FAILURE;
    }
    if (tex) {
        double t = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();
        SDL_GPUColorTargetInfo info;
        SDL_zero(info);
        info.texture = tex;
        info.clear_color.r = (float)(0.5 + 0.5 * SDL_sin(t));
        info.clear_color.g = (float)(0.5 + 0.5 * SDL_sin(t + 2.094));
        info.clear_color.b = (float)(0.5 + 0.5 * SDL_sin(t + 4.188));
        info.clear_color.a = 1.0f;
        info.load_op = SDL_GPU_LOADOP_CLEAR;
        info.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmdbuf, &info, 1, NULL);
        SDL_EndGPURenderPass(pass);
        SDL_SubmitGPUCommandBuffer(cmdbuf);
    } else {
        SDL_CancelGPUCommandBuffer(cmdbuf);
    }
    frames++;
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    Uint64 now = SDL_GetTicks();
    if (now > start)
        SDL_Log("[gpu-test] %.1f FPS", ((double)frames * 1000.0) / (now - start));
    if (gpu) {
        if (window) SDL_ReleaseWindowFromGPUDevice(gpu, window);
        SDL_DestroyGPUDevice(gpu);
    }
    SDL_Quit();
}
