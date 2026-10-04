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
/** Original QVW title/image composition; strings stay in text nodes, never HTML. */
export function cardElements(canvas, font, image, options) {
  if (!options.title.trim() || options.title.length > 80 || options.subtitle.length > 180) throw new Error('标题不能为空（最多80字）；副标题最多180字。');
  if (!/^#[0-9a-f]{6}$/i.test(options.color)) throw new Error('主题色必须为六位十六进制颜色。');
  if (!Number.isSafeInteger(options.entranceFrames) || options.entranceFrames < 1 || options.entranceFrames > 60) throw new Error('入场帧数需为1–60的整数。');
  if (canvas.widthPx !== 1280 || canvas.heightPx !== 720) throw new Error('标题卡模板固定为1280×720。');
  const {imageFit,imagePositionX,imagePositionY} = imageFraming(options);
  let order = 3;
  const text = (id,value,left,top,width,size,color) => ({id,parent:'card',order:++order,kind:'text',text:value,fonts:font.faces,
    style:style({position:'absolute',left:`${left}px`,top:`${top}px`,width:`${width}px`,'font-size':`${size}px`,'line-height':1.35,color})});
  return [
    {id:'card',kind:'box',order:0,style:style({position:'absolute',inset:0,width:'1280px',height:'720px','background-color':'#111f2c',overflow:'hidden'}),
      animation:{keyframes:[{atFrame:0,style:style({opacity:0})},{atFrame:options.entranceFrames,easing:'ease-out',style:style({opacity:1})}]}},
    {id:'accent',parent:'card',kind:'box',order:1,style:style({position:'absolute',left:'72px',top:'176px',width:'60px',height:'6px','background-color':options.color})},
    {id:'photo',parent:'card',kind:'image',order:2,artifact:image,style:style({position:'absolute',left:'724px',top:'100px',width:'484px',height:'520px','object-fit':imageFit,'object-position':`${imagePositionX}% ${imagePositionY}%`,'border-radius':'24px'})},
    text('eyebrow','CAMPUS / CREATIVE COMMUNITY',72,126,590,18,options.color),
    text('title',options.title,72,222,590,54,'#f4f7fb'),
    text('subtitle',options.subtitle,72,416,565,25,'#bbcbd8'),
    text('footer','一起探索 · 一起创造',72,594,560,18,options.color),
  ];
}
export function renderCard(timeline, canvas, window, font, image, options) {
  assertTemporalWindowFor(window,{subjectId:options.id,space:timeline});
  return sealVisualTrack({id:options.id,programSpaceId:timeline.id,visualIr:'hypit.visual-ir@1',presents:[{
    id:options.id,subjectId:options.id,span:window.span,stacking:{order:0,tieBreak:options.id},elements:cardElements(canvas,font,image,options)
  }]});
}
