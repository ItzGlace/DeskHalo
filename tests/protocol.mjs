// Usage: node tests/protocol.mjs HOST PAIRING_CODE
// Read-only checks, apart from intentionally invalid virtual-display requests.
import assert from 'node:assert/strict';
const [host='127.0.0.1',code]=process.argv.slice(2);
if(!/^\d{6}$/.test(code??''))throw Error('Pass the six-digit code shown by DeskHalo Host.');
const base=`http://${host}:47654`;
const headers={'X-DeskHalo-Code':code};
let pass=0;
async function test(name,run){await run();console.log('PASS '+name);pass++;}
await test('Unpaired client denied',async()=>assert.equal((await fetch(base+'/hello')).status,401));
await test('Wrong code denied',async()=>assert.equal((await fetch(base+'/keys',{headers:{'X-DeskHalo-Code':'000000'}})).status,401));
await test('Host identity and monitor enumeration',async()=>{const r=await fetch(base+'/hello',{headers});assert.equal(r.status,200);const j=await r.json();assert.equal(j.app,'DeskHalo');assert(j.displays.length>0);});
await test('Keyboard state range',async()=>{const j=await(await fetch(base+'/keys',{headers})).json();assert(j.keys.every(k=>Number.isInteger(k)&&k>=8&&k<=254));});
await test('Oversized frame rejected',async()=>assert.equal((await fetch(base+'/frame?id=0&width=99999&height=720',{headers})).status,400));
await test('Missing monitor rejected',async()=>assert.equal((await fetch(base+'/frame?id=999&width=1280&height=720',{headers})).status,404));
await test('Invalid driver count rejected',async()=>assert.equal((await fetch(base+'/virtual?count=-1',{method:'POST',headers})).status,400));
for(const [w,h] of [[1280,720],[1920,1080],[2560,1440]])await test(`Actual desktop JPEG ${w}x${h}`,async()=>{const t=performance.now();const r=await fetch(base+`/frame?id=0&width=${w}&height=${h}`,{headers});assert.equal(r.status,200,await (r.ok?Promise.resolve(''):r.text()));assert.equal(r.headers.get('content-type'),'image/jpeg');const b=Buffer.from(await r.arrayBuffer());assert.equal(b.readUInt16BE(0),0xffd8);assert(b.length>5000);console.log(`  ${b.length} bytes, ${Math.round(performance.now()-t)} ms`);});
console.log(`${pass} protocol checks passed.`);
