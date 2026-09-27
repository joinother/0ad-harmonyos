import { abilityAccessCtrl, AbilityConstant, ConfigurationConstant, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { i18n } from '@kit.LocalizationKit';
import { inputMethod } from '@kit.IMEKit';
import pointer from "@ohos.multimodalInput.pointer"
import sdl from 'libSDL3.so';
import { command } from 'libclibridge.so';

const DOMAIN = 0x0000;
const PAYLOAD_PREFIX = '__ZEROAD_CLI_PAYLOAD_URI__';

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(DOMAIN, 'SDL/STARTUP', '%{public}s', 'Handing control from ArkTS to native SDL.');
    try {
        // This hands stuff to SDL's C code that it needs to deal with ArkTS interfaces. We are trying not to write scripting code, if we can
        //  handle it awkwardly in C through NAPI calls, so we have a _tiny_ surface area of risk where the script and the native code can go
        //  out of sync. This function call will add methods to `this`, such as onWindowStageCreate(), so we can drive everything from C and
        //  have almost no ArkTS code at all.
        // Which is to say: if this function call fails, your app won't actually start.
        //
        // ONLY EVER ADD PARAMETERS TO THIS FUNCTION CALL. THE C CODE WILL ABORT IF IT DOESN'T GET THE EXACT NUMBER IT EXPECTS.
        sdl.provideArkTSObjects(this, abilityAccessCtrl.createAtManager(), i18n.System, pointer.setPointerVisibleSync, inputMethod.getController());
    } catch (e) {
        hilog.info(DOMAIN, 'SDL/STARTUP', '%{public}s', 'Exception thrown! App will not start.');
    }
    this.handleCliWant(want);
  }

  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    this.handleCliWant(want);
  }

  // Open an external http(s) URL in the default browser. context.openLink goes
  // through App Linking and can silently fail; an implicit viewData Want is the
  // well-supported path. Errors are logged so a missing browser is visible.
  openExternalUrl(url: string, cb: () => void): void {
    try {
      const openWant: Want = { action: 'ohos.want.action.viewData', uri: url };
      this.context.startAbility(openWant)
        .then(() => { try { cb(); } catch (e) {} })
        .catch((err: Error) => {
          hilog.error(0xD00, 'zeroad/url', 'openExternalUrl failed: %{public}s', String(err));
          try { cb(); } catch (e) {}
        });
    } catch (e) {
      hilog.error(0xD00, 'zeroad/url', 'openExternalUrl threw: %{public}s', String(e));
      try { cb(); } catch (e2) {}
    }
  }

  // Parse a CLI command carried in the Want parameters, run it through the
  // native bridge, and stream the JSON response back over hilog in chunks.
  private handleCliWant(want: Want): void {
    const parameters = want?.parameters as Record<string, Object> | undefined;
    try {
      const keys = parameters ? Object.keys(parameters).join(',') : '(none)';
      hilog.info(DOMAIN, 'zeroad/cli', 'handleCliWant entered; keys=%{public}s', keys);
    } catch (e) {}
    if (!parameters) {
      return;
    }

    const name = String(parameters['zeroad.cli.command'] ?? '').trim();
    if (!name || !/^[A-Za-z][A-Za-z0-9_]*$/.test(name)) {
      return;
    }

    let payload = String(parameters['zeroad.cli.payload'] ?? '');
    if (payload.startsWith(PAYLOAD_PREFIX)) {
      try {
        payload = decodeURIComponent(payload.substring(PAYLOAD_PREFIX.length));
      } catch (e) {
        hilog.warn(DOMAIN, 'zeroad/cli', 'rejected malformed payload');
        return;
      }
    }

    const requestId = String(parameters['zeroad.cli.requestId'] ?? `cli-${Date.now()}`);

    let response = '';
    try {
      response = command(name, payload);
    } catch (e) {
      response = JSON.stringify({ ok: false, error: String(e) });
    }

    const chunkSize = 600;
    const chunkCount = Math.max(1, Math.ceil(response.length / chunkSize));
    for (let i = 0; i < chunkCount; i += 1) {
      const chunk = response.substring(i * chunkSize, (i + 1) * chunkSize);
      hilog.info(DOMAIN, 'zeroad/cli',
        '[cli-response] requestId=%{public}s command=%{public}s chunkIndex=%{public}d chunkCount=%{public}d responseChunk=%{public}s',
        requestId, name, i, chunkCount, chunk);
    }
  }
}
