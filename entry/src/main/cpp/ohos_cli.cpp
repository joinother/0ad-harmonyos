/*
 * HarmonyOS in-app command bridge implementation. The dispatch call runs on
 * the ArkTS/napi thread; the queued work is executed on the engine main thread
 * inside OhosCli_Pump (once per rendered frame).
 */
#include "ohos_cli.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

#include "gui/CGUI.h"
#include "gui/GUIManager.h"
#include "ps/Game.h"
#include "ps/GameSetup/Config.h"
#include "ps/VideoMode.h"
#include "scriptinterface/ScriptInterface.h"

#include "SDL_video.h"
#include <SDL_events.h>
#include <cstring>
#include <sys/stat.h>
#include <hilog/log.h>
#include <curl/curl.h>

extern "C" void SDL_OpenHarmonySendTouchpadPan(float, float);
extern "C" void SDL_OpenHarmonySendTouchpadPinch(float);
extern "C" void SDL_OpenHarmonySendWheelZoom(float);
extern "C" void SDL_OpenHarmonyInjectKey(int keycode, int down);

// AxisAction values carried by ArkUI AxisEvent (NONE0/BEGIN1/UPDATE2/END3/CANCEL4).
enum { AXIS_ACT_BEGIN = 1, AXIS_ACT_UPDATE = 2, AXIS_ACT_END = 3, AXIS_ACT_CANCEL = 4 };

struct AxisEv { int tool, action, hasPinch; float h, v, pinch; };
std::mutex g_axisMtx;
std::vector<AxisEv> g_axisQueue;

struct KeyEv { int keycode; int down; };
std::mutex g_keyMtx;
std::vector<KeyEv> g_keyQueue;

// Live Ctrl-modifier state, tracked from injected key events. Holding Ctrl while
// two-finger scrolling is the OS-standard zoom shortcut (the keyboard equivalent
// of a pinch), which works even on trackpads that expose no separate pinch axis.
static bool g_ctrlHeld = false;
// MMI key codes (multimodalinput/oh_key_code.h).
static bool IsCtrlCode(int code) { return code == 2072 || code == 2073; }

// Thread-safe enqueuers used by the native multimodal-input monitors
// (ohos_input_monitor.cpp), which fire on an MMI thread rather than the engine
// main thread. The events are drained and injected on the main thread in Pump.
extern "C" void OhosCli_EnqueueKey(int keycode, int down)
{
	std::lock_guard<std::mutex> lk(g_keyMtx);
	g_keyQueue.push_back(KeyEv{keycode, down});
}

extern "C" void OhosCli_EnqueueAxis(int tool, float h, float v, float pinch)
{
	std::lock_guard<std::mutex> lk(g_axisMtx);
	g_axisQueue.push_back(AxisEv{tool, AXIS_ACT_UPDATE, 0, h, v, pinch});
}

// Pinch scale is cumulative over one gesture (1.0 at begin); keep the previous
// value so each UPDATE becomes an incremental zoom delta.
static float g_pinchPrev = 1.0f;

static void InjectAxis(const AxisEv& e)
{
	OH_LOG_Print(LOG_APP, LOG_INFO, 0xD00, "zeroad/axis",
		"inject act=%{public}d tool=%{public}d h=%{public}.2f v=%{public}.2f pinch=%{public}.4f hasPinch=%{public}d",
		e.action, e.tool, e.h, e.v, e.pinch, e.hasPinch);
	// The real touchpad reports sourceTool 9 on this device; 4 is the documented
	// TOUCHPAD value. Both drive a two-finger pan / pinch. A physical mouse wheel
	// (tool 3) zooms instead.
	const bool isTouchpad = (e.tool == 4 || e.tool == 9);
	if (isTouchpad)
	{
		if (e.hasPinch)
		{
			// A genuine pinch gesture; convert its cumulative scale to deltas.
			if (e.action == AXIS_ACT_BEGIN)
			{
				g_pinchPrev = (e.pinch > 0.0f ? e.pinch : 1.0f);
				return;
			}
			if (e.action == AXIS_ACT_END || e.action == AXIS_ACT_CANCEL)
			{
				g_pinchPrev = 1.0f;
				return;
			}
			const float cur = (e.pinch > 0.0f ? e.pinch : g_pinchPrev);
			float delta = cur - g_pinchPrev;
			g_pinchPrev = cur;
			// Ignore implausibly large jumps (a dropped/reset gesture).
			if (delta > 0.4f || delta < -0.4f)
				return;
			// Suppress any pan that arrives bundled with the pinch.
			SDL_OpenHarmonySendTouchpadPinch(delta);
			return;
		}
		if (g_ctrlHeld)
		{
			// Ctrl + two-finger drag = zoom. Match the mouse-wheel sign convention
			// (SendWheelZoom positive = zoom in; two-finger up, v<0, zooms in).
			const float z = (e.v != 0.0f ? e.v : e.h);
			if (z != 0.0f)
				SDL_OpenHarmonySendWheelZoom(-z / 100.0f);
			return;
		}
		if (e.h != 0.0f || e.v != 0.0f)
			SDL_OpenHarmonySendTouchpadPan(e.h, e.v);
	}
	else if (e.tool == 3) // physical mouse wheel
	{
		if (e.v != 0.0f)
			SDL_OpenHarmonySendWheelZoom(-e.v / 100.0f);
	}
}

namespace
{

struct Request
{
	std::string name;
	std::string payload;
	std::string result;
	bool done = false;
	std::mutex mtx;
	std::condition_variable cv;
};

std::mutex g_queueMtx;
std::vector<Request*> g_queue;

std::string JsonQuote(const std::string& s)
{
	std::string out = "\"";
	for (unsigned char c : s)
	{
		switch (c)
		{
		case '"':  out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20)
			{
				char buf[8];
				std::snprintf(buf, sizeof(buf), "\\u%04x", c);
				out += buf;
			}
			else
				out += static_cast<char>(c);
		}
	}
	out += "\"";
	return out;
}

// Evaluate arbitrary JS in the current GUI context and return the JSON result.
std::string ExecuteJs(const std::string& payload)
{
	if (!g_GUI)
		return R"({"ok":false,"error":"gui not ready"})";

	std::shared_ptr<CGUI> active = g_GUI->GetActiveGUI();
	if (!active)
		return R"({"ok":false,"error":"no active gui page"})";

	std::shared_ptr<ScriptInterface> si = active->GetScriptInterface();
	if (!si)
		return R"({"ok":false,"error":"script interface not ready"})";

	const std::string quoted = JsonQuote(payload);
	const std::string code =
		"(function(){"
		"try{"
		"var __v=(function(){return eval(" + quoted + ");})();"
		"return JSON.stringify({ok:true,result:__v});"
		"}catch(e){"
		"return JSON.stringify({ok:false,error:String(e),stack:(e.stack||'')});"
		"}})()";

	std::string out;
	if (!si->Eval(code.c_str(), out) || out.empty())
		return R"({"ok":false,"error":"eval did not return a value"})";
	return out;
}

// Report window/engine sizes and actively run the surface-size rescan, so a
// resized window is reconciled even if the OS never sent us a resize event.
std::string ExecuteDiag()
{
	SDL_Window* win = g_VideoMode.GetWindow();
	int lw = 0, lh = 0, pw = 0, ph = 0;
	if (win)
	{
		SDL_GetWindowSize(win, &lw, &lh);
		SDL_GetWindowSizeInPixels(win, &pw, &ph);
	}

	g_VideoMode.HarmonyPollSurfaceSize();

	char buf[384];
	std::snprintf(buf, sizeof(buf),
		R"({"ok":true,"sdlWindow":[%d,%d],"sdlPixels":[%d,%d],"engine":[%d,%d]})",
		lw, lh, pw, ph, g_xres, g_yres);
	return std::string(buf);
}

std::string ReadFileToString(const std::string& path, size_t maxBytes)
{
	FILE* f = std::fopen(path.c_str(), "rb");
	if (!f)
		return "";
	std::string data;
	char buf[8192];
	size_t total = 0;
	while (total < maxBytes)
	{
		const size_t n = std::fread(buf, 1, sizeof(buf), f);
		if (!n)
			break;
		const size_t take = std::min(n, maxBytes - total);
		data.append(buf, take);
		total += take;
		if (n < sizeof(buf))
			break;
	}
	std::fclose(f);
	return data;
}

std::string ExecuteReadFile(const std::string& payload)
{
	static const char* const kBase =
		"/data/storage/el2/base/haps/entry/files/0ad/logs/";
	std::vector<std::string> candidates;
	if (payload.empty() || payload == "log" || payload == "interesting")
	{
		candidates.push_back(std::string(kBase) + "interestinglog.html");
		candidates.push_back(std::string(kBase) + "mainlog.html");
	}
	else
		candidates.push_back(payload);

	std::string content;
	for (const std::string& p : candidates)
	{
		content = ReadFileToString(p, 800 * 1024);
		if (!content.empty())
			break;
	}
	if (content.empty())
		return R"({"ok":false,"error":"cannot open or empty"})";
	// The most recent entries are at the end; return the tail.
	if (content.size() > 240 * 1024)
		content = content.substr(content.size() - 240 * 1024);
	return R"({"ok":true,"content":)" + JsonQuote(content) + "}";
}

std::string ExecuteSslTest(const std::string& payload)
{
	const std::string kCa =
		"/data/storage/el2/base/haps/entry/files/0ad/config/cacert.pem";
	const std::string url = payload.empty()
		? std::string("https://api.mod.io/v1/games")
		: payload;

	const char* envCurl = std::getenv("CURL_CA_BUNDLE");
	const char* envSsl = std::getenv("SSL_CERT_FILE");
	struct stat st;
	std::memset(&st, 0, sizeof(st));
	stat(kCa.c_str(), &st);

	curl_global_init(CURL_GLOBAL_ALL);
	CURL* c = curl_easy_init();
	char errbuf[CURL_ERROR_SIZE];
	errbuf[0] = '\0';
	curl_easy_setopt(c, CURLOPT_URL, url.c_str());
	curl_easy_setopt(c, CURLOPT_NOBODY, 1L);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, 20L);
	curl_easy_setopt(c, CURLOPT_CAINFO, kCa.c_str());
	curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(c, CURLOPT_ERRORBUFFER, errbuf);
	const CURLcode rc = curl_easy_perform(c);
	long httpCode = 0;
	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &httpCode);
	curl_easy_cleanup(c);

	std::string r = "{\"ok\":";
	r += (rc == CURLE_OK) ? "true" : "false";
	r += ",\"curlCode\":" + std::to_string(static_cast<int>(rc));
	r += ",\"httpCode\":" + std::to_string(static_cast<int>(httpCode));
	r += ",\"error\":" + JsonQuote(std::string(errbuf));
	r += ",\"caExists\":" + std::string(st.st_size > 0 ? "true" : "false");
	r += ",\"caSize\":" + std::to_string(static_cast<long long>(st.st_size));
	r += ",\"envCurl\":" + JsonQuote(envCurl ? envCurl : "");
	r += ",\"envSsl\":" + JsonQuote(envSsl ? envSsl : "");
	r += "}";
	return r;
}

std::string Execute(const std::string& name, const std::string& payload)
{
	if (name == "ping")
		return R"({"ok":true,"pong":true})";

	if (name == "diag")
		return ExecuteDiag();

	if (name == "readfile")
		return ExecuteReadFile(payload);

	if (name == "ssltest")
		return ExecuteSslTest(payload);

	if (name == "state")
	{
		const bool gui = static_cast<bool>(g_GUI);
		const bool game = static_cast<bool>(g_Game && g_Game->IsGameStarted());
		std::string r = "{\"ok\":true,\"gui\":";
		r += gui ? "true" : "false";
		r += ",\"game\":";
		r += game ? "true" : "false";
		r += "}";
		return r;
	}

	if (name == "js")
		return ExecuteJs(payload);

	return R"({"ok":false,"error":"unknown command: "})" + JsonQuote(name) + "}";
}

} // namespace

extern "C" char* OhosCli_Dispatch(const char* name, const char* payload,
                                  int timeoutMs)
{
	if (name && std::strcmp(name, "axis") == 0)
	{
		AxisEv e{0, AXIS_ACT_UPDATE, 0, 0.0f, 0.0f, 0.0f};
		if (payload)
			std::sscanf(payload, "%d,%d,%f,%f,%f,%d",
			            &e.tool, &e.action, &e.h, &e.v, &e.pinch, &e.hasPinch);
		{ std::lock_guard<std::mutex> lk(g_axisMtx); g_axisQueue.push_back(e); }
		const char* r = "{\"ok\":true}";
		char* buf = static_cast<char*>(std::malloc(std::strlen(r) + 1));
		std::strcpy(buf, r);
		return buf;
	}

	if (name && std::strcmp(name, "key") == 0)
	{
		KeyEv k{0, 0};
		if (payload)
			std::sscanf(payload, "%d,%d", &k.keycode, &k.down);
		{ std::lock_guard<std::mutex> lk(g_keyMtx); g_keyQueue.push_back(k); }
		const char* r = "{\"ok\":true}";
		char* buf = static_cast<char*>(std::malloc(std::strlen(r) + 1));
		std::strcpy(buf, r);
		return buf;
	}

	Request* req = new Request;
	req->name = name ? name : "";
	req->payload = payload ? payload : "";

	{
		std::lock_guard<std::mutex> lk(g_queueMtx);
		g_queue.push_back(req);
	}

	std::unique_lock<std::mutex> lk(req->mtx);
	const int ms = timeoutMs > 0 ? timeoutMs : 15000;
	const bool finished = req->cv.wait_for(lk, std::chrono::milliseconds(ms),
	                                       [&] { return req->done; });

	std::string out;
	if (!finished)
	{
		std::lock_guard<std::mutex> ql(g_queueMtx);
		auto it = std::find(g_queue.begin(), g_queue.end(), req);
		if (it != g_queue.end())
			g_queue.erase(it);
		out = R"({"ok":false,"error":"cli timeout"})";
	}
	else
		out = req->result;

	char* buf = static_cast<char*>(std::malloc(out.size() + 1));
	std::memcpy(buf, out.data(), out.size());
	buf[out.size()] = '\0';
	delete req;
	return buf;
}

extern "C" void OhosCli_Free(char* p)
{
	std::free(p);
}

extern "C" void OhosCli_Pump(void)
{
	{
		std::vector<AxisEv> axisBatch;
		{ std::lock_guard<std::mutex> lk(g_axisMtx); axisBatch.swap(g_axisQueue); }
		for (const AxisEv& e : axisBatch) InjectAxis(e);
	}
	{
		std::vector<KeyEv> keyBatch;
		{ std::lock_guard<std::mutex> lk(g_keyMtx); keyBatch.swap(g_keyQueue); }
		for (const KeyEv& k : keyBatch)
		{
			if (IsCtrlCode(k.keycode))
				g_ctrlHeld = (k.down != 0);
			SDL_OpenHarmonyInjectKey(k.keycode, k.down);
		}
	}
	std::vector<Request*> batch;
	{
		std::lock_guard<std::mutex> lk(g_queueMtx);
		batch.swap(g_queue);
	}
	for (Request* req : batch)
	{
		req->result = Execute(req->name, req->payload);
		{
			std::lock_guard<std::mutex> lk(req->mtx);
			req->done = true;
		}
		req->cv.notify_all();
	}
}
