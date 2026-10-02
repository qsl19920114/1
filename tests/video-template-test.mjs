import { strict as assert } from 'node:assert';
import { existsSync,readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const repo=resolve(import.meta.dirname,'..');
const lock=JSON.parse(readFileSync(resolve(repo,'config/version-lock.json')));
const hypit=resolve(repo,lock.hypit.distributionPath);
const require=createRequire(resolve(hypit,'package.json'));
require('tsx/esm/api').register();
const {installDistributionPackageResolution}=await import(pathToFileURL(resolve(hypit,'packages/package-loader-node/src/distribution-resolution.ts')));
installDistributionPackageResolution([hypit]);
import { test } from 'node:test';
const path=new URL('../templates/video-story/packages/video-story/render.js',import.meta.url);
test('video-story template exists',()=>assert.ok(existsSync(path),'new original video-story template required'));
if(existsSync(path)){
 const {storyElements}=await import(path);
 const canvas={widthPx:1280,heightPx:720},font={faces:[]},blob={kind:'blob',digest:'a'.repeat(64),mediaType:'video/mp4'},options={title:'校园故事',subtitle:'今天的故事',color:'#35bca8',entranceFrames:18,video:true};
 test('MP4 uses exactly the first 240 frames and is muted',()=>{const elements=storyElements(canvas,font,blob,options);const video=elements.find(x=>x.kind==='video');assert.ok(video);assert.equal(video.muted,true);assert.deepEqual(video.sampling,{sourceFrameRate:{numerator:30,denominator:1},sourceFrameCount:240,segments:[{target:{startFrame:0,endFrameExclusive:240},sourceFrame:{numerator:0,denominator:1},rate:{numerator:1,denominator:1}}]});assert.equal(video.style.find(x=>x.name==='object-fit').value,'contain');});
 test('default image keeps the same composition',()=>{const e=storyElements(canvas,font,{...blob,mediaType:'image/png'},{...options,video:false});assert.equal(e.find(x=>x.id==='portrait').kind,'image');assert.equal(e.filter(x=>x.kind==='text').length,4);});
 test('rejects invalid native fields',()=>{for(const bad of [{title:''},{color:'red'},{entranceFrames:0},{entranceFrames:61},{subtitle:'x'.repeat(181)}])assert.throws(()=>storyElements(canvas,font,blob,{...options,...bad}));assert.throws(()=>storyElements({widthPx:1920,heightPx:1080},font,blob,options));});
 test('strings stay in text nodes',()=>{const title='<script>alert(1)</script>';const e=storyElements(canvas,font,blob,{...options,title});assert.equal(e.find(x=>x.id==='title').text,title);assert.ok(!JSON.stringify(e).includes('html'));});
}
