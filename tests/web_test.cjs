// Exercise the actual embedded script with a simulated DOM and WebSocket.
// This checks behavior; it is not a browser rendering or hardware test.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../include/web_page.h'), 'utf8');
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];

class Element {
  constructor() { this.events = {}; this.style = {}; this.attrs = {}; this.clientWidth = 320; this.value = '50'; }
  addEventListener(name, callback) { this.events[name] = callback; }
  setAttribute(name, value) { this.attrs[name] = value; }
  getBoundingClientRect() { return {left: 0, top: 0, width: 320, height: 320}; }
  setPointerCapture(pointer) { this.capture = pointer; }
  hasPointerCapture(pointer) { return this.capture === pointer; }
  releasePointerCapture(pointer) {
    this.capture = null;
    this.events.lostpointercapture?.({pointerId: pointer});
  }
}
const ids = Object.fromEntries(['pad', 'knob', 'status', 'values', 'speed', 'speedValue', 'stop']
  .map(id => [id, new Element()]));
const document = new Element(); document.hidden = false;
document.querySelector = id => ids[id.slice(1)];
const window = new Element();
let now = 0, heartbeat;
const sockets = [];
const timers = new Map(); let nextTimer = 0;
class FakeSocket {
  static OPEN = 1;
  constructor(url) { this.url = url; this.readyState = 1; this.bufferedAmount = 0; this.sent = []; sockets.push(this); }
  send(data) { this.sent.push(data); }
  close() { this.readyState = 3; this.onclose?.(); }
  message(data) { this.onmessage({data}); }
}
vm.runInNewContext(script, {document, window, location: {hostname: '192.168.4.1'},
  WebSocket: FakeSocket, performance: {now: () => now},
  setInterval: callback => { heartbeat = callback; },
  setTimeout: callback => { timers.set(++nextTimer, callback); return nextTimer; },
  clearTimeout: id => timers.delete(id)});
const pad = ids.pad;
const down = (id = 1) => pad.events.pointerdown({pointerId: id, button: 0,
  clientX: 160, clientY: 45, preventDefault() {}});
const latest = () => sockets[sockets.length - 1];
const last = () => latest().sent.at(-1);

assert.equal(latest().url, 'ws://192.168.4.1:81/');
latest().message('disabled'); down(); assert.equal(last(), '0,0');
latest().message('ready'); assert.equal(last(), '0,0');
down(); assert.match(last(), /^0,49\d$/); // Up, at default half speed.
down(2); pad.events.pointerup({pointerId: 2}); // Other pointers cannot stop/control this gesture.
assert.notEqual(last(), '0,0');
pad.events.pointerup({pointerId: 1}); assert.equal(last(), '0,0');
for (const event of ['pointercancel', 'lostpointercapture']) {
  down(); pad.events[event]({pointerId: 1}); assert.equal(last(), '0,0');
}
down(); window.events.blur(); assert.equal(last(), '0,0');
down(); ids.stop.events.click(); assert.equal(last(), '0,0');
down(); window.events.keydown({key: 'Escape', preventDefault() {}}); assert.equal(last(), '0,0');
down(); latest().message('release'); assert.equal(last(), '0,0');
// A hidden page sends neutral, closes the socket, and cannot drive.
down(); const previous = latest(); document.hidden = true; document.events.visibilitychange();
assert.equal(previous.sent.at(-1), '0,0'); assert.equal(previous.readyState, 3);
document.hidden = false; document.events.visibilitychange();
assert.notEqual(latest(), previous);
latest().message('ready'); heartbeat(); assert.equal(last(), '0,0');
// Missing acknowledgments close the socket and clear the gesture at 250 ms.
down(); now = 250; heartbeat(); assert.equal(latest().readyState, 3);
assert.match(ids.status.textContent, /Disconnected/);
// Reconnect starts neutral; a stale pointer move cannot resume driving.
document.events.visibilitychange(); latest().message('ready');
pad.events.pointermove({pointerId: 1, clientX: 160, clientY: 45});
heartbeat(); assert.equal(last(), '0,0');
// Congestion aborts rather than appending more motion commands.
down(); latest().bufferedAmount = 1; heartbeat(); assert.equal(latest().readyState, 3);
console.log('Web controller behavior checks passed.');
