#include "ui/StudioTransport.h"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>
namespace qvw::ui {
StudioTransport::StudioTransport(QObject *parent):QObject(parent) {
    m_timer.setInterval(250);connect(&m_timer,&QTimer::timeout,this,&StudioTransport::poll);
}
void StudioTransport::attach(QWebEngineView *view) {
    clear();m_view=view;m_timer.start();setReduced(m_reduced);
}
void StudioTransport::setSpace(const domain::CanvasSpace &space) {m_space=space;}
void StudioTransport::clear() {
    ++m_generation;m_timer.stop();m_view.clear();m_ready=false;m_pending=false;
    emit positionChanged(false,0,0,{});
}
void StudioTransport::poll() {
    if(!m_view||m_pending||m_space.frameCount<1||!std::isfinite(m_space.frameRate)||m_space.frameRate<=0)return;
    m_pending=true;const auto generation=m_generation;QPointer<StudioTransport> guard(this);
    // Pinned Studio ui/stage.ts: compositionTime() updates the real scrubber.
    m_view->page()->runJavaScript(R"JS((()=>{try{const s=document.querySelector('.stage-scrubber');const d=document.querySelector('.stage iframe')?.contentDocument;const c=document.querySelector('.stage-scaler');return {ready:!!s&&!s.disabled&&!!c&&!c.hidden&&getComputedStyle(c).display!=='none'&&!!d?.querySelector('[data-composition-id]'),seconds:Number(s?.value||0),time:document.querySelector('.stage-time')?.textContent||''};}catch(_){return {ready:false}}})())JS",
        [guard,generation](const QVariant &value){
            if(!guard||generation!=guard->m_generation)return;
            guard->m_pending=false;const auto state=value.toMap();guard->m_ready=state.value("ready").toBool();
            const auto seconds=state.value("seconds").toDouble();
            const int frame=std::isfinite(seconds)?qBound(0,int(std::round(seconds*guard->m_space.frameRate)),guard->m_space.frameCount-1):0;
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
