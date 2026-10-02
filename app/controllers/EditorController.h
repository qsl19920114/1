#pragma once
#include "backend/hypit/StudioClient.h"
#include "backend/hypit/StudioWriter.h"
#include "services/SourceEditGuard.h"
#include <QTimer>
namespace qvw::controllers {
class EditorController : public QObject {
    Q_OBJECT
public:
    explicit EditorController(QObject *parent=nullptr);
    void attach(QUrl baseUrl,QString workspace);
    void clear();
    void acceptSnapshot(const domain::Snapshot &snapshot);
    void refresh();
    void edit(QString entityId,QString fieldId,QVariant value);
    void undo();
    void redo();
    void replaceSource(QString path,QString text);
    bool isBusy() const { return m_phase!=Phase::Idle; }
    const domain::Snapshot &snapshot() const { return m_snapshot; }
    void setTimeoutMs(int timeout);
signals:
    void snapshotReady(const qvw::domain::Snapshot &snapshot);
    void stateChanged(bool ready,bool busy,bool canUndo,bool canRedo);
    void message(const QString &text);
    void failed(const QString &text);
    void operationSucceeded();
private:
    enum class Phase { Idle,Refresh,Preflight,Write,Confirm,Recover,RestorePoll };
    enum class Intent { New,Undo,Redo };
    struct Change { bool source=false;QString entity,field,path;QVariant before,after; };
    void begin(Change change,Intent intent);
    void read(const domain::Snapshot &snapshot);
    void readFailed(int httpStatus,int serverRevision,const QString &error);
    void writeFailed(backend::hypit::StudioWriter::WriteFailure kind,const QString &error);
    void finish(const domain::Snapshot *snapshot,const QString &error={},bool succeeded=false);
    void emitState();
    void resetHistory();
    void pollRestore();
    bool available();
    backend::hypit::StudioClient m_reader;
    backend::hypit::StudioWriter m_writer;
    services::SourceEditGuard m_guard;
    QTimer m_poll,m_restoreDeadline;
    QUrl m_url;QString m_workspace;
    domain::Snapshot m_snapshot,m_preflight;
    QVector<Change> m_history;
    int m_cursor=0,m_ack=-1,m_timeoutMs=15000;
    quint64 m_generation=0;
    bool m_ready=false,m_keepHistory=false;
    Phase m_phase=Phase::Idle;
    Intent m_intent=Intent::New;
    Change m_change;
    QString m_recoveryError;
};
}
