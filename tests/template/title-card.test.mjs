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

const activation = await import('../../templates/title-card/packages/title-card/activation.js');
const photoStyle = (change = {}) => Object.fromEntries(
  cardElements({widthPx:1280,heightPx:720}, {faces:[]}, image, {...options, ...change})
    .find(element => element.id === 'photo').style.map(({name,value}) => [name,value]));
test('legacy options retain centered cover framing', () => {
  assert.equal(photoStyle()['object-fit'], 'cover');
  assert.equal(photoStyle()['object-position'], '50% 50%');
});
test('cover and contain honor independent decimal focal positions including bounds', () => {
  for (const imageFit of ['cover','contain']) {
    for (const [imagePositionX,imagePositionY] of [[0,100],[100,0],[12.5,83.25]]) {
      const style = photoStyle({imageFit,imagePositionX,imagePositionY});
      assert.equal(style['object-fit'], imageFit);
      assert.equal(style['object-position'], `${imagePositionX}% ${imagePositionY}%`);
    }
  }
});
test('invalid fit and nonfinite or nonnumeric positions cannot reach CSS', () => {
  for (const imageFit of ['fill','COVER','','cover; display:none',null,0]) {
    assert.throws(() => photoStyle({imageFit}), /image-fit/);
  }
  for (const key of ['imagePositionX','imagePositionY']) {
    for (const value of [-0.1,100.1,NaN,Infinity,-Infinity,'50','0; display:none',null,true,{}]) {
      assert.throws(() => photoStyle({[key]:value}), /image-position/);
    }
  }
});
test('source framing literals use backward defaults and strict decimal parsing', () => {
  assert.equal(typeof activation.imageFramingOptions, 'function');
  const parse = attributes => activation.imageFramingOptions({name:'Card',attributes});
  assert.deepEqual(parse({}), {imageFit:'cover',imagePositionX:50,imagePositionY:50});
  assert.deepEqual(parse({'image-fit':'contain','image-position-x':'0','image-position-y':'100'}),
    {imageFit:'contain',imagePositionX:0,imagePositionY:100});
  assert.equal(parse({'image-position-x':'12.5'}).imagePositionX,12.5);
  for (const key of ['image-position-x','image-position-y']) {
    for (const value of ['',' ','NaN','Infinity','-1','100.1','50%','0x10','1e1','1; display:none',{kind:'reference',path:'x'},null]) {
      assert.throws(() => parse({[key]:value}), /image-position/);
    }
  }
  for (const value of ['fill','cover; display:none',{kind:'reference',path:'x'},null]) {
    assert.throws(() => parse({'image-fit':value}), /image-fit/);
  }
});
test('fresh sources and package companions expose actual writable framing controls', () => {
  const pkg = activation.hypitPackage;
  const companion = pkg.hostFacets.find(facet => facet.implementation.tracks)?.implementation.tracks[0];
  const fit = companion.inspector.find(field => field.binding === 'image-fit');
  assert.equal(fit?.control,'select');
  assert.deepEqual(fit.options,[{value:'cover',label:'铺满裁切'},{value:'contain',label:'完整显示'}]);
  for (const binding of ['image-fit','image-position-x','image-position-y']) {
    assert.equal(companion.bindings.find(field => field.name === binding)?.writable,true);
  }
  for (const binding of ['image-position-x','image-position-y']) {
    const field = companion.inspector.find(field => field.binding === binding);
    assert.equal(field?.control,'number');
    assert.equal(field.unit,'%');
    assert.deepEqual(field.number,{minimum:0,maximum:100,step:1});
  }
  const source = readFileSync(resolve(repo,'templates/title-card/main.svml'),'utf8');
  const cards = source.match(/<card:Card\b[^>]*>/g);
  for (const card of cards) {
    assert.match(card,/image-fit="cover"/);
    assert.match(card,/image-position-x="50"/);
    assert.match(card,/image-position-y="50"/);
  }
  const vocabulary = pkg.hostFacets.find(facet => facet.implementation.vocabulary).implementation.vocabulary;
  for (const name of ['image-fit','image-position-x','image-position-y']) {
    assert.equal(vocabulary.attributes.find(attribute => attribute.name === name)?.required,false);
  }
  assert.equal(pkg.modules[0].manifest.version,'1');
  assert.equal(JSON.parse(readFileSync(resolve(repo,'templates/title-card/template.json'))).version,'1.1.0');
});

test('generated framing styles pass the pinned Hypit Visual IR validator', async () => {
  const {assertVisualStyleV1} = await import(pathToFileURL(resolve(hypit,'packages/visual-ir/src/style.ts')));
  for (const imageFit of ['cover','contain']) {
    const style = photoStyle({imageFit,imagePositionX:12.5,imagePositionY:100});
    for (const [name,value] of Object.entries(style)) assertVisualStyleV1(name,value,'photo');
  }
});
