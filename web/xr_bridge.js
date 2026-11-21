// SPDX-License-Identifier: MIT
// Thin WebXR 6-DOF bridge connecting browser navigator.xr sessions to C engine hooks.

window.initRtsWebXR = function (Module) {
  const btnVr = document.getElementById('btn-vr');
  const btnTouch = document.getElementById('btn-touch');
  let touchForced = false;
  let xrSession = null;

  if (btnTouch) {
    btnTouch.addEventListener('click', () => {
      touchForced = !touchForced;
      if (Module._rts_web_notify_touch_device) {
        Module._rts_web_notify_touch_device(touchForced ? 1 : 0);
      }
    });
  }

  if (!navigator.xr || !btnVr) return;

  navigator.xr.isSessionSupported('immersive-vr').then((supported) => {
    if (!supported) return;
    btnVr.disabled = false;
    btnVr.addEventListener('click', async () => {
      if (xrSession) {
        await xrSession.end();
        return;
      }
      xrSession = await navigator.xr.requestSession('immersive-vr', {
        optionalFeatures: ['local-floor', 'bounded-floor']
      });
      const gl = Module.canvas.getContext('webgl2', { xrCompatible: true });
      await gl.makeXRCompatible();
      xrSession.updateRenderState({ baseLayer: new XRWebGLLayer(xrSession, gl) });
      if (Module._rts_webxr_on_session_state) {
        Module._rts_webxr_on_session_state(1);
      }
      xrSession.addEventListener('end', () => {
        xrSession = null;
        if (Module._rts_webxr_on_session_state) {
          Module._rts_webxr_on_session_state(0);
        }
      });
    });
  });
};
