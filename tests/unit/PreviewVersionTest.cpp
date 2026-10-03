#include "ui/StudioTransport.h"
#include "backend/hypit/SnapshotMapper.h"
#include <QApplication>
#include <QBuffer>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFont>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <memory>

using namespace qvw;
namespace {
QString quoted(const QString &text) {
    return QString::fromUtf8(QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact)) + "[0]";
}
QString png(const QColor &color) {
    QImage image(16, 24, QImage::Format_RGB32); image.fill(color);
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer, "PNG");
    return "data:image/png;base64," + QString::fromLatin1(bytes.toBase64());
}
QString picture(const QString &title = "Old title", const QString &image = {}, int font = 18, const QString &extra = {}, int script = 1) {
    return QStringLiteral("<!doctype html><html><head><style>html,body{margin:0;} [data-composition-id]{background:#112233;}</style></head><body>"
        "<div id='composition' data-composition-id='movie' data-width='64' data-height='96' data-fps='30' data-hypit-frame-count='450'>"
        "<div id='title' data-hypit-element-id='title' style='font-size:%1px;color:#ffffff;'>%2</div>"
        "<img id='photo' data-hypit-element-id='photo' src='%3'>%4</div>"
        "<script>window.testVersion=%5;window.__hypitSeekFrame=function(){};window.__hypitFrameReady=Promise.resolve(true);</script></body></html>")
        .arg(font).arg(title, image.isEmpty() ? png(Qt::red) : image, extra).arg(script);
}
QString textLayoutPicture(bool shrink, bool lines) {
    // Execute the fixed external renderer, never a copied approximation of its
    // layout. The fixed+hug shrink structure follows hyperframes/test/document.test.ts:117–219.
    QFile source(QFINDTESTDATA("../../../hypit/packages/hyperframes/src/text.ts"));
    if (!source.open(QIODevice::ReadOnly)) return {};
    const auto text = QString::fromUtf8(source.readAll());
    const QString marker = "export const terminalTextLayoutScript = String.raw`";
    const auto markerAt = text.indexOf(marker);
    if (markerAt < 0) return {};
    const auto start = markerAt + marker.size(), end = text.indexOf("\n`;", start);
    if (end < 0) return {};
    const auto runtime = text.mid(start, end - start);
    const auto glyphs = [](const QString &value, int offset) {
        QString result;
        for (qsizetype index = 0; index < value.size(); ++index)
            result += QString("<span data-hypit-text-unit-grapheme='%1'>%2</span>").arg(offset + index).arg(QString(value[index]).toHtmlEscaped());
        return result;
    };
    const QString firstLine = "FOOTBALLERS";
    const auto content = glyphs(firstLine, 0) + (lines ? "<br>" + glyphs("END", firstLine.size()) : QString());
    const auto lineData = lines ? QString(" data-hypit-text-line-sequences='[{\"range\":{\"start\":0,\"endExclusive\":2}}]'") : QString();
    const auto flow = QStringLiteral("<div id='text-frame' data-hypit-element-id='flow' data-hypit-text-clock style='position:absolute;left:0px;top:0px;width:64px;height:60px;display:flex;align-items:center;justify-content:center;'>"
        "<div id='flow' data-hypit-text-flow data-hypit-text-overflow='%1' data-hypit-text-inline-size='fixed' data-hypit-text-block-size='hug' "
        "data-hypit-text-inline-align='center' data-hypit-text-block-align='center' data-hypit-text-writing-mode='horizontal-tb' data-hypit-text-minimum-scale='0.4'%2 "
        "style='position:relative;width:100%;height:max-content;flex-shrink:0;font-family:monospace;font-size:18px;line-height:1;color:#ffffff;white-space:pre;'>%3</div></div>")
        .arg(shrink ? "shrink" : "visible", lineData, content);
    return picture("Old title", {}, 18, flow).replace("</body>", "<script>" + runtime + "</script><script>window.__hypitFrameReady=svmlTextLayoutReady;</script></body>");
}
domain::Snapshot snapshot(const QString &html, int revision = 7) {
    domain::Snapshot s; s.revision = revision; s.sourceFingerprint = QByteArray("source-") + QByteArray::number(revision);
    s.space.width = 64; s.space.height = 96; s.space.frameRate = 30; s.space.frameCount = 450; s.space.durationSec = 15;
    s.previewSrcdoc = html; return s;
}
QVariant js(QWebEngineView &view, const QString &script) {
    struct State { QVariant value; bool done = false; }; auto state = std::make_shared<State>();
    QEventLoop loop; QPointer<QEventLoop> guard(&loop); QTimer timeout; timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    view.page()->runJavaScript(script, [state, guard](const QVariant &value) { state->value = value; state->done = true; if (guard) guard->quit(); });
    timeout.start(4000); if (!state->done) loop.exec(); return state->value;
}
bool waitDocument(QWebEngineView &view) {
    QElapsedTimer deadline; deadline.start();
    while (deadline.elapsed() < 5000) {
        if (js(view, "(()=>{const d=document.querySelector('.stage iframe')?.contentDocument;return d?.readyState==='complete'&&!!d.querySelector('[data-composition-id]')&&Array.from(d.querySelectorAll('img')).every(i=>i.complete&&i.naturalWidth>0)})()").toBool()) return true;
        QTest::qWait(20);
    }
    return false;
}
bool mount(QWebEngineView &view, const QString &html) {
    view.resize(640, 720); view.show(); QSignalSpy loaded(&view, &QWebEngineView::loadFinished);
    view.setHtml(QStringLiteral("<!doctype html><html><head><style>.stage-scaler{width:64px;height:96px;}iframe{width:64px;height:96px;border:0;}</style></head><body>"
        "<section class='stage'><div class='stage-scaler'><iframe sandbox='allow-scripts allow-same-origin'></iframe></div>"
        "<div class='stage-artifact' hidden></div><button class='stage-return' hidden></button>"
        "<input class='stage-scrubber' type='range' min='0' max='15' value='0' disabled><span class='stage-time'>0:00 / 0:15</span></section>"
        "<script>document.querySelector('iframe').addEventListener('load',()=>{document.querySelector('.stage-scrubber').disabled=false;});</script></body></html>"));
    if (loaded.isEmpty() && !loaded.wait(6000)) return false;
    if (!loaded.last().first().toBool()) return false;
    js(view, "(()=>{document.querySelector('iframe').srcdoc=" + quoted(html) + ";return true})()"); return waitDocument(view);
}
QVariantMap probe(QWebEngineView &view, const domain::Snapshot &s) { return js(view, ui::previewProbeScript(s)).toMap(); }
}
class PreviewVersionTest : public QObject {
    Q_OBJECT
private slots:
    void mapperCarriesOnlyHyperframesPreviewWithoutChangingSourceIdentity() {
        QJsonObject root{{"revision", 7}, {"source", QJsonObject{{"path", "main.svml"}, {"files", QJsonArray{QJsonObject{{"path", "main.svml"}, {"text", "authored"}}}}}}, {"space", QJsonObject{}}, {"tracks", QJsonArray{}}};
        root["preview"] = QJsonObject{{"kind", "hyperframes"}, {"srcdoc", picture()}};
        const auto first = backend::hypit::mapSessionPayload(QJsonDocument(root).toJson()); QVERIFY(first.ok()); QCOMPARE(first.snapshot.previewSrcdoc, picture());
        root["preview"] = QJsonObject{{"kind", "hyperframes"}, {"srcdoc", picture("New title")}};
        const auto second = backend::hypit::mapSessionPayload(QJsonDocument(root).toJson()); QVERIFY(second.ok()); QCOMPARE(second.snapshot.sourceFingerprint, first.snapshot.sourceFingerprint);
        root["preview"] = QJsonObject{{"kind", "artifact"}, {"srcdoc", picture()}};
        QVERIFY(backend::hypit::mapSessionPayload(QJsonDocument(root).toJson()).snapshot.previewSrcdoc.isEmpty());
        root["preview"] = QJsonObject{{"kind", "hyperframes"}, {"srcdoc", 42}};
        QVERIFY(backend::hypit::mapSessionPayload(QJsonDocument(root).toJson()).snapshot.previewSrcdoc.isEmpty());
    }
    void loadedCompositionConfirmsExactVersionAndFingerprint() {
        QWebEngineView view; const auto s = snapshot(picture()); QVERIFY(mount(view, s.previewSrcdoc));
        ui::StudioTransport transport; transport.attach(&view); QSignalSpy versions(&transport, &ui::StudioTransport::previewVersionChanged); transport.setSnapshot(s);
        QTRY_VERIFY_WITH_TIMEOUT(transport.previewVersion().isConfirmed(), 1600);
        QCOMPARE(transport.previewVersion().revision, 7); QCOMPARE(transport.previewVersion().sourceFingerprint, s.sourceFingerprint); QVERIFY(transport.ready());
        QVERIFY(!versions.isEmpty()); const auto emitted = versions.last().first().value<domain::PreviewVersion>(); QCOMPARE(emitted.revision, 7);
    }
    void fixedTextRuntimeLayoutConfirmsVisibleVersion_data() {
        QTest::addColumn<bool>("shrink"); QTest::addColumn<bool>("lines");
        QTest::newRow("shrink") << true << false;
        QTest::newRow("line-sequences") << false << true;
        QTest::newRow("shrink-with-line-sequences") << true << true;
    }
    void fixedTextRuntimeLayoutConfirmsVisibleVersion() {
        QFETCH(bool, shrink); QFETCH(bool, lines);
        const auto s = snapshot(textLayoutPicture(shrink, lines)); QVERIFY(!s.previewSrcdoc.isEmpty());
        QWebEngineView view; QVERIFY(mount(view, s.previewSrcdoc));
        if (shrink) {
            QTRY_VERIFY_WITH_TIMEOUT(js(view, "document.querySelector('iframe').contentDocument.querySelector('#flow').hasAttribute('data-hypit-text-shrink-scale')").toBool(), 1600);
            const auto scale = js(view, "Number(document.querySelector('iframe').contentDocument.querySelector('#flow').getAttribute('data-hypit-text-shrink-scale'))").toDouble();
            QVERIFY(scale >= 0.4 && scale < 1);
            QCOMPARE(js(view, "document.querySelector('iframe').contentDocument.querySelector('#flow').style.height").toString(), QString("max-content"));
        }
        if (lines) {
            QTRY_VERIFY_WITH_TIMEOUT(js(view, "document.querySelector('iframe').contentDocument.querySelector('#flow [data-hypit-text-unit-grapheme]').hasAttribute('data-hypit-text-physical-line')").toBool(), 1600);
            QCOMPARE(js(view, "Array.from(document.querySelector('iframe').contentDocument.querySelectorAll('#flow [data-hypit-text-unit-grapheme]')).at(-1).getAttribute('data-hypit-text-physical-line')").toString(), QString("1"));
        }
        ui::StudioTransport transport; transport.attach(&view); transport.setSnapshot(s);
        QTRY_VERIFY_WITH_TIMEOUT(transport.previewVersion().isConfirmed(), 1600); QVERIFY(transport.ready());
        QCOMPARE(transport.previewVersion().revision, s.revision); QCOMPARE(transport.previewVersion().sourceFingerprint, s.sourceFingerprint);
    }
    void fixedTextRuntimeStillRejectsStaticAndUnownedChanges_data() {
        QTest::addColumn<QString>("mutation");
        QTest::newRow("static-text") << "d.querySelector('#flow span').textContent='Changed'";
        QTest::newRow("static-image") << "d.querySelector('#photo').src=" + quoted(png(Qt::blue));
        QTest::newRow("static-font") << "d.querySelector('#flow').style.fontSize='22px'";
        QTest::newRow("authored-flow-data") << "d.querySelector('#flow').setAttribute('data-hypit-text-inline-size','hug')";
        QTest::newRow("unknown-flow-data") << "d.querySelector('#flow').setAttribute('data-hypit-custom','changed')";
        QTest::newRow("scale-outside-shrink-flow") << "d.querySelector('#photo').setAttribute('data-hypit-text-shrink-scale','0.5')";
        QTest::newRow("physical-line-outside-glyph") << "d.querySelector('#flow').setAttribute('data-hypit-text-physical-line','0')";
        QTest::newRow("parent-position") << "d.querySelector('#text-frame').style.position='relative'";
        QTest::newRow("parent-left") << "d.querySelector('#text-frame').style.left='10px'";
        QTest::newRow("parent-top") << "d.querySelector('#text-frame').style.top='10px'";
        QTest::newRow("parent-width") << "d.querySelector('#text-frame').style.width='50px'";
        QTest::newRow("parent-height") << "d.querySelector('#text-frame').style.height='50px'";
        QTest::newRow("invalid-runtime-scale") << "d.querySelector('#flow').setAttribute('data-hypit-text-shrink-scale','0.01')";
        QTest::newRow("invalid-runtime-geometry") << "d.querySelector('#flow').style.width='1px'";
    }
    void fixedTextRuntimeStillRejectsStaticAndUnownedChanges() {
        QFETCH(QString, mutation);
        const auto s = snapshot(textLayoutPicture(true, true)); QVERIFY(!s.previewSrcdoc.isEmpty());
        QWebEngineView view; QVERIFY(mount(view, s.previewSrcdoc));
        QTRY_VERIFY_WITH_TIMEOUT(probe(view, s).value("confirmed").toBool(), 1600);
        QVERIFY(js(view, "(()=>{const d=document.querySelector('iframe').contentDocument;" + mutation + ";return true})()").toBool());
        QVERIFY(!probe(view, s).value("confirmed").toBool());
    }
    void compiledTextKeepsQStringPlaceholdersLiteral() {
        const auto s = snapshot(picture().replace("Old title", "Keep %2, %3, %4 literal"));
        QWebEngineView view; QVERIFY(mount(view, s.previewSrcdoc));
        QCOMPARE(js(view, "document.querySelector('iframe').contentDocument.querySelector('#title').textContent").toString(), QString("Keep %2, %3, %4 literal"));
        QTRY_VERIFY_WITH_TIMEOUT(probe(view, s).value("confirmed").toBool(), 1600);
    }
    void supersededInitialFramePromiseStillAllowsDecodedCurrentDocument() {
        // The fixed shim resolves false when another seek supersedes its initial
        // apply; false is a completed seek token, not a runtime failure.
        const auto s = snapshot(picture().replace("Promise.resolve(true)", "Promise.resolve(false)"));
        QWebEngineView view; QVERIFY(mount(view, s.previewSrcdoc));
        QTRY_VERIFY_WITH_TIMEOUT(probe(view, s).value("confirmed").toBool(), 1600);
    }
    void pendingAndRejectedFrameRuntimeRemainUnconfirmed_data() {
        QTest::addColumn<QString>("promise");
        QTest::newRow("pending") << "new Promise(()=>{})";
        QTest::newRow("rejected") << "Promise.reject(new Error('runtime failed'))";
    }
    void pendingAndRejectedFrameRuntimeRemainUnconfirmed() {
        QFETCH(QString, promise);
        const auto s = snapshot(picture().replace("Promise.resolve(true)", promise));
        QWebEngineView view; QVERIFY(mount(view, s.previewSrcdoc));
        ui::StudioTransport transport; transport.attach(&view); transport.setSnapshot(s);
        QTest::qWait(350); QVERIFY(!probe(view, s).value("confirmed").toBool());
        QVERIFY(!transport.previewVersion().isConfirmed()); QVERIFY(!transport.ready());
    }
    void newSrcdocAttributeCannotConfirmOldDocument_data() {
        QTest::addColumn<QString>("nextHtml");
        QTest::newRow("script") << picture("Old title", {}, 18, {}, 2);
        QTest::newRow("static-text") << picture("New title");
        QTest::newRow("font-size") << picture("Old title", {}, 24);
        QTest::newRow("image-src") << picture("Old title", png(Qt::blue));
    }
    void newSrcdocAttributeCannotConfirmOldDocument() {
        QFETCH(QString, nextHtml); QWebEngineView view; const auto old = snapshot(picture()); QVERIFY(mount(view, old.previewSrcdoc));
        QTRY_VERIFY_WITH_TIMEOUT(probe(view, old).value("confirmed").toBool(), 1600);
        const auto next = snapshot(nextHtml, 8);
        const auto transition = js(view, "(()=>{const frame=document.querySelector('.stage iframe');const oldDocument=frame.contentDocument;frame.srcdoc=" + quoted(nextHtml) +
            ";const gap=frame.contentDocument===oldDocument;const state=" + ui::previewProbeScript(next) + ";return {gap:gap,confirmed:state.confirmed,reason:state.reason}})()").toMap();
        QVERIFY(transition.value("gap").toBool()); QVERIFY(!transition.value("confirmed").toBool());
        QVERIFY(waitDocument(view)); QTRY_VERIFY_WITH_TIMEOUT(probe(view, next).value("confirmed").toBool(), 1600);
    }
    void differentHtmlAndMissingBaselineRemainUnconfirmed() {
        QWebEngineView view; const auto s = snapshot(picture()); QVERIFY(mount(view, s.previewSrcdoc));
        QTRY_VERIFY_WITH_TIMEOUT(probe(view, s).value("confirmed").toBool(), 1600);
        QVERIFY(!probe(view, snapshot(picture("Different title"), 8)).value("confirmed").toBool());
        auto unknown = s; unknown.previewSrcdoc.clear(); QVERIFY(!probe(view, unknown).value("confirmed").toBool());
        unknown = s; unknown.sourceFingerprint.clear(); QVERIFY(!probe(view, unknown).value("confirmed").toBool());
    }
    void undecodedVideoDoesNotConfirmEvenWhenImagesAreLoaded() {
        const auto s = snapshot(picture("Video waiting", {}, 18, "<video preload='none' src='data:video/mp4;base64,AAAA'></video>"));
        QWebEngineView view; QVERIFY(mount(view, s.previewSrcdoc));
        QCOMPARE(js(view, "document.querySelector('iframe').contentDocument.querySelector('video').readyState").toInt(), 0);
        QVERIFY(!probe(view, s).value("confirmed").toBool());
        ui::StudioTransport transport; transport.attach(&view); transport.setSnapshot(s); QTest::qWait(350);
        QVERIFY(!transport.previewVersion().isConfirmed()); QVERIFY(!transport.ready());
    }
    void artifactModeRevokesAndCompositionReturnConfirmsAgain() {
        QWebEngineView view; const auto s = snapshot(picture()); QVERIFY(mount(view, s.previewSrcdoc)); ui::StudioTransport transport;
        transport.attach(&view); transport.setSnapshot(s); QTRY_VERIFY_WITH_TIMEOUT(transport.previewVersion().isConfirmed(), 1600);
        js(view, "(()=>{document.querySelector('.stage-scaler').hidden=true;document.querySelector('.stage-artifact').hidden=false;document.querySelector('.stage-return').hidden=false;return true})()");
        QTRY_VERIFY_WITH_TIMEOUT(!transport.previewVersion().isConfirmed(), 1600); QVERIFY(!transport.ready());
        js(view, "(()=>{document.querySelector('.stage-scaler').hidden=false;document.querySelector('.stage-artifact').hidden=true;document.querySelector('.stage-return').hidden=true;return true})()");
        QTRY_VERIFY_WITH_TIMEOUT(transport.previewVersion().isConfirmed(), 1600); QCOMPARE(transport.previewVersion().revision, s.revision);
    }
    void clearAndNewSnapshotRejectQueuedOldConfirmation() {
        QWebEngineView view; const auto s = snapshot(picture()); QVERIFY(mount(view, s.previewSrcdoc)); ui::StudioTransport transport;
        transport.attach(&view); QSignalSpy versions(&transport, &ui::StudioTransport::previewVersionChanged); transport.setSnapshot(s); transport.clear();
        QTest::qWait(350); QVERIFY(!transport.previewVersion().isConfirmed()); QVERIFY(!transport.ready());
        for (const auto &event : versions) QVERIFY(!event.first().value<domain::PreviewVersion>().isConfirmed());
        transport.attach(&view); transport.setSnapshot(s); transport.setSnapshot(snapshot(picture("Next version"), 8));
        QTest::qWait(350); QVERIFY(!transport.previewVersion().isConfirmed());
        transport.setSnapshot(s); QTRY_VERIFY_WITH_TIMEOUT(transport.previewVersion().isConfirmed(), 1600);
        QSignalSpy failedLoad(&view, &QWebEngineView::loadFinished); view.load(QUrl("file:///private/tmp/qvw-preview-missing-page-no-file.html"));
        if (failedLoad.isEmpty()) QVERIFY(failedLoad.wait(5000)); QVERIFY(!failedLoad.last().first().toBool());
        QVERIFY(!transport.previewVersion().isConfirmed()); QVERIFY(!transport.ready());
    }
};
int main(int argc, char **argv) {
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv); app.setFont(QFont("PingFang SC"));
    PreviewVersionTest test; return QTest::qExec(&test, argc, argv);
}
#include "PreviewVersionTest.moc"
