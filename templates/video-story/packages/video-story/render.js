import { sealVisualTrack } from '@hypit/hypit/composition';
import { assertTemporalWindowFor } from '@hypit/hypit/temporal';
const style=values=>Object.entries(values).map(([name,value])=>({name,value}));
/** Original QVW composition; video samples the first eight seconds with muted playback. */
export function storyElements(canvas,font,artifact,options){
  if(typeof options.title!=='string'||!options.title.trim()||options.title.length>80||typeof options.subtitle!=='string'||options.subtitle.length>180)throw new Error('标题不能为空（最多80字）；副标题最多180字。');
  if(!/^#[0-9a-f]{6}$/i.test(options.color))throw new Error('主题色必须为六位十六进制颜色。');
  if(!Number.isSafeInteger(options.entranceFrames)||options.entranceFrames<1||options.entranceFrames>60)throw new Error('入场帧数需为1–60的整数。');
  if(canvas.widthPx!==1280||canvas.heightPx!==720)throw new Error('视频故事模板固定为1280×720。');
  const portrait={id:'portrait',parent:'frame',order:3,kind:options.video?'video':'image',artifact,
    style:style({position:'absolute',inset:0,width:'100%',height:'100%','object-fit':'contain'})};
  // Public Visual IR contract: responsive-explainer/src/render.ts:22–30 and hyperframes/src/document.ts:372–407.
  if(options.video)Object.assign(portrait,{muted:true,sampling:{sourceFrameRate:{numerator:30,denominator:1},sourceFrameCount:240,
    segments:[{target:{startFrame:0,endFrameExclusive:240},sourceFrame:{numerator:0,denominator:1},rate:{numerator:1,denominator:1}}]}});
  const text=(id,value,left,top,width,size,color,order)=>({id,parent:'story',order,kind:'text',text:value,fonts:font.faces,
    style:style({position:'absolute',left:`${left}px`,top:`${top}px`,width:`${width}px`,'font-size':`${size}px`,'line-height':1.35,color})});
  return [
    {id:'story',kind:'box',order:0,style:style({position:'absolute',inset:0,width:'1280px',height:'720px','background-color':'#101c29',overflow:'hidden'}),
      animation:{keyframes:[{atFrame:0,style:style({opacity:0})},{atFrame:options.entranceFrames,easing:'ease-out',style:style({opacity:1})}]}},
    {id:'accent',parent:'story',kind:'box',order:1,style:style({position:'absolute',left:'72px',top:'193px',width:'64px',height:'6px','background-color':options.color})},
    {id:'frame',parent:'story',kind:'box',order:2,style:style({position:'absolute',left:'804px',top:'64px',width:'332px',height:'592px','background-color':'#06121e','border-radius':'28px',overflow:'hidden',border:'1px solid #294252'})},
    portrait,
    text('eyebrow','CAMPUS / MOTION STORIES',72,141,638,18,options.color,4),
    text('title',options.title,72,250,640,58,'#f4f7fb',5),
    text('subtitle',options.subtitle,72,429,610,25,'#bbcbd8',6),
    text('footer','一起记录 · 一起创造',72,608,610,18,options.color,7),
  ];
}
export function renderStory(timeline,canvas,window,font,artifact,options){
  assertTemporalWindowFor(window,{subjectId:options.id,space:timeline});
  if(timeline.frameRate.numerator!==30||timeline.frameRate.denominator!==1||window.span.endFrameExclusive-window.span.startFrame!==240)throw new Error('视频故事模板固定为30 fps、8秒。');
  return sealVisualTrack({id:options.id,programSpaceId:timeline.id,visualIr:'hypit.visual-ir@1',presents:[{
    id:options.id,subjectId:options.id,span:window.span,stacking:{order:0,tieBreak:options.id},elements:storyElements(canvas,font,artifact,options)}]});
}
