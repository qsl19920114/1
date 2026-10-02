#pragma once
#include "domain/Snapshot.h"
#include <QObject>
#include <QPointer>
#include <QTimer>
class QWebEngineView;
namespace qvw::ui {
class StudioTransport : public QObject {
    Q_OBJECT
public:
    explicit StudioTransport(QObject *parent=nullptr);
    void attach(QWebEngineView *view);
    void setSpace(const domain::CanvasSpace &space);
    void clear();
    void seek(int frame);
    void togglePlayback();
    void step(int direction);
    void setReduced(bool reduced);
    bool ready() const { return m_ready; }
signals:
    void positionChanged(bool ready,int frame,int lastFrame,const QString &time);
private:
    void poll();
    void command(const QString &script);
    QPointer<QWebEngineView> m_view;
    QTimer m_timer;
    domain::CanvasSpace m_space;
    quint64 m_generation=0;
    bool m_pending=false,m_ready=false,m_reduced=true;
};
}
