import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const repo = resolve(import.meta.dirname, '../..');
const lock = JSON.parse(readFileSync(resolve(repo, 'config/version-lock.json')));
const hypit = resolve(repo, lock.hypit.distributionPath);
const require = createRequire(resolve(hypit, 'package.json'));
require('tsx/esm/api').register();
const { installDistributionPackageResolution } = await import(pathToFileURL(resolve(hypit, 'packages/package-loader-node/src/distribution-resolution.ts')));
installDistributionPackageResolution([hypit]);
const { cardElements } = await import('../../templates/title-card/packages/title-card/render.js');
const options = {title:'社团 <hello>',subtitle:'一起创作',color:'#35bca8',entranceFrames:18};
const image = {kind:'blob',mediaType:'image/png',digest:'sample'};
test('authored text, color and exact image artifact reach visual elements', () => {
  const elements = cardElements({widthPx:1280,heightPx:720},{faces:[]},image,options);
  assert.equal(elements.find(x=>x.id==='title').text, options.title);
  assert.equal(elements.find(x=>x.id==='subtitle').text, options.subtitle);
  assert.deepEqual(elements.find(x=>x.id==='photo').artifact, image);
  assert.equal(elements.find(x=>x.id==='accent').style.find(x=>x.name==='background-color').value, options.color);
  assert.equal(new Set(elements.map(x=>x.order)).size,elements.length);
  assert.equal(elements[0].animation.keyframes.at(-1).atFrame,18);
});
test('invalid colors and animation duration fail instead of injecting CSS', () => {
  for (const change of [{color:'red; display:none'}, {entranceFrames:0}, {entranceFrames:1.5}, {title:''}]) {
    assert.throws(()=>cardElements({widthPx:1280,heightPx:720},{faces:[]},image,{...options,...change}));
  }
});
