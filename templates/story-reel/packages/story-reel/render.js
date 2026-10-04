import { sealVisualTrack } from '@hypit/hypit/composition';
import { assertTemporalWindowFor } from '@hypit/hypit/temporal';
// These defaults preserve the framing of projects authored before template 1.1.
export function imageFraming({imageFit = 'cover', imagePositionX = 50, imagePositionY = 50}) {
  if (!['cover','contain'].includes(imageFit)) throw new Error('image-fit 仅支持 cover 或 contain。');
  for (const [name,value] of [['image-position-x',imagePositionX],['image-position-y',imagePositionY]]) {
    if (!Number.isFinite(value) || value < 0 || value > 100) throw new Error(`${name} 需为0–100的有限数值。`);
  }
  return {imageFit,imagePositionX,imagePositionY};
}
const style = values => Object.entries(values).map(([name,value])=>({name,value}));
export function sceneElements(canvas,font,image,options) {
  if(canvas.widthPx!==720||canvas.heightPx!==1280)throw new Error('故事模板画布固定为720×1280。');
  if(typeof options.title!=='string'||!options.title.trim()||options.title.length>60||typeof options.subtitle!=='string'||options.subtitle.length>160)throw new Error('标题需1–60字，副标题最多160字。');
  if(!/^#[0-9a-f]{6}$/i.test(options.color))throw new Error('主题色需六位十六进制颜色。');
  if(!Number.isSafeInteger(options.fontSize)||options.fontSize<32||options.fontSize>76)throw new Error('标题字号需32–76整数。');
  const {imageFit,imagePositionX,imagePositionY} = imageFraming(options);
  let order=2;
  const text=(id,value,top,size,color)=>({id,parent:'scene',order:++order,kind:'text',text:value,fonts:font.faces,style:style({position:'absolute',left:'48px',top:`${top}px`,width:'624px','font-size':`${size}px`,'line-height':1.3,color,'white-space':'pre-wrap'})});
  return [
    {id:'scene',kind:'box',order:0,style:style({position:'absolute',inset:0,width:'720px',height:'1280px','background-color':'#101823',overflow:'hidden'})},
    {id:'photo',parent:'scene',kind:'image',order:1,artifact:image,style:style({position:'absolute',left:'24px',top:'24px',width:'672px',height:'796px','object-fit':imageFit,'object-position':`${imagePositionX}% ${imagePositionY}%`,'border-radius':'28px'})},
    {id:'accent',parent:'scene',kind:'box',order:2,style:style({position:'absolute',left:'48px',top:'858px',width:'72px',height:'6px','background-color':options.color})},
    text('eyebrow','MOMENTS / OUR STORY',892,18,options.color),
    text('title',options.title,940,options.fontSize,'#f3f6fa'),
    text('subtitle',options.subtitle,1130,25,'#b9c8d6')
  ];
}
export function renderScene(timeline,canvas,window,font,image,options) {
  assertTemporalWindowFor(window,{subjectId:options.id,space:timeline});
  return sealVisualTrack({id:options.id,programSpaceId:timeline.id,visualIr:'hypit.visual-ir@1',presents:[{
    id:options.id,subjectId:options.id,span:window.span,stacking:{order:0,tieBreak:options.id},elements:sceneElements(canvas,font,image,options)
  }]});
}
