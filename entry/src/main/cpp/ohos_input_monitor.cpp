/*
 * Native multimodal-input monitors for the HarmonyOS port.
 *
 * On 2-in-1 devices the XComponent surface does not reliably hold key focus, and
 * real touchpad two-finger pan/pinch events do not reach the ArkUI overlay. The
 * system multimodal-input (MMI) service offers app-wide monitors that observe
 * every key and axis event regardless of focus. We register those monitors and
 * forward events into the engine through the thread-safe CLI queues (drained and
 * injected on the engine main thread).
 */
#include <multimodalinput/oh_input_manager.h>
#include <multimodalinput/oh_axis_type.h>
#include <hilog/log.h>

#include <cmath>
#include <cstdio>

extern "C" void OhosCli_EnqueueKey(int keycode, int down);
extern "C" void OhosCli_EnqueueAxis(int tool, float h, float v, float pinch);

namespace
{
constexpr unsigned int kDomain = 0xD00;

// ---- Key events ----
void OnKeyEvent(const Input_KeyEvent* event)
{
	const int32_t action = OH_Input_GetKeyEventAction(event);
	const int32_t keycode = OH_Input_GetKeyEventKeyCode(event);
	const int down = (action == KEY_ACTION_DOWN) ? 1 : 0;
	OH_LOG_Print(LOG_APP, LOG_INFO, kDomain, "zeroad/mmi",
		"MMI KEY code=%{public}d action=%{public}d", keycode, action);
	OhosCli_EnqueueKey(keycode, down);
}

// ---- Axis events (touchpad pan/pinch, mouse wheel) ----
void OnAxisEvent(const Input_AxisEvent* event)
{
	InputEvent_AxisEventType eventType = AXIS_EVENT_TYPE_SCROLL;
	InputEvent_AxisAction axisAction = AXIS_ACTION_UPDATE;
	InputEvent_SourceType source = SOURCE_TYPE_MOUSE;
	OH_Input_GetAxisEventType(event, &eventType);
	OH_Input_GetAxisEventAction(event, &axisAction);
	OH_Input_GetAxisEventSourceType(event, &source);

	// Match the SourceTool numbering used by the ArkUI bridge:
	// 3 = mouse, 4 = touchpad.
	const int tool = (source == SOURCE_TYPE_TOUCHPAD) ? 4
		: (source == SOURCE_TYPE_MOUSE ? 3 : 0);

	if (eventType == AXIS_EVENT_TYPE_SCROLL)
	{
		double vertical = 0.0, horizontal = 0.0;
		OH_Input_GetAxisEventAxisValue(event, AXIS_TYPE_SCROLL_VERTICAL, &vertical);
		OH_Input_GetAxisEventAxisValue(event, AXIS_TYPE_SCROLL_HORIZONTAL, &horizontal);

		// Touchpad scroll values are delivered in pixels and drive a map pan.
		// A real mouse wheel reports discrete detents and should zoom. The MMI
		// wheel value is commonly ~100 units per detent; normalise to a notch.
		float v = static_cast<float>(vertical), h = static_cast<float>(horizontal);
		float pinch = 0.0f;
		if (tool == 3)
		{
			// Convert the wheel amount to a zoom delta (one notch ~= 0.1).
			float notch = v;
			if (std::fabs(notch) > 5.0f)
				notch = notch / 100.0f;
			pinch = -notch * 0.5f;
			v = 0.0f;
			h = 0.0f;
		}

		char buf[160];
		std::snprintf(buf, sizeof(buf),
			"MMI SCROLL tool=%d action=%d h=%.2f v=%.2f pinch=%.3f",
			tool, static_cast<int>(axisAction), h, v, pinch);
		OH_LOG_Print(LOG_APP, LOG_INFO, kDomain, "zeroad/mmi", "%{public}s", buf);

		OhosCli_EnqueueAxis(tool, h, v, pinch);
	}
	else if (eventType == AXIS_EVENT_TYPE_PINCH)
	{
		double scale = 0.0;
		OH_Input_GetAxisEventAxisValue(event, AXIS_TYPE_PINCH, &scale);

		// The MMI pinch value is a cumulative scale (1.0 at the gesture start),
		// matching SDL_OpenHarmonySendTouchpadPinch. Pass it through.
		char buf[128];
		std::snprintf(buf, sizeof(buf), "MMI PINCH action=%d scale=%.4f",
			static_cast<int>(axisAction), scale);
		OH_LOG_Print(LOG_APP, LOG_INFO, kDomain, "zeroad/mmi", "%{public}s", buf);

		OhosCli_EnqueueAxis(4, 0.0f, 0.0f, static_cast<float>(scale));
	}
}

} // namespace

// Registered from the bootstrap once the engine process is up.
extern "C" void OhosInput_StartMonitors()
{
	Input_Result kr = OH_Input_AddKeyEventMonitor(OnKeyEvent);
	Input_Result ar = OH_Input_AddAxisEventMonitorForAll(OnAxisEvent);
	OH_LOG_Print(LOG_APP, LOG_INFO, kDomain, "zeroad/mmi",
		"monitors registered key=%{public}d axis=%{public}d",
		static_cast<int>(kr), static_cast<int>(ar));
}
