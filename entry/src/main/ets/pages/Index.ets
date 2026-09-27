import { hilog } from '@kit.PerformanceAnalysisKit';
import { command } from 'libclibridge.so';

const AX_DOMAIN = 0xD00;

@Entry
@Component
struct Index {
  build() {
    Stack() {
      XComponent({ id: 'SDL_XComponent', type: XComponentType.SURFACE, libraryname: 'SDL3' })
        .id('SDL_XComponent')

      // The XComponent surface reliably receives mouse/touch but, on this 2in1,
      // does not receive AxisEvents (touchpad two-finger pan/pinch). Overlay a
      // plain ArkUI column (hit-test transparent so mouse/touch still reach the
      // surface) to capture the axis events and forward them into the engine.
      Column()
        .width('100%')
        .height('100%')
        .hitTestBehavior(HitTestMode.Transparent)
        .onAxisEvent((e: AxisEvent) => {
          const h = e.getHorizontalAxisValue();
          const v = e.getVerticalAxisValue();
          const pinch = e.getPinchAxisScaleValue();
          const hasPinch = e.hasAxis(AxisType.PINCH_AXIS);
          hilog.info(AX_DOMAIN, 'zeroad/axis',
            'OVAXIS act=%{public}d tool=%{public}d src=%{public}d h=%{public}s v=%{public}s pinch=%{public}s hasPinch=%{public}d step=%{public}d',
            e.action, e.sourceTool, e.source, h.toFixed(2), v.toFixed(2), pinch.toFixed(4), hasPinch ? 1 : 0, e.scrollStep ?? 0);
          // payload: tool,action,h,v,pinch,hasPinch
          command('axis', `${e.sourceTool},${e.action},${h},${v},${pinch},${hasPinch ? 1 : 0}`);
        })
    }
    .width('100%')
    .height('100%')
    // The surface window does not reliably hold key focus on 2in1 devices, so
    // capture all key events on the root (which we force-focus) and forward them
    // into the engine through the command bridge.
    .focusable(true)
    .defaultFocus(true)
    .focusOnTouch(true)
    .onKeyEvent((k: KeyEvent) => {
      const down = k.type === KeyType.Down ? 1 : 0;
      hilog.info(AX_DOMAIN, 'zeroad/axis', 'OVKEY code=%{public}d type=%{public}d',
        k.keyCode, k.type);
      command('key', `${k.keyCode},${down}`);
    })
  }
}
