#pragma once
#include "domain/Snapshot.h"
#include <QObject>
#include <QPointer>
#include <QTimer>
class QWebEngineView;
namespace qvw::ui {
/// The same read-only DOM observation is used by polling and diagnostics.
QString previewProbeScript(const domain::Snapshot &snapshot);
class StudioTransport : public QObject {
    Q_OBJECT
public:
    explicit StudioTransport(QObject *parent=nullptr);
    void attach(QWebEngineView *view);
    void setSpace(const domain::CanvasSpace &space);
    void setSnapshot(const domain::Snapshot &snapshot);
    void clear();
    void seek(int frame);
    void togglePlayback();
    void step(int direction);
    void setReduced(bool reduced);
    bool ready() const { return m_ready; }
    domain::PreviewVersion previewVersion() const { return m_previewVersion; }
signals:
    void positionChanged(bool ready,int frame,int lastFrame,const QString &time);
    void previewVersionChanged(const qvw::domain::PreviewVersion &version);
private:
    void poll();
    void command(const QString &script);
    void invalidate();
    void publishVersion(const domain::PreviewVersion &version);
    QPointer<QWebEngineView> m_view;
    QMetaObject::Connection m_loadStarted,m_loadFinished,m_viewDestroyed;
    QTimer m_timer;
    domain::CanvasSpace m_space;
    domain::Snapshot m_snapshot;
    domain::PreviewVersion m_previewVersion;
    quint64 m_generation=0;
    bool m_pending=false,m_ready=false,m_reduced=true;
    bool m_loading=false,m_loadFailed=false;
};
}
