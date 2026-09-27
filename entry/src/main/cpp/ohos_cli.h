/*
 * HarmonyOS in-app command bridge (CLI), modeled on the Stellarium port.
 *
 * Commands arrive from ArkTS on the UI/napi thread via OhosCli_Dispatch,
 * which blocks until the engine main thread runs them in OhosCli_Pump (called
 * once per frame). Results are JSON strings.
 */
#ifndef INCLUDED_OHOS_CLI_H
#define INCLUDED_OHOS_CLI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Run a command on the engine main thread. Returns a malloc'd JSON string the
 * caller must free with OhosCli_Free. timeoutMs bounds the wait. */
char* OhosCli_Dispatch(const char* name, const char* payload, int timeoutMs);
void  OhosCli_Free(char* result);

/* Called by the engine main thread once per frame; executes queued commands. */
void  OhosCli_Pump(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_OHOS_CLI_H */
