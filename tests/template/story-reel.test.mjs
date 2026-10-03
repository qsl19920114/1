import test from 'node:test';
import assert from 'node:assert/strict';
import {createRequire} from 'node:module';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const repo=resolve(import.meta.dirname,'../..');
const lock=JSON.parse(readFileSync(resolve(repo,'config/version-lock.json')));
const hypit=resolve(repo,lock.hypit.distributionPath);
createRequire(resolve(hypit,'package.json'))('tsx/esm/api').register();
const {installDistributionPackageResolution}=await import(pathToFileURL(resolve(hypit,'packages/package-loader-node/src/distribution-resolution.ts')));
installDistributionPackageResolution([hypit]);
const {sceneElements}=await import('../../templates/story-reel/packages/story-reel/render.js');
const canvas={widthPx:720,heightPx:1280},font={faces:[]},image={kind:'reference',uri:'x'};
const options={id:'scene-1',title:'校园创作社',subtitle:'欢迎加入',color:'#35bca8',fontSize:54};
test('portrait scene fits actual canvas and leaves all text as text nodes',()=>{
 const elements=sceneElements(canvas,font,image,options);
 assert.equal(elements[0].style.find(x=>x.name==='width').value,'720px');
 assert.equal(elements.find(x=>x.id==='title').text,options.title);
 assert.equal(elements.find(x=>x.id==='photo').artifact,image);
 assert.equal(elements.find(x=>x.id==='title').style.find(x=>x.name==='font-size').value,'54px');
 assert.equal(new Set(elements.map(x=>x.order)).size,elements.length,'each element order must be unique for Hypit VisualTrack');
});
test('invalid typography and color do not compile',()=>{
 assert.throws(()=>sceneElements(canvas,font,image,{...options,fontSize:200}));
 assert.throws(()=>sceneElements(canvas,font,image,{...options,color:'red'}));
 assert.throws(()=>sceneElements(canvas,font,image,{...options,title:''}));
});
