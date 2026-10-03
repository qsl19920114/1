#include "ui/StudioTransport.h"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>
namespace qvw::ui {
QString previewProbeScript(const domain::Snapshot &snapshot) {
    if (!snapshot.isLoaded() || snapshot.sourceFingerprint.isEmpty() || snapshot.previewSrcdoc.isEmpty())
        return QStringLiteral("(()=>({ready:false,confirmed:false,reason:'no preview baseline'}))()");
    const auto html = QString::fromUtf8(QJsonDocument(QJsonArray{snapshot.previewSrcdoc}).toJson(QJsonDocument::Compact));
    // Fixed Hypit stage.ts mounts srcdoc before the new document finishes loading.
    // render.ts + runtime-shim.ts leave SCRIPT/STYLE text and static content intact;
    // the shim changes visibility/animation/transform and the composition box size.
    return QStringLiteral(R"JS((()=>{
 const state={ready:false,confirmed:false,reason:'waiting',seconds:0,time:''};
 const fail=reason=>{state.reason=reason;return state;};
 try {
  const stage=document.querySelector('.stage');
  const scaler=stage?.querySelector('.stage-scaler');
  const frame=scaler?.querySelector('iframe');
  const scrubber=stage?.querySelector('.stage-scrubber');
  const artifact=stage?.querySelector('.stage-artifact');
  const back=stage?.querySelector('.stage-return');
  const shown=e=>!!e&&!e.hidden&&getComputedStyle(e).display!=='none'&&getComputedStyle(e).visibility!=='hidden';
  if(!shown(scaler)||(artifact&&shown(artifact))||(back&&!back.hidden))return fail('composition mode is not visible');
  if(!scrubber||scrubber.disabled||!frame)return fail('composition is loading');
  state.seconds=Number(scrubber.value||0);state.time=stage.querySelector('.stage-time')?.textContent||'';
  const expectedHtml=%1[0];
  if(frame.srcdoc!==expectedHtml)return fail('iframe srcdoc differs from compiled snapshot');
  const actual=frame.contentDocument;
  const win=frame.contentWindow;
  if(!actual||!win||actual.readyState!=='complete')return fail('document is still loading');
  let cache=window.__qvwPreviewExpected;
  if(!cache||cache.html!==expectedHtml){
   cache={html:expectedHtml,document:new DOMParser().parseFromString(expectedHtml,'text/html')};
   window.__qvwPreviewExpected=cache;
  }
  const expected=cache.document;
  const composition=actual.querySelector('[data-composition-id]');
  if(!composition||!expected.querySelector('[data-composition-id]'))return fail('compiled composition is absent');
  if(Number(composition.getAttribute('data-width'))!==%2||Number(composition.getAttribute('data-height'))!==%3||Number(composition.getAttribute('data-hypit-frame-count'))!==%4)
   return fail('composition space differs from snapshot');
  const code=document=>Array.from(document.querySelectorAll('script,style')).map(e=>[e.tagName,e.getAttribute('src')||'',e.getAttribute('type')||'',e.textContent]);
  if(JSON.stringify(code(actual))!==JSON.stringify(code(expected)))return fail('document script/style differs from snapshot');
  const nodes=document=>Array.from(document.body.querySelectorAll('*')).filter(e=>e.tagName!=='SCRIPT'&&e.tagName!=='STYLE');
  const expectedNodes=nodes(expected),actualNodes=nodes(actual);
  if(expectedNodes.length!==actualNodes.length)return fail('static document structure differs from snapshot');
  const attributes=(e,generated)=>Array.from(e.attributes).filter(a=>(a.name.startsWith('data-')||['id','class','src','href','poster','type'].includes(a.name))&&!generated.has(a.name)).map(a=>[a.name,a.value]).sort((a,b)=>a[0].localeCompare(b[0]));
  const texts=e=>Array.from(e.childNodes).filter(n=>n.nodeType===3&&n.textContent.trim()).map(n=>n.textContent);
  const stableStyles=['font-size','font-family','font-weight','font-style','line-height','letter-spacing','color','background','background-color','background-image','border-color','border-radius','text-align','white-space','object-fit','fill','stroke','width','height','left','right','top','bottom','position','overflow'];
  for(let index=0;index<expectedNodes.length;index++){
   const e=expectedNodes[index],a=actualNodes[index];
   // Fixed hyperframes/text.ts:1062–1134 owns shrink geometry and its
   // scale marker; :1136–1162 owns physical-line markers only on glyphs
   // inside an authored line-sequence flow. Derive permissions from the
   // inert expected document so runtime DOM cannot grant itself exceptions.
   const shrink=e.matches('[data-hypit-text-flow][data-hypit-text-overflow="shrink"]');
   const lineGlyph=e.hasAttribute('data-hypit-text-unit-grapheme')&&!!e.closest('[data-hypit-text-flow][data-hypit-text-line-sequences]');
   const generated=new Set();
   if(shrink&&!e.hasAttribute('data-hypit-text-shrink-scale'))generated.add('data-hypit-text-shrink-scale');
   if(lineGlyph&&!e.hasAttribute('data-hypit-text-physical-line'))generated.add('data-hypit-text-physical-line');
   if(e.tagName!==a.tagName||JSON.stringify(attributes(e,generated))!==JSON.stringify(attributes(a,generated))||JSON.stringify(texts(e))!==JSON.stringify(texts(a)))
    return fail('static text/material/attributes differ from snapshot');
   let shrinkGeometry=false;
   if(shrink&&a.hasAttribute('data-hypit-text-shrink-scale')){
    const scale=Number(a.getAttribute('data-hypit-text-shrink-scale'));
    const minimum=Number(e.getAttribute('data-hypit-text-minimum-scale'));
    if(!Number.isFinite(scale)||!Number.isFinite(minimum)||minimum<=0||minimum>1||scale<minimum-1e-8||scale>1)
     return fail('text shrink scale is invalid');
    const size=(name,mode)=>mode==='hug'?a.style.getPropertyValue(name)==='max-content':mode==='fixed'&&a.style.getPropertyValue(name).endsWith('%')&&Math.abs(parseFloat(a.style.getPropertyValue(name))*scale-100)<0.001;
    if(a.style.position!=='relative'||a.style.left!==''||a.style.top!==''||!size('width',e.getAttribute('data-hypit-text-inline-size'))||!size('height',e.getAttribute('data-hypit-text-block-size')))
     return fail('text shrink geometry differs from renderer layout');
    shrinkGeometry=true;
   }
   if(lineGlyph&&a.hasAttribute('data-hypit-text-physical-line')){
    const line=a.getAttribute('data-hypit-text-physical-line');
    if(!/^\d+$/.test(line)||!Number.isSafeInteger(Number(line)))return fail('text physical-line marker is invalid');
   }
   const animated=(e.getAttribute('data-hypit-animation-properties')||'').split(',');
   for(const name of stableStyles){
    if(shrinkGeometry&&['position','left','top','width','height'].includes(name))continue;
    const value=e.style.getPropertyValue(name);
    if(value&&!animated.includes(name)&&a.style.getPropertyValue(name)!==value)return fail('static presentation differs from snapshot');
   }
  }
  const images=Array.from(actual.querySelectorAll('img'));
  const videos=Array.from(actual.querySelectorAll('video'));
  const audio=Array.from(actual.querySelectorAll('audio'));
  state.images=images.length;state.videos=videos.length;
  if(!images.every(i=>i.complete&&i.naturalWidth>0))return fail('images are not decoded');
  if(!videos.every(v=>!v.error&&!v.seeking&&v.readyState>=2&&v.videoWidth>0&&v.videoHeight>0))return fail('video frames are not decoded');
  if(!audio.every(a=>!a.error&&!a.seeking&&a.readyState>=2))return fail('audio is not decoded');
  if(actual.fonts&&actual.fonts.status!=='loaded')return fail('fonts are loading');
  if(win.__hypitBrowserProgramError)return fail('composition runtime failed');
  if(typeof win.__hypitSeekFrame!=='function'||!win.__hypitFrameReady||typeof win.__hypitFrameReady.then!=='function')return fail('frame runtime is not ready');
  let pending=win.__qvwPreviewFrameReady;
  if(!pending||pending.promise!==win.__hypitFrameReady){
   pending={promise:win.__hypitFrameReady,done:false,failed:false};win.__qvwPreviewFrameReady=pending;
   const observed=pending;
   // The fixed shim returns false for a seek superseded by Studio's onload
   // seek/mute calls. It still settled; current media readiness was checked
   // above. Only rejection is a runtime failure.
   Promise.resolve(observed.promise).then(()=>{if(win.__qvwPreviewFrameReady===observed)observed.done=true;},()=>{if(win.__qvwPreviewFrameReady===observed)observed.failed=true;});
  }
  if(!pending.done||pending.failed)return fail('frame runtime is still settling');
  state.ready=true;state.confirmed=true;state.reason='document and decoded media match snapshot';return state;
 }catch(_){return fail('preview document cannot be inspected');}
})())JS").arg(html, QString::number(snapshot.space.width), QString::number(snapshot.space.height), QString::number(snapshot.space.frameCount));
}
StudioTransport::StudioTransport(QObject *parent):QObject(parent) {
    setObjectName("studioTransport");
    m_timer.setInterval(250);connect(&m_timer,&QTimer::timeout,this,&StudioTransport::poll);
}
void StudioTransport::attach(QWebEngineView *view) {
    clear();m_view=view;if(!view)return;
    m_loadStarted=connect(view,&QWebEngineView::loadStarted,this,[this]{m_loading=true;m_loadFailed=false;invalidate();});
    m_loadFinished=connect(view,&QWebEngineView::loadFinished,this,[this](bool ok){m_loading=false;m_loadFailed=!ok;invalidate();if(ok)poll();});
    m_viewDestroyed=connect(view,&QObject::destroyed,this,[this]{clear();});
    m_timer.start();setReduced(m_reduced);
}
void StudioTransport::setSpace(const domain::CanvasSpace &space) {
    auto snapshot=m_snapshot;snapshot.space=space;setSnapshot(snapshot);
}
void StudioTransport::setSnapshot(const domain::Snapshot &snapshot) {
    const bool changed=snapshot.revision!=m_snapshot.revision||snapshot.sourceFingerprint!=m_snapshot.sourceFingerprint||snapshot.previewSrcdoc!=m_snapshot.previewSrcdoc||snapshot.sourcePath!=m_snapshot.sourcePath;
    m_snapshot=snapshot;m_space=snapshot.space;
    if(changed)invalidate();
    poll();
}
void StudioTransport::publishVersion(const domain::PreviewVersion &version) {
    if(version.revision==m_previewVersion.revision&&version.sourceFingerprint==m_previewVersion.sourceFingerprint)return;
    m_previewVersion=version;emit previewVersionChanged(version);
}
void StudioTransport::invalidate() {
    ++m_generation;m_pending=false;m_ready=false;publishVersion({});emit positionChanged(false,0,qMax(0,m_space.frameCount-1),{});
}
void StudioTransport::clear() {
    disconnect(m_loadStarted);disconnect(m_loadFinished);disconnect(m_viewDestroyed);
    m_timer.stop();m_view.clear();m_snapshot={};m_space={};m_loading=false;m_loadFailed=false;invalidate();
}
void StudioTransport::poll() {
    if(!m_view||m_pending||m_loading||m_loadFailed||m_space.frameCount<1||!std::isfinite(m_space.frameRate)||m_space.frameRate<=0)return;
    m_pending=true;const auto generation=m_generation;QPointer<StudioTransport> guard(this);
    m_view->page()->runJavaScript(previewProbeScript(m_snapshot),[guard,generation](const QVariant &value){
        if(!guard||generation!=guard->m_generation)return;
        guard->m_pending=false;const auto state=value.toMap();guard->m_ready=state.value("confirmed").toBool();
        const auto seconds=state.value("seconds").toDouble();
        const int frame=std::isfinite(seconds)?qBound(0,int(std::round(seconds*guard->m_space.frameRate)),guard->m_space.frameCount-1):0;
        guard->publishVersion(guard->m_ready?domain::PreviewVersion{guard->m_snapshot.revision,guard->m_snapshot.sourceFingerprint}:domain::PreviewVersion{});
        if(!guard||generation!=guard->m_generation)return;
        emit guard->positionChanged(guard->m_ready,frame,guard->m_space.frameCount-1,state.value("time").toString());
    });
}
void StudioTransport::command(const QString &script) {
    if(!m_view||!m_ready)return;
    // Recheck at execution, closing the polling interval when Studio changes mode.
    const auto guard=QStringLiteral("const c=document.querySelector('.stage-scaler');const s=document.querySelector('.stage-scrubber');if(!c||c.hidden||getComputedStyle(c).display==='none'||!s||s.disabled||!document.querySelector('.stage iframe')?.contentDocument?.querySelector('[data-composition-id]'))return;");
    m_view->page()->runJavaScript("(()=>{"+guard+script+";})()");
}
void StudioTransport::seek(int frame) {
    if(!m_ready||m_space.frameRate<=0)return;
    const auto seconds=QString::number(qBound(0,frame,m_space.frameCount-1)/m_space.frameRate,'g',17);
    command(QString("(()=>{const s=document.querySelector('.stage-scrubber');if(s&&!s.disabled){s.value=%1;s.dispatchEvent(new Event('input',{bubbles:true}));}})()").arg(seconds));
}
void StudioTransport::togglePlayback(){command("document.querySelector('[data-play]')?.click()");}
void StudioTransport::step(int direction){command(direction<0?"document.querySelector('[data-previous]')?.click()":"document.querySelector('[data-next]')?.click()");}
void StudioTransport::setReduced(bool reduced) {
    m_reduced=reduced;if(!m_view)return;
    // Only presentation is changed; Studio's stage/store and failure UI remain.
    const QString css=R"CSS(.topbar,.source-panel,.workspace-panel,.comments-panel,.handle,.timeline-panel,.stage-heading,.stage-bar,.stage-progress{display:none!important}.studio-shell{grid-template-rows:minmax(0,1fr)!important;padding:0!important}.upper-shell{grid-template-columns:minmax(0,1fr)!important}.preview-panel{grid-column:1!important}.stage{height:100%!important;background:#0b1018!important}.failure{z-index:100})CSS";
    const auto quoted=QString::fromUtf8(QJsonDocument(QJsonArray{css}).toJson(QJsonDocument::Compact));
    m_view->page()->runJavaScript(QString("(()=>{let s=document.getElementById('qvw-preview-style');if(!s){s=document.createElement('style');s.id='qvw-preview-style';document.head.append(s)}s.textContent=%1?%2[0]:'';if(%1&&document.querySelector('.stage-scaler')?.hidden)document.querySelector('.stage-return')?.click();window.dispatchEvent(new Event('resize'));})()").arg(reduced?"true":"false",quoted));
}
}
