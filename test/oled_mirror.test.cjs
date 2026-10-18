const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { test } = require('node:test');

const source = fs.readFileSync(
  path.join(__dirname, '..', 'src', 'html_template.h'), 'utf8');
const renderingCode = source.slice(
  source.indexOf('function renderOledFrame('),
  source.indexOf('async function refreshLiveData('));

function harness(response, debugInfoEnabled = true) {
  const pixels = [];
  const context = vm.createContext({
    oledContext: {
      fillStyle: '',
      fillRect(x, y, width, height) {
        if (width === 1 && height === 1) pixels.push([x, y]);
      }
    },
    oledStatusEl: { textContent: '' },
    oledTimingEl: { textContent: '' },
    oledDebugInfoEnabled: debugInfoEnabled,
    lastOledSequence: -1,
    fetch: async () => response
  });
  vm.runInContext(renderingCode, context);
  return { context, pixels };
}

const normal = {
  oledKnownState: 127,
  oledDisplayEnabled: true,
  oledSegmentRemap: true,
  oledComScanReverse: true
};

test('SH1106 A1/C8 renders asymmetric corner pixels upright', () => {
  const { context, pixels } = harness();
  const frame = new Uint8Array(1024);
  frame[0] = 1;
  frame[1023] = 128;
  context.renderOledFrame(frame, normal);
  assert.deepEqual(pixels, [[0, 0], [127, 63]]);
});

test('SH1106 A0/C0 reverses both panel axes', () => {
  const { context, pixels } = harness();
  const frame = new Uint8Array(1024);
  frame[0] = 1;
  context.renderOledFrame(frame, {
    ...normal, oledSegmentRemap: false, oledComScanReverse: false
  });
  assert.deepEqual(pixels, [[127, 63]]);
});

test('start line and display offset wrap without losing pixels', () => {
  const { context, pixels } = harness();
  const frame = new Uint8Array(1024);
  frame[0] = 1;
  context.renderOledFrame(frame, {
    ...normal, oledStartLine: 1, oledDisplayOffset: 2
  });
  assert.deepEqual(pixels, [[0, 1]]);
});

test('display off is black; entire-display-on overrides inversion', () => {
  const { context, pixels } = harness();
  const frame = new Uint8Array(1024);
  context.renderOledFrame(frame, { ...normal, oledDisplayEnabled: false });
  assert.equal(pixels.length, 0);
  context.renderOledFrame(frame, {
    ...normal, oledDisplayAllOn: true, oledInverse: true
  });
  assert.equal(pixels.length, 8192);
});

function frameResponse(headers) {
  const frame = new Uint8Array(1024);
  frame[0] = 1;
  return {
    ok: true,
    headers: new Map(Object.entries({ 'X-OLED-Known-State': '127', ...headers })),
    arrayBuffer: async () => frame.buffer
  };
}

test('pixels use metadata from /oled, not the earlier /api snapshot', async () => {
  const { context, pixels } = harness(frameResponse({
    'X-OLED-Flags': '25',
    'X-OLED-Sequence': '42',
    'X-OLED-Start-Line': '0',
    'X-OLED-Display-Offset': '0'
  }));
  await context.refreshOledMirror({
    oledReady: true, oledSequence: 41, oledSegmentRemap: false,
    oledComScanReverse: false
  });
  assert.deepEqual(pixels, [[0, 0]]);
  assert.equal(context.lastOledSequence, 42);
});

test('missing frame metadata is reported and does not mark the frame as read', async () => {
  const { context, pixels } = harness(frameResponse({}));
  await context.refreshOledMirror({ oledReady: true, oledSequence: 1 });
  assert.match(context.oledStatusEl.textContent, /Invalid.*metadata/);
  assert.equal(context.lastOledSequence, -1);
  assert.equal(pixels.length, 0);
});

test('invalid flags and offsets are rejected rather than rendered', async () => {
  for (const [name, value] of [
    ['X-OLED-Flags', '32'], ['X-OLED-Display-Offset', '64'],
    ['X-OLED-Start-Line', 'NaN'], ['X-OLED-Sequence', '-1'],
    ['X-OLED-Known-State', '128']
  ]) {
    const { context, pixels } = harness(frameResponse({
      'X-OLED-Flags': '25',
      'X-OLED-Sequence': '42',
      'X-OLED-Start-Line': '0',
      'X-OLED-Display-Offset': '0',
      [name]: value
    }));
    await context.refreshOledMirror({ oledReady: true, oledSequence: 1 });
    assert.match(context.oledStatusEl.textContent, /Invalid.*metadata/);
    assert.equal(context.lastOledSequence, -1);
    assert.equal(pixels.length, 0);
  }
});

test('an unavailable capture is reported and retried', async () => {
  const { context } = harness({ ok: false });
  let requests = 0;
  context.fetch = async () => { requests++; return { ok: false }; };
  await context.refreshOledMirror({ oledReady: true, oledSequence: 1 });
  await context.refreshOledMirror({ oledReady: true, oledSequence: 1 });
  assert.equal(requests, 2);
  assert.equal(context.lastOledSequence, -1);
  assert.match(context.oledStatusEl.textContent, /Unable to read/);
});

test('loss of synchronization is visible even without ring buffer overflows', async () => {
  const { context, pixels } = harness();
  await context.refreshOledMirror({
    oledReady: false, oledCapturedBytes: 128,
    oledOverflows: 0, oledCaptureErrors: 17
  });
  assert.match(context.oledStatusEl.textContent, /128\/1024/);
  assert.match(context.oledStatusEl.textContent, /synchronization errors: 17/);
  assert.equal(pixels.length, 0);
});

test('missing initialization shows raw RAM rather than treating the panel as off', () => {
  const { context, pixels } = harness();
  const frame = new Uint8Array(1024);
  frame[0] = 1;
  context.renderOledFrame(frame, { oledKnownState: 0, oledDisplayEnabled: false });
  assert.deepEqual(pixels, [[0, 0]]);
});

test('an observed display-off command is respected even before full initialization', () => {
  const { context, pixels } = harness();
  const frame = new Uint8Array(1024);
  frame[0] = 1;
  context.renderOledFrame(frame, { oledKnownState: 1, oledDisplayEnabled: false });
  assert.equal(pixels.length, 0);
});

test('RAM preview is explicitly labelled when initialization was missed', async () => {
  const { context, pixels } = harness(frameResponse({
    'X-OLED-Flags': '0', 'X-OLED-Sequence': '1',
    'X-OLED-Start-Line': '0', 'X-OLED-Display-Offset': '0',
    'X-OLED-Known-State': '0'
  }));
  await context.refreshOledMirror({ oledReady: true, oledSequence: 1, oledKnownState: 0 });
  assert.deepEqual(pixels, [[0, 0]]);
  assert.match(context.oledStatusEl.textContent, /RAM preview.*initialization/);
});

test('a capture error retains the last complete frame and reports reacquisition', async () => {
  const { context, pixels } = harness();
  context.lastOledSequence = 42;
  let requests = 0;
  context.fetch = async () => { requests++; throw new Error('Unexpected fetch'); };
  await context.refreshOledMirror({
    oledReady: true, oledSequence: 42, oledKnownState: 127,
    oledRecovering: true, oledCapturedBytes: 512,
    oledCaptureErrors: 3, oledDroppedFrames: 2
  });
  assert.equal(requests, 0);
  assert.equal(pixels.length, 0);
  assert.equal(context.lastOledSequence, 42);
  assert.match(context.oledStatusEl.textContent, /Holding last complete frame.*512\/1024/);
  assert.match(context.oledStatusEl.textContent, /rejected refreshes: 2/);
});

test('live polling cannot overlap and releases its lock after a failed request', async () => {
  let finishRequest;
  let requests = 0;
  const context = vm.createContext({
    liveDataRefreshInProgress: false,
    fetch: () => {
      requests++;
      return new Promise(resolve => { finishRequest = resolve; });
    }
  });
  vm.runInContext(source.slice(
    source.indexOf('async function refreshLiveData('),
    source.indexOf('async function submitSettingsForm(')), context);
  const first = context.refreshLiveData();
  await context.refreshLiveData();
  assert.equal(requests, 1);
  finishRequest({ ok: false });
  await first;
  const second = context.refreshLiveData();
  assert.equal(requests, 2);
  finishRequest({ ok: false });
  await second;
  assert.equal(context.liveDataRefreshInProgress, false);
});

test('timing shows count, duration, completed gaps, live wait and long-gap buckets', () => {
  const { context } = harness();
  context.renderOledTiming({
    frames: 4, intervals: 3, observedMs: 1800500,
    lastIntervalMs: 4000, minIntervalMs: 1000,
    meanIntervalMs: 2333.33, maxIntervalMs: 4000, currentGapMs: 35000,
    buckets: [1, 1, 1, 0, 0, 0]
  });
  assert.match(context.oledTimingEl.textContent, /Good frames: 4.*observed: 30m 0s/);
  assert.match(context.oledTimingEl.textContent, /4\.000s \/ 1\.000s \/ 2\.333s \/ 4\.000s/);
  assert.match(context.oledTimingEl.textContent, /current wait: 35\.000s/);
  assert.match(context.oledTimingEl.textContent, /1 \/ 1 \/ 1 \/ 0 \/ 0 \/ 0/);
});

test('a single frame does not invent a zero-length interval', () => {
  const { context } = harness();
  context.renderOledTiming({
    frames: 1, intervals: 0, observedMs: 1000,
    lastIntervalMs: null, minIntervalMs: null,
    meanIntervalMs: null, maxIntervalMs: null, currentGapMs: 1000,
    buckets: [0, 0, 0, 0, 0, 0]
  });
  assert.match(context.oledTimingEl.textContent, /waiting for two complete frames/);
  assert.doesNotMatch(context.oledTimingEl.textContent, /NaN|gaps last/);
});

test('inconsistent timing counters are reported rather than displayed as valid', () => {
  const { context } = harness();
  context.renderOledTiming({
    frames: 5, intervals: 4, observedMs: 4000,
    lastIntervalMs: 1000, minIntervalMs: 1000,
    meanIntervalMs: 1000, maxIntervalMs: 1000, currentGapMs: 0,
    buckets: [3, 0, 0, 0, 0, 0]
  });
  assert.match(context.oledTimingEl.textContent, /statistics unavailable/);
});

test('debug display defaults to off and is wired into the page', () => {
  const config = fs.readFileSync(
    path.join(__dirname, '..', 'src', 'config.h'), 'utf8');
  assert.match(config, /constexpr bool SHOW_DEBUG_INFO = false;/);
  assert.match(source, /html \+= OledMirrorConfig::SHOW_DEBUG_INFO \? "true" : "false";/);
  assert.match(source, /id="oled-timing"[^>]* hidden/);

  for (const enabled of [false, true]) {
    const timing = { hidden: true };
    const context = vm.createContext({
      document: {
        getElementById(id) {
          if (id === 'oled-timing') return timing;
          if (id === 'oled-screen') return {
            closest: () => ({ dataset: { debugInfo: String(enabled) } }),
            getContext: () => null
          };
          return null;
        }
      }
    });
    vm.runInContext(source.slice(
      source.indexOf('const oledCanvas ='),
      source.indexOf('function showStatus(')), context);
    assert.equal(timing.hidden, !enabled);
  }
});

test('debug off hides timing and capture counters without hiding recovery status', async () => {
  const { context } = harness(undefined, false);
  await context.refreshOledMirror({
    oledReady: false, oledCapturedBytes: 128, oledCaptureErrors: 17
  });
  assert.equal(context.oledStatusEl.textContent, 'Waiting for a complete display frame...');
  assert.equal(context.oledTimingEl.textContent, '');

  context.lastOledSequence = 42;
  await context.refreshOledMirror({
    oledReady: true, oledSequence: 42, oledKnownState: 127,
    oledRecovering: true, oledCapturedBytes: 512,
    oledCaptureErrors: 3, oledDroppedFrames: 2
  });
  assert.equal(context.oledStatusEl.textContent, 'Holding last complete frame · reacquiring');
});

test('debug off still renders frames and reports failed frame requests', async () => {
  const { context, pixels } = harness(frameResponse({
    'X-OLED-Flags': '25', 'X-OLED-Sequence': '42',
    'X-OLED-Start-Line': '0', 'X-OLED-Display-Offset': '0'
  }), false);
  await context.refreshOledMirror({ oledReady: true, oledSequence: 42 });
  assert.deepEqual(pixels, [[0, 0]]);
  assert.equal(context.oledStatusEl.textContent, 'Mirroring active');

  context.fetch = async () => { throw new Error('Network offline'); };
  await context.refreshOledMirror({ oledReady: true, oledSequence: 43 });
  assert.match(context.oledStatusEl.textContent, /Network error/);
  assert.equal(context.lastOledSequence, 42);
});
