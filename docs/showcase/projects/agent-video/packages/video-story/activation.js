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
import { renderStory } from './render.js';
const module = {name:'@qvw/video-story',version:'1'};
const optionsType = {module,name:'Options'};
const producer = {module,name:'render'};
const inputs = [{name:'options',type:optionsType},{name:'timeline',type:timelineTypes.track},{name:'canvas',type:spatialTypes.canvas},
  {name:'window',type:temporalTypes.window},{name:'font',type:mediaTypes.fontStack},{name:'video',type:artifactTypes.blob}];
const manifest = {format:'hypit.module@1',...module,dependencies:[compositionTypes.visualTrack,artifactTypes.blob,mediaTypes.fontStack,timelineTypes.track,spatialTypes.canvas,temporalTypes.window].map(type=>({module:type.module})),
  types:[{name:'Options'}],capabilities:[],producers:[{name:'render',inputs,outputs:[{name:'track',type:compositionTypes.visualTrack}],needs:[]}]};
const inline = record => {if(record?.value.kind!=='inline')throw new Error('Video-story expects inline inputs.');return record.value.value;};
const value = data => ({kind:'inline',value:canonicalize(data)});
const component = {producers:[{producer,handler:({inputs})=>({outputs:{track:value(renderStory(inline(inputs.timeline),inline(inputs.canvas),inline(inputs.window),inline(inputs.font),inputs.video.value,inline(inputs.options)))},needs:{}})}]};
const decode = async ({element,resolveReference,resolveAsset}) => {
  assertAttributes(element,['id','timeline','canvas','font','title','subtitle','color','video','entrance-frames',...temporalWindowAttributeNames]);
  assertEmptyElement(element);
  const id=textAttribute(element,'id'),context=resolveTemporalContext({element,resolveReference});
  const window=createTemporalWindowProjection({id:`${id}.window`,subjectId:id,element,...context,resolveReference});
  const reference=(name,type)=>{const raw=element.attributes[name];if(typeof raw!=='object'||raw.kind!=='reference')throw new Error(`${name} must be a reference.`);
    const found=resolveReference(raw.path);if(!found||!sameType(found.type,type))throw new Error(`${name} has the wrong Type.`);return found.ref;};
  const videoPath=textAttribute(element,'video');
  if (!/^\.\/(?:assets\/)[a-zA-Z0-9._/-]+\.(?:mp4|png|jpe?g)$/i.test(videoPath) || videoPath.split('/').includes('..')) throw new Error('视频 slot 仅接受工程 assets 内的 MP4/PNG/JPEG 相对路径。');
  const resolved=await resolveAsset({from:videoPath,mediaType:/\.mp4$/i.test(videoPath)?'video/mp4':/\.png$/i.test(videoPath)?'image/png':'image/jpeg',range:element.range});
  const options={id,title:textAttribute(element,'title'),subtitle:textAttribute(element,'subtitle'),color:textAttribute(element,'color'),entranceFrames:Number(element.attributes['entrance-frames']??'18'),video:/\.mp4$/i.test(videoPath)};
  const records=[...window.records,{id:`${id}.options`,type:optionsType,value:value(options),range:element.range},{id:`${id}.video`,type:artifactTypes.blob,value:resolved.artifact,range:element.range}];
  const bindings={options:{kind:'record',id:`${id}.options`},timeline:context.timeline.ref,canvas:reference('canvas',spatialTypes.canvas),font:reference('font',mediaTypes.fontStack),window:window.ref,video:{kind:'record',id:`${id}.video`}};
  const fragment=sealGraphFragment({inputs,operations:[{id:'render',producer,inputs:Object.fromEntries(inputs.map(x=>[x.name,{kind:'fragment-input',name:x.name}])),result:{kind:'output',name:'track'}}],exports:[{name:'track',type:compositionTypes.visualTrack,root:{kind:'fragment-operation',operation:'render'}}]});
  return {records,fragments:[...window.fragments,fragment],components:[...window.components,{id,fragment:fragment.id,inputs:bindings,outputs:{track:`${id}.track`},range:element.range}],exports:[`${id}.track`]};
};
const declaration={name:'story',tag:'Story',mode:'structured',outputs:[compositionTypes.visualTrack,timelineTypes.track,temporalTypes.window,temporalTypes.windowSpec,temporalTypes.instant,temporalTypes.instantSpec,artifactTypes.blob,optionsType],
  vocabulary:{summary:'Original campus title and local video story.',attributes:[...temporalContextAttributeVocabulary,...temporalWindowAttributeVocabulary,
    ...['id','canvas','font'].map(name=>({name,kind:'expression',required:true,summary:name})),
    ...['title','subtitle','color','video','entrance-frames'].map(name=>({name,kind:'literal',required:true,summary:name}))],
    ports:[{name:'track',type:compositionTypes.visualTrack,summary:'One title/video visual track.'}]}};
const companion={id:'story',role:'track',output:{type:compositionTypes.visualTrack,surface:'story',modules:[module]},family:'media',label:'视频故事',icon:'component',lane:{heightPx:76},
  bindings:['title','subtitle','color','video','entrance-frames'].map(name=>({name,writable:true})),
  inspector:[['title','标题','text'],['subtitle','副标题','text'],['color','主题色','color'],['video','视频路径','text'],['entrance-frames','入场帧数','number']].map(([binding,label,control])=>({binding,label,control,domain:'how',section:{id:'story',label:'视频故事'}}))};
export const hypitPackage={format:'hypit.node-package@1',modules:[{manifest}],components:[component],hostFacets:[createMarkupSurfaceHostFacet({module,declaration,handler:decode}),createStudioTrackCompanionHostFacet([companion])]};
export default hypitPackage;
