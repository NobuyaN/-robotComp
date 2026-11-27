#pragma once
#include <pgmspace.h>

const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>RobotCar controller</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#101820;color:#edf6fc;font:17px system-ui,sans-serif;
min-height:100svh;display:grid;place-items:center;padding:24px}main{width:min(100%,480px);text-align:center}
h1{margin:0 0 8px;font-size:2rem}p{line-height:1.5;color:#b9c9d7}#status{min-height:3em}
#pad{width:min(78vw,320px);aspect-ratio:1;border-radius:50%;margin:24px auto;background:#1c2d3c;
border:2px solid #4b6579;position:relative;touch-action:none;user-select:none;cursor:grab;outline-offset:6px}
#pad:before,#pad:after{content:"";position:absolute;background:#4b6579;pointer-events:none}
#pad:before{width:1px;height:100%;left:50%}#pad:after{height:1px;width:100%;top:50%}
#knob{position:absolute;width:28%;height:28%;left:36%;top:36%;border-radius:50%;background:#54d2b0;
box-shadow:0 4px 18px #0006;z-index:1;pointer-events:none}label{display:block;margin:20px 0}
input{width:100%;margin-top:16px;accent-color:#54d2b0}button{width:100%;padding:17px;border:0;
border-radius:12px;background:#f16868;color:#171a20;font:700 18px system-ui;cursor:pointer}
output{font-variant-numeric:tabular-nums}small{color:#b9c9d7}#pad[aria-disabled="true"]{opacity:.45}
</style></head><body><main>
<h1>RobotCar</h1><p id="status" role="status" aria-live="polite">Connecting…</p>
<div id="pad" tabindex="0" role="application" aria-label="Joystick: drag to drive; up is forward. Release to stop."
aria-disabled="true"><div id="knob"></div></div>
<output id="values">X 0 · Y 0</output>
<label>Maximum speed: <output id="speedValue">50%</output>
<input id="speed" type="range" min="10" max="100" value="50" aria-label="Maximum speed"></label>
<button id="stop" type="button">STOP</button>
<p>Drag and hold to drive. Up/down: forward/reverse.<br>Left/right: turn. Release to stop.</p>
<small>Keep this page visible. One controller at a time.<br>After a connection loss, release before driving again.</small>
</main><script>
'use strict';
const pad=document.querySelector('#pad'), knob=document.querySelector('#knob');
const status=document.querySelector('#status'), values=document.querySelector('#values');
const speed=document.querySelector('#speed');
let ws, pointer=null, x=0, y=0, lastAck=0, ready=false, enabled=false, blocked=false;
let reconnectTimer;
function display(){
  const r=pad.clientWidth*.36;
  knob.style.transform=`translate(${x*r}px,${-y*r}px)`;
  values.textContent=`X ${Math.round(x*1000)} · Y ${Math.round(y*1000)}`;
}
function send(){
  if(!ready || !ws || ws.readyState!==WebSocket.OPEN)return;
  if(ws.bufferedAmount>0){ fail('Connection congested; release to reconnect.');return; }
  const scale=Number(speed.value)*10;
  ws.send(`${Math.round(x*scale)},${Math.round(y*scale)}`);
}
function release(transmit=true){
  const old=pointer;pointer=null;x=y=0;display();
  if(old!==null && pad.hasPointerCapture(old))pad.releasePointerCapture(old);
  if(transmit)send();
}
function fail(message){
  ready=false;pad.setAttribute('aria-disabled','true');release(false);
  status.textContent=message;
  if(ws)ws.close();
}
function connect(){
  clearTimeout(reconnectTimer);
  if(document.hidden || blocked)return;
  const connection=new WebSocket(`ws://${location.hostname}:81/`);ws=connection;
  connection.onmessage=event=>{
    if(ws!==connection)return;
    if(event.data==='busy'){
      blocked=true;fail('Another controller is connected. Close it, then reload this page.');return;
    }
    if(event.data==='ready' || event.data==='disabled'){
      enabled=event.data==='ready';ready=true;lastAck=performance.now();
      pad.setAttribute('aria-disabled',String(!enabled));
      status.textContent=enabled?'Connected — hold joystick to drive.':'Motor output disabled. Verify wiring and enable it in config.h.';
      release();return;
    }
    if(event.data==='ok' || event.data==='release'){
      lastAck=performance.now();
      if(event.data==='release'){
        release();status.textContent='Stopped — release and press the joystick again.';
      }
    }
  };
  connection.onclose=()=>{
    if(ws!==connection)return;
    ready=false;pad.setAttribute('aria-disabled','true');release(false);
    if(!blocked){status.textContent='Disconnected — motors stop automatically.';
      reconnectTimer=setTimeout(connect,1000);}
  };
  connection.onerror=()=>connection.close();
}
function move(event){
  if(pointer!==event.pointerId)return;
  const b=pad.getBoundingClientRect(),r=b.width*.36;
  x=(event.clientX-b.left-b.width/2)/r;y=-(event.clientY-b.top-b.height/2)/r;
  const magnitude=Math.hypot(x,y);if(magnitude>1){x/=magnitude;y/=magnitude;}
  display();
}
pad.addEventListener('pointerdown',event=>{
  if(!ready || !enabled || pointer!==null || event.button!==0)return;
  event.preventDefault();pointer=event.pointerId;pad.setPointerCapture(pointer);move(event);send();
});
pad.addEventListener('pointermove',move);
for(const name of ['pointerup','pointercancel','lostpointercapture']){
  pad.addEventListener(name,event=>{if(event.pointerId===pointer)release();});
}
pad.addEventListener('contextmenu',event=>event.preventDefault());
document.querySelector('#stop').addEventListener('click',()=>release());
window.addEventListener('keydown',event=>{if(event.key==='Escape' || event.code==='Space'){
  event.preventDefault();release();}});
speed.addEventListener('input',()=>{document.querySelector('#speedValue').textContent=`${speed.value}%`;});
window.addEventListener('blur',()=>release());
document.addEventListener('visibilitychange',()=>{
  if(document.hidden){release();fail('Paused while page is hidden.');}else if(!ws || ws.readyState>1)connect();
});
window.addEventListener('pagehide',()=>{release();if(ws)ws.close();});
setInterval(()=>{
  if(!ready)return;
  if(performance.now()-lastAck>=250){fail('Connection timed out — release to reconnect.');return;}
  send();
},50);
display();connect();
</script></body></html>)HTML";
