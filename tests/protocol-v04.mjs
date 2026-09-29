import fs from 'node:fs';
import assert from 'node:assert/strict';
const report=JSON.parse(fs.readFileSync(process.argv[2],'utf8'));
const base='http://127.0.0.1:47654';const headers={'X-DeskHalo-Code':report.code};
const checks=[];
async function check(name,fn){await fn();checks.push(name);console.log('PASS '+name);}
await check('pairing required',async()=>assert.equal((await fetch(base+'/files')).status,401));
await check('v0.4 display inventory',async()=>{const j=await fetch(base+'/hello',{headers}).then(r=>r.json());assert.equal(j.version,'0.4.0');assert(j.displays.length>0);console.log(JSON.stringify(j.displays));});
await check('outbox list',async()=>assert(Array.isArray(await fetch(base+'/files',{headers}).then(r=>r.json()))));
for(const name of ['../outside.txt','..\\outside.txt','CON','NUL.txt','bad:name','trailing.'])await check('reject unsafe name '+name,async()=>assert.equal((await fetch(base+'/files/upload?name='+encodeURIComponent(name),{method:'POST',headers,body:'test'})).status,400));
await check('bounded authenticated upload',async()=>{const r=await fetch(base+'/files/upload?name=DeskHalo-transfer-test.txt',{method:'POST',headers,body:'DeskHalo v0.4 transfer test. Safe to delete.\n'});assert.equal(r.status,200);assert((await r.json()).name.endsWith('DeskHalo-transfer-test.txt'));});
await check('download traversal blocked',async()=>assert.equal((await fetch(base+'/files/download?name='+encodeURIComponent('../outside.txt'),{headers})).status,400));
await check('invalid pointer rejected',async()=>assert.equal((await fetch(base+'/pointer?id=0&u=NaN&v=0&down=0',{headers,method:'POST'})).status,400));
await check('physical desktop mode protected',async()=>assert.equal((await fetch(base+'/display-mode?id=0&width=7680&height=4320&fps=60',{headers,method:'POST'})).status,409));
await check('PCM sound stream',async()=>{const controller=new AbortController();const timer=setTimeout(()=>controller.abort(),5000);try{const r=await fetch(base+'/audio',{headers,signal:controller.signal});assert.equal(r.status,200);assert.equal(r.headers.get('X-DeskHalo-Audio'),'s16le;rate=48000;channels=2');const reader=r.body.getReader();const value=await reader.read();assert(value.value.length>0);await reader.cancel();}finally{clearTimeout(timer);controller.abort();}});
console.log(checks.length+' checks passed. No desktop topology changed.');
