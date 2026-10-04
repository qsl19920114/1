// Contracts verified against Hypit 0.2.10; see docs/TEMPLATE_CONTRACT.md.
import { assertAttributes, assertEmptyElement, canonicalize, createMarkupSurfaceHostFacet, sameType, sealGraphFragment, textAttribute } from '@hypit/hypit/author-kit';
import { compositionTypes } from '@hypit/hypit/composition';
import { artifactTypes } from '@hypit/hypit/artifact';
import { mediaTypes } from '@hypit/hypit/media';
import { timelineTypes } from '@hypit/hypit/timeline';
import { spatialTypes } from '@hypit/hypit/spatial';
import { temporalTypes } from '@hypit/hypit/temporal';
import { createTemporalWindowProjection, resolveTemporalContext, temporalWindowAttributeNames, temporalWindowAttributeVocabulary, temporalContextAttributeVocabulary } from '@hypit/hypit/temporal-markup';
import { createStudioTrackCompanionHostFacet } from '@hypit/hypit/studio-adapter';
import { renderScene, imageFraming } from './render.js';
const module = {name:'@qvw/story-reel',version:'1'};
const optionsType = {module,name:'Options'};
const producer = {module,name:'render'};
const inputs = [{name:'options',type:optionsType},{name:'timeline',type:timelineTypes.track},{name:'canvas',type:spatialTypes.canvas},
  {name:'window',type:temporalTypes.window},{name:'font',type:mediaTypes.fontStack},{name:'image',type:artifactTypes.blob}];
const manifest = {format:'hypit.module@1',...module,dependencies:[compositionTypes.visualTrack,artifactTypes.blob,mediaTypes.fontStack,timelineTypes.track,spatialTypes.canvas,temporalTypes.window].map(type=>({module:type.module})),
  types:[{name:'Options'}],capabilities:[],producers:[{name:'render',inputs,outputs:[{name:'track',type:compositionTypes.visualTrack}],needs:[]}]};
const inline = record => {if(record?.value.kind!=='inline')throw new Error('Story-reel expects inline inputs.');return record.value.value;};
const value = data => ({kind:'inline',value:canonicalize(data)});
const component = {producers:[{producer,handler:({inputs})=>({outputs:{track:value(renderScene(inline(inputs.timeline),inline(inputs.canvas),inline(inputs.window),inline(inputs.font),inputs.image.value,inline(inputs.options)))},needs:{}})}]};
const framingAttributes = ['image-fit','image-position-x','image-position-y'];
export function imageFramingOptions(element) {
  const position = name => {
    const literal = textAttribute(element,name,'50');
    if (!/^(?:\d+(?:\.\d+)?|\.\d+)$/.test(literal)) throw new Error(`${name} 需为0–100的十进制数值。`);
    return Number(literal);
  };
  return imageFraming({imageFit:textAttribute(element,'image-fit','cover'),
    imagePositionX:position('image-position-x'),imagePositionY:position('image-position-y')});
}
const decode = async ({element,resolveReference,resolveAsset}) => {
  assertAttributes(element,['id','timeline','canvas','font','title','subtitle','color','image','font-size',...framingAttributes,...temporalWindowAttributeNames]);
  assertEmptyElement(element);
  const framing = imageFramingOptions(element);
  const id=textAttribute(element,'id'),context=resolveTemporalContext({element,resolveReference});
  const window=createTemporalWindowProjection({id:`${id}.window`,subjectId:id,element,...context,resolveReference});
  const reference=(name,type)=>{const raw=element.attributes[name];if(typeof raw!=='object'||raw.kind!=='reference')throw new Error(`${name} must be a reference.`);
    const found=resolveReference(raw.path);if(!found||!sameType(found.type,type))throw new Error(`${name} has the wrong Type.`);return found.ref;};
  const imagePath=textAttribute(element,'image');
  if (!/^\.\/(?:assets\/)[a-zA-Z0-9._/-]+\.(?:png|jpe?g)$/i.test(imagePath) || imagePath.split('/').includes('..')) throw new Error('图片 slot 仅接受工程 assets 内的 PNG/JPEG 相对路径。');
  const resolved=await resolveAsset({from:imagePath,mediaType:/\.png$/i.test(imagePath)?'image/png':'image/jpeg',range:element.range});
  const options={id,...framing,title:textAttribute(element,'title'),subtitle:textAttribute(element,'subtitle'),color:textAttribute(element,'color'),fontSize:Number(element.attributes['font-size']??'54')};
  const records=[...window.records,{id:`${id}.options`,type:optionsType,value:value(options),range:element.range},{id:`${id}.image`,type:artifactTypes.blob,value:resolved.artifact,range:element.range}];
  const bindings={options:{kind:'record',id:`${id}.options`},timeline:context.timeline.ref,canvas:reference('canvas',spatialTypes.canvas),font:reference('font',mediaTypes.fontStack),window:window.ref,image:{kind:'record',id:`${id}.image`}};
  const fragment=sealGraphFragment({inputs,operations:[{id:'render',producer,inputs:Object.fromEntries(inputs.map(x=>[x.name,{kind:'fragment-input',name:x.name}])),result:{kind:'output',name:'track'}}],exports:[{name:'track',type:compositionTypes.visualTrack,root:{kind:'fragment-operation',operation:'render'}}]});
  return {records,fragments:[...window.fragments,fragment],components:[...window.components,{id,fragment:fragment.id,inputs:bindings,outputs:{track:`${id}.track`},range:element.range}],exports:[`${id}.track`]};
};
const declaration={name:'card',tag:'Card',mode:'structured',outputs:[compositionTypes.visualTrack,timelineTypes.track,temporalTypes.window,temporalTypes.windowSpec,temporalTypes.instant,temporalTypes.instantSpec,artifactTypes.blob,optionsType],
  vocabulary:{summary:'Original portrait story scene with local image.',attributes:[...temporalContextAttributeVocabulary,...temporalWindowAttributeVocabulary,
    ...['id','canvas','font'].map(name=>({name,kind:'expression',required:true,summary:name})),
    ...['title','subtitle','color','image','font-size'].map(name=>({name,kind:'literal',required:true,summary:name})),
    ...framingAttributes.map(name=>({name,kind:'literal',required:false,summary:name}))],
    ports:[{name:'track',type:compositionTypes.visualTrack,summary:'One title/image visual track.'}]}};
const companion={id:'card',role:'track',output:{type:compositionTypes.visualTrack,surface:'card',modules:[module]},family:'media',label:'故事场景',icon:'component',lane:{heightPx:76},
  bindings:['title','subtitle','color','image','font-size',...framingAttributes].map(name=>({name,writable:true})),
  inspector:[['title','标题','text'],['subtitle','副标题','text'],['color','主题色','color'],['image','图片路径','text'],['font-size','标题字号','number']].map(([binding,label,control])=>({binding,label,control,domain:'how',section:{id:'card',label:'场景'}})).concat([
    {binding:'image-fit',label:'图片适配',control:'select',domain:'how',section:{id:'image',label:'图片构图'},
      options:[{value:'cover',label:'铺满裁切'},{value:'contain',label:'完整显示'}]},
    ...[['image-position-x','水平焦点'],['image-position-y','垂直焦点']].map(([binding,label])=>({binding,label,control:'number',domain:'where',
      section:{id:'image',label:'图片构图'},unit:'%',number:{minimum:0,maximum:100,step:1}}))
  ])};
export const hypitPackage={format:'hypit.node-package@1',modules:[{manifest}],components:[component],hostFacets:[createMarkupSurfaceHostFacet({module,declaration,handler:decode}),createStudioTrackCompanionHostFacet([companion])]};
export default hypitPackage;
