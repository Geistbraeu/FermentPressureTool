const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { test } = require('node:test');

const source = fs.readFileSync(
  path.join(__dirname, '..', 'src', 'html_template.h'), 'utf8');
const code = source.slice(
  source.indexOf('let ferminatorButtonPending ='),
  source.indexOf('function renderOledFrame('));

function harness(fetch) {
  const buttons = Array.from({ length: 4 }, () => ({
    disabled: false, addEventListener() {}
  }));
  const status = { textContent: '' };
  const context = vm.createContext({
    document: {
      querySelectorAll: () => buttons,
      getElementById: () => status
    },
    deviceRestartPending: false,
    URLSearchParams,
    AbortSignal,
    setTimeout: resolve => resolve(),
    fetch
  });
  vm.runInContext(code, context);
  return { context, buttons, status };
}

for (const button of ['temp_up', 'temp_down', 'start_stop', 'fan_speed']) {
  test(button + ' sends exactly one POST and unlocks controls', async () => {
    let requests = 0;
    const { context, buttons, status } = harness(async (url, options) => {
      requests++;
      assert.equal(url, '/ferminator/button');
      assert.equal(options.method, 'POST');
      assert.equal(options.body, 'button=' + button);
      assert.ok(buttons.every(control => control.disabled));
      return { ok: true, json: async () => ({ success: true }) };
    });
    await context.pressFerminatorButton(button);
    assert.equal(requests, 1);
    assert.ok(buttons.every(control => !control.disabled));
    assert.match(status.textContent, /pulse sent/);
  });
}

test('overlapping clicks and clicks during restart do not send commands', async () => {
  let requests = 0;
  let finish;
  const { context } = harness(() => {
    requests++;
    return new Promise(resolve => { finish = resolve; });
  });
  const first = context.pressFerminatorButton('start_stop');
  await context.pressFerminatorButton('fan_speed');
  assert.equal(requests, 1);
  finish({ ok: true, json: async () => ({ success: true }) });
  await first;
  context.deviceRestartPending = true;
  await context.pressFerminatorButton('start_stop');
  assert.equal(requests, 1);
});

for (const [name, response] of [
  ['disabled', { ok: false, json: async () => ({ message: 'Ferminator control is disabled' }) }],
  ['busy', { ok: false, json: async () => ({ message: 'Wait before pressing another button' }) }],
  ['invalid response', { ok: true, json: async () => { throw new Error('Invalid JSON'); } }],
  ['timeout', null]
]) {
  test(name + ' is reported, never retried, and unlocks controls', async () => {
    let requests = 0;
    const { context, buttons, status } = harness(async () => {
      requests++;
      if (!response) throw new Error('Timed out');
      return response;
    });
    await context.pressFerminatorButton('start_stop');
    assert.equal(requests, 1);
    assert.ok(buttons.every(control => !control.disabled));
    assert.match(status.textContent, /Unable to confirm/);
    assert.match(status.textContent, /Check the display/);
  });
}
