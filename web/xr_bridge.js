// SPDX-License-Identifier: MIT
// Full 6-DOF WebXR Session Loop (XRViewerPose eye matrices + XRInputSource hand rays/axes + VR Comfort)
// & 2-Finger Mobile Pinch-Zoom / Drag-Rotate Holo-Globe Bridge.

window.initRtsWebXR = function (Module, initialTouch) {
  const btnVr = document.getElementById('btn-vr');
  const btnTouch = document.getElementById('btn-touch');
  const canvas = Module.canvas || document.getElementById('canvas');
  let touchForced = Boolean(initialTouch);
  let xrSession = null;
  let xrRefSpace = null;
  let comfortVignette = 1;
  let seatedMode = 0;

  const updateTouchLabel = () => {
    if (btnTouch) btnTouch.textContent = touchForced ? 'Touch HUD: ON' : 'Touch HUD';
  };
  updateTouchLabel();

  if (btnTouch) {
    btnTouch.addEventListener('click', (e) => {
      e.currentTarget.blur();
      touchForced = !touchForced;
      updateTouchLabel();
      if (Module._rts_web_notify_touch_device) {
        Module._rts_web_notify_touch_device(touchForced ? 1 : 0);
      }
    });
  }

  // 2-Finger Mobile Pinch-Zoom & Rotate Gesture for the 3D Holo-Globe
  if (canvas) {
    const activeTouches = new Map();
    const emitPinch = (active) => {
      if (!Module._rts_web_on_pinch_gesture) return;
      if (!active || activeTouches.size < 2) {
        Module._rts_web_on_pinch_gesture(0, 0, 0);
        return;
      }
      const pts = Array.from(activeTouches.values());
      const dx = pts[1].x - pts[0].x;
      const dy = pts[1].y - pts[0].y;
      const normScale = Math.max(Math.min(window.innerWidth, window.innerHeight), 1);
      const dist = Math.hypot(dx, dy) / normScale;
      const angle = Math.atan2(dy, dx);
      Module._rts_web_on_pinch_gesture(dist, angle, 1);
    };
    canvas.addEventListener('touchstart', (e) => {
      for (const t of e.changedTouches) activeTouches.set(t.identifier, { x: t.clientX, y: t.clientY });
      if (activeTouches.size >= 2) emitPinch(true);
    }, { passive: true });
    canvas.addEventListener('touchmove', (e) => {
      for (const t of e.changedTouches) activeTouches.set(t.identifier, { x: t.clientX, y: t.clientY });
      if (activeTouches.size >= 2) emitPinch(true);
    }, { passive: true });
    const endTouch = (e) => {
      for (const t of e.changedTouches) activeTouches.delete(t.identifier);
      if (activeTouches.size < 2) emitPinch(false);
    };
    canvas.addEventListener('touchend', endTouch, { passive: true });
    canvas.addEventListener('touchcancel', endTouch, { passive: true });
  }

  if (!navigator.xr || !btnVr) return;

  navigator.xr.isSessionSupported('immersive-vr').then((supported) => {
    if (!supported) return;
    btnVr.disabled = false;
    btnVr.addEventListener('click', async (e) => {
      e.currentTarget.blur();
      if (xrSession) {
        await xrSession.end();
        return;
      }
      xrSession = await navigator.xr.requestSession('immersive-vr', {
        optionalFeatures: ['local-floor', 'bounded-floor', 'hand-tracking']
      });
      const gl = canvas.getContext('webgl2', { xrCompatible: true });
      await gl.makeXRCompatible();
      xrSession.updateRenderState({ baseLayer: new XRWebGLLayer(xrSession, gl) });
      xrRefSpace = await xrSession.requestReferenceSpace('local-floor').catch(() =>
        xrSession.requestReferenceSpace('local')
      );

      // Allocate 4x16 floats in WASM heap for left/right eye view + projection matrices
      const matPtr = Module._malloc ? Module._malloc(64 * 4) : 0;

      if (Module._rts_webxr_on_session_state) {
        Module._rts_webxr_on_session_state(1);
      }

      const onXRFrame = (time, frame) => {
        if (!xrSession) return;
        xrSession.requestAnimationFrame(onXRFrame);
        const pose = frame.getViewerPose(xrRefSpace);
        if (!pose || pose.views.length < 2 || !Module._rts_webxr_submit_pose) return;

        const hp = pose.transform.position;
        let lhx = hp.x - 0.35, lhy = hp.y - 0.35, lhz = hp.z - 0.45;
        let rox = hp.x + 0.35, roy = hp.y - 0.30, roz = hp.z - 0.40;
        let rdx = 0, rdy = 0, rdz = -1;
        let trigger = 0, lsx = 0, lsy = 0, rsx = 0;

        for (const src of xrSession.inputSources) {
          const gp = src.gamepad;
          if (src.handedness === 'left') {
            const sp = frame.getPose(src.gripSpace || src.targetRaySpace, xrRefSpace);
            if (sp) { lhx = sp.transform.position.x; lhy = sp.transform.position.y; lhz = sp.transform.position.z; }
            if (gp && gp.axes.length >= 4) { lsx = gp.axes[2] || 0; lsy = -(gp.axes[3] || 0); }
          } else if (src.handedness === 'right') {
            const sp = frame.getPose(src.targetRaySpace, xrRefSpace);
            if (sp) {
              rox = sp.transform.position.x; roy = sp.transform.position.y; roz = sp.transform.position.z;
              const m = sp.transform.matrix;
              rdx = -m[8]; rdy = -m[9]; rdz = -m[10];
            }
            if (gp) {
              if (gp.buttons.length > 0 && gp.buttons[0].pressed) trigger = 1;
              if (gp.axes.length >= 4) rsx = gp.axes[2] || 0;
            }
          }
        }

        let lvPtr = 0, lpPtr = 0, rvPtr = 0, rpPtr = 0;
        if (matPtr && Module.HEAPF32) {
          const base = matPtr >> 2;
          Module.HEAPF32.set(pose.views[0].transform.inverse.matrix, base);
          Module.HEAPF32.set(pose.views[0].projectionMatrix, base + 16);
          Module.HEAPF32.set(pose.views[1].transform.inverse.matrix, base + 32);
          Module.HEAPF32.set(pose.views[1].projectionMatrix, base + 48);
          lvPtr = matPtr; lpPtr = matPtr + 64; rvPtr = matPtr + 128; rpPtr = matPtr + 192;
        }

        Module._rts_webxr_submit_pose(
          hp.x, hp.y, hp.z, lvPtr, lpPtr, rvPtr, rpPtr,
          lhx, lhy, lhz, rox, roy, roz, rdx, rdy, rdz,
          trigger, lsx, lsy, rsx, comfortVignette, seatedMode
        );
      };

      xrSession.requestAnimationFrame(onXRFrame);
      xrSession.addEventListener('end', () => {
        if (matPtr && Module._free) Module._free(matPtr);
        xrSession = null;
        xrRefSpace = null;
        if (Module._rts_webxr_on_session_state) {
          Module._rts_webxr_on_session_state(0);
        }
      });
    });
  }).catch(() => {});
};
