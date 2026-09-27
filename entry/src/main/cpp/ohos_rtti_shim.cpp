// HarmonyOS build shim.
//
// SpiderMonkey's own translation units are built without RTTI, so
// libjs_static.a contains the js::BaseProxyHandler methods/vtable but no
// type_info. 0 A.D.'s GUI proxy handlers (gui/Scripting/JSInterface_GUIProxy)
// inherit js::BaseProxyHandler and the compiler emits references to its
// type_info when building their RTTI.
//
// Including the complete class definition in this RTTI-enabled translation
// unit makes the compiler emit the missing typeinfo as a weak definition,
// which satisfies the final link without rebuilding SpiderMonkey.

#include <js/Proxy.h>

#include <typeinfo>

namespace js
{
static const void* forceBaseProxyHandlerRTTI =
	static_cast<const void*>(&typeid(::js::BaseProxyHandler));
}
