// SPDX-License-Identifier: MIT
// Automated Node.js smoke test that executes the compiled Emscripten WASM binary
// with all loose external assets/scripts mounted into MEMFS and runs all 19 game scenarios.

import fs from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';
import vm from 'node:vm';

const nodeRequire = createRequire(import.meta.url);
const siteDir = path.resolve(process.argv[2] || 'site');
const jsPath = path.join(siteDir, '1prtsg.js');
const manifestPath = path.join(siteDir, 'manifest.json');

if (!fs.existsSync(jsPath)) {
  console.error('[wasm-smoke] Missing:', jsPath);
  process.exit(1);
}

const code = fs.readFileSync(jsPath, 'utf8');
const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));

let finished = false;
const timer = setTimeout(() => {
  if (!finished) {
    console.error('[wasm-smoke] TIMEOUT waiting for WASM runtime');
    process.exit(1);
  }
}, 30000);

const sandbox = {
  console,
  process,
  Buffer,
  require: nodeRequire,
  __dirname: siteDir,
  __filename: jsPath,
  setTimeout,
  clearTimeout,
  setInterval,
  clearInterval,
  URL,
  TextDecoder,
  TextEncoder,
  WebAssembly,
  performance,
  Module: {
    noInitialRun: true,
    locateFile: (p) => path.join(siteDir, p),
    print: (s) => console.log('[wasm]', s),
    printErr: (s) => console.error('[wasm err]', s),
    onRuntimeInitialized() {
      try {
        const M = sandbox.Module;
        const ensureDir = (d) => {
          let cur = '';
          for (const part of d.split('/').filter(Boolean)) {
            cur += '/' + part;
            try { M.FS.mkdir(cur); } catch (_) {}
          }
        };
        for (const rel of manifest.files) {
          const full = '/game/' + rel;
          ensureDir(full.slice(0, full.lastIndexOf('/')));
          M.FS.writeFile(full, fs.readFileSync(path.join(siteDir, rel)));
        }
        M._rts_web_notify_touch_device(1);
        if (M._rts_web_is_touch_device() !== 1) {
          throw new Error('Web touch hook round-trip failed');
        }
        const rc = M.callMain(['--headless', '--data-dir', '/game', '--test-script', 'tests/game/run_all.lua']);
        if (rc !== 0) {
          throw new Error('WASM callMain returned non-zero exit code: ' + rc);
        }
        finished = true;
        clearTimeout(timer);
        console.log('[wasm-smoke] PASS: 1prtsg.wasm booted cleanly, mounted', manifest.files.length, 'loose files, and passed all 20 game scenarios!');
        process.exit(0);
      } catch (err) {
        finished = true;
        clearTimeout(timer);
        console.error('[wasm-smoke] FAIL:', err);
        process.exit(1);
      }
    }
  }
};

vm.runInNewContext(code, sandbox);
