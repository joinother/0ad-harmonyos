/*
 * NAPI surface for the 0 A.D. command bridge. Built as libclibridge.so so
 * ArkTS can `import { command } from 'libclibridge.so'`. It forwards into the
 * engine (libmain.so) via dlopen/dlsym; the engine executes on its own main
 * thread, avoiding any conflict with this napi/UI thread.
 */
#include "napi/native_api.h"

#include <dlfcn.h>

#include <string>

typedef char* (*OhosDispatchFn)(const char*, const char*, int);
typedef void (*OhosFreeFn)(char*);

namespace
{

struct Bridge
{
	void* handle = nullptr;
	OhosDispatchFn dispatch = nullptr;
	OhosFreeFn freeResult = nullptr;
	bool ready = false;
};

Bridge& GetBridge()
{
	static Bridge bridge;
	if (!bridge.handle)
	{
		bridge.handle = dlopen("libmain.so", RTLD_NOW | RTLD_LOCAL);
		if (bridge.handle)
		{
			bridge.dispatch = reinterpret_cast<OhosDispatchFn>(
				dlsym(bridge.handle, "OhosCli_Dispatch"));
			bridge.freeResult = reinterpret_cast<OhosFreeFn>(
				dlsym(bridge.handle, "OhosCli_Free"));
			bridge.ready = bridge.dispatch && bridge.freeResult;
		}
	}
	return bridge;
}

std::string GetString(napi_env env, napi_value value)
{
	size_t length = 0;
	napi_get_value_string_utf8(env, value, nullptr, 0, &length);
	std::string out(length, '\0');
	napi_get_value_string_utf8(env, value, &out[0], length + 1, &length);
	return out;
}

napi_value Command(napi_env env, napi_callback_info info)
{
	size_t argc = 2;
	napi_value args[2] = {nullptr, nullptr};
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

	std::string name = argc > 0 ? GetString(env, args[0]) : "";
	std::string payload = argc > 1 ? GetString(env, args[1]) : "";

	Bridge& bridge = GetBridge();
	std::string result;
	if (bridge.ready)
	{
		char* raw = bridge.dispatch(name.c_str(), payload.c_str(), 15000);
		result = raw ? raw : "";
		bridge.freeResult(raw);
	}
	else
	{
		result = R"({"ok":false,"error":"cli bridge unavailable"})";
	}

	napi_value rv;
	napi_create_string_utf8(env, result.c_str(), result.size(), &rv);
	return rv;
}

napi_value Init(napi_env env, napi_value exports)
{
	napi_property_descriptor desc[] = {
		{"command", nullptr, Command, nullptr, nullptr, nullptr, napi_default, nullptr}
	};
	napi_define_properties(env, exports, 1, desc);
	return exports;
}

napi_module g_module = {
	.nm_version = 1,
	.nm_flags = 0,
	.nm_filename = nullptr,
	.nm_register_func = Init,
	.nm_modname = "clibridge",
	.nm_priv = nullptr,
	.reserved = {0},
};

} // namespace

extern "C" __attribute__((constructor)) void RegisterCliBridge()
{
	napi_module_register(&g_module);
}
