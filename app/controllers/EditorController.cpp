#include "controllers/EditorController.h"
#include <QJsonValue>
#include <cmath>
namespace qvw::controllers {
namespace {
using domain::Snapshot;
using backend::hypit::StudioWriter;
const domain::InspectorField *fieldIn(const Snapshot &snapshot,const QString &entity,const QString &id) {
    for(const auto &track:snapshot.tracks)for(const auto &clip:track.clips)if(clip.id==entity)
        for(const auto &field:clip.inspector)if(field.id==id)return &field;
    return nullptr;
}
// Studio snapshots expose authored literals, while parameterAuthorValue
// (pinned studio/server.ts:416-420) requires typed numbers and booleans on POST.
QVariant semanticValue(const domain::InspectorField &field) {
    if(field.control==domain::ControlKind::Number) {
        bool valid=false;const auto number=field.rawValue.toDouble(&valid);
        return valid&&std::isfinite(number)?QVariant(number):QVariant();
    }
    if(field.control==domain::ControlKind::Boolean) {
        if(field.rawValue.typeId()==QMetaType::Bool)return field.rawValue;
        if(field.rawValue.typeId()==QMetaType::QString) {
            if(field.rawValue.toString()=="true")return true;
            if(field.rawValue.toString()=="false")return false;
        }
        return {};
    }
    return field.rawValue;
}
bool validInput(const domain::InspectorField &field,const QVariant &value) {
    const auto json=QJsonValue::fromVariant(value);
    switch(field.control) {
    case domain::ControlKind::Number:return json.isDouble()&&std::isfinite(value.toDouble());
    case domain::ControlKind::Boolean:return json.isBool();
    default:return !json.isArray()&&!json.isObject()&&!json.isUndefined()
        &&QJsonValue::fromVariant(field.rawValue).type()==json.type();
    }
}
bool sameValue(const QVariant &a,const QVariant &b){return QJsonValue::fromVariant(a)==QJsonValue::fromVariant(b);}
bool sameVersion(const Snapshot &a,const Snapshot &b){return a.revision==b.revision&&a.sourceFingerprint==b.sourceFingerprint;}
bool hasSources(const Snapshot &snapshot){return snapshot.isLoaded()&&!snapshot.sourceFiles.isEmpty()&&!snapshot.sourceFingerprint.isEmpty();}
}
EditorController::EditorController(QObject *parent):QObject(parent) {
    m_poll.setSingleShot(true);m_restoreDeadline.setSingleShot(true);
    connect(&m_reader,&backend::hypit::StudioClient::snapshotReady,this,&EditorController::read);
    connect(&m_reader,&backend::hypit::StudioClient::requestFailed,this,&EditorController::readFailed);
    connect(&m_writer,&StudioWriter::completed,this,[this](int revision) {
        if(m_phase!=Phase::Write)return;
        m_ack=revision;m_phase=Phase::Confirm;m_reader.fetchSession(m_url);
    });
    connect(&m_writer,&StudioWriter::failed,this,[this](StudioWriter::WriteFailure kind,const QString &error,int){writeFailed(kind,error);});
    connect(&m_poll,&QTimer::timeout,this,&EditorController::pollRestore);
    connect(&m_restoreDeadline,&QTimer::timeout,this,[this] {
        if(m_phase!=Phase::RestorePoll)return;
        m_reader.cancel();resetHistory();finish(nullptr,m_recoveryError+QStringLiteral(" 恢复后的会话未能在 3 秒内确认，请手动刷新。"));
    });
}
void EditorController::setTimeoutMs(int timeout){m_timeoutMs=qMax(1,timeout);m_reader.setTimeoutMs(m_timeoutMs);m_writer.setTimeoutMs(m_timeoutMs);}
void EditorController::resetHistory(){m_history.clear();m_cursor=0;}
void EditorController::emitState(){emit stateChanged(m_ready,isBusy(),m_ready&&!isBusy()&&m_cursor>0,m_ready&&!isBusy()&&m_cursor<m_history.size());}
void EditorController::clear() {
    ++m_generation;m_reader.cancel();m_writer.cancel();m_poll.stop();m_restoreDeadline.stop();m_guard.clear();
    m_reader.setTimeoutMs(m_timeoutMs);m_phase=Phase::Idle;m_ready=false;m_snapshot={};m_preflight={};m_url=QUrl();m_workspace.clear();resetHistory();emitState();
}
void EditorController::attach(QUrl url,QString workspace) {
    clear();m_url=std::move(url);m_workspace=std::move(workspace);
}
void EditorController::acceptSnapshot(const Snapshot &snapshot) {
    ++m_generation;m_reader.cancel();m_writer.cancel();m_poll.stop();m_restoreDeadline.stop();m_guard.clear();m_reader.setTimeoutMs(m_timeoutMs);
    if(isBusy()||!sameVersion(m_snapshot,snapshot))resetHistory();
    m_phase=Phase::Idle;m_snapshot=snapshot;m_ready=hasSources(snapshot)&&!m_url.isEmpty();
    const auto generation=m_generation;emitState();if(generation==m_generation)emit snapshotReady(m_snapshot);
}
bool EditorController::available() {
    if(isBusy()){emit failed(QStringLiteral("已有编辑或刷新操作正在进行，请等待完成。"));return false;}
    if(!m_ready){emit failed(QStringLiteral("会话尚未就绪，请先刷新有效会话后再编辑。"));return false;}
    return true;
}
void EditorController::refresh() {
    if(isBusy()){emit failed(QStringLiteral("已有操作正在进行，请等待完成。"));return;}
    if(m_url.isEmpty()){emit failed(QStringLiteral("请先打开 Studio 会话。"));return;}
    m_phase=Phase::Refresh;const auto generation=++m_generation;emitState();if(generation==m_generation)m_reader.fetchSession(m_url);
}
void EditorController::edit(QString entity,QString id,QVariant value) {
    if(!available())return;
    const auto *field=fieldIn(m_snapshot,entity,id);
    if(!field||!field->isEditable()||!validInput(*field,value)||!semanticValue(*field).isValid()) {
        emit failed(QStringLiteral("该属性不存在、不可写，或输入值不是匹配的简单类型。"));return;
    }
    Change change;change.entity=std::move(entity);change.field=std::move(id);change.before=semanticValue(*field);change.after=std::move(value);begin(change,Intent::New);
}
void EditorController::replaceSource(QString path,QString text) {
    if(!available())return;
    if(!m_snapshot.sourceFiles.contains(path)){emit failed(QStringLiteral("只能编辑当前会话声明的源码文件。"));return;}
    Change change;change.source=true;change.path=std::move(path);change.before=m_snapshot.sourceFiles.value(change.path);change.after=std::move(text);begin(change,Intent::New);
}
void EditorController::undo(){if(available()){if(m_cursor>0)begin(m_history[m_cursor-1],Intent::Undo);else emit failed(QStringLiteral("没有可撤销的操作。"));}}
void EditorController::redo(){if(available()){if(m_cursor<m_history.size())begin(m_history[m_cursor],Intent::Redo);else emit failed(QStringLiteral("没有可重做的操作。"));}}
void EditorController::begin(Change change,Intent intent) {
    m_change=std::move(change);m_intent=intent;m_phase=Phase::Preflight;m_ack=-1;m_recoveryError.clear();m_guard.clear();
    const auto generation=++m_generation;emitState();if(generation==m_generation)m_reader.fetchSession(m_url);
}
void EditorController::read(const Snapshot &snapshot) {
    const auto target=m_intent==Intent::Undo?m_change.before:m_change.after;
    switch(m_phase) {
    case Phase::Idle:case Phase::Write:return;
    case Phase::Refresh:
        if(!sameVersion(m_snapshot,snapshot))resetHistory();
        finish(&snapshot);return;
    case Phase::Preflight: {
        if(!sameVersion(m_snapshot,snapshot)||!hasSources(snapshot)) {
            resetHistory();finish(&snapshot,QStringLiteral("源码或 revision 已发生外部变化；已刷新会话并重置历史，本次操作未写入。"));return;
        }
        m_preflight=snapshot;
        if(m_change.source) {
            QString error;
            if(!m_guard.arm(m_workspace,snapshot,m_change.path,target.toString(),&error)){resetHistory();finish(nullptr,error);return;}
        } else {
            const auto *field=fieldIn(snapshot,m_change.entity,m_change.field);
            if(!field||!field->isEditable()){resetHistory();finish(&snapshot,QStringLiteral("最新会话中的属性不可编辑。"));return;}
        }
        m_phase=Phase::Write;
        if(m_change.source)m_writer.replaceSource(m_url,snapshot.revision,m_change.path,target.toString());
        else m_writer.adjustParameter(m_url,snapshot.revision,m_change.entity,m_change.field,target);
        return;
    }
    case Phase::Confirm: {
        bool matches=snapshot.revision==m_ack;
        if(m_change.source) {
            matches=matches&&snapshot.sourceFiles.contains(m_change.path)
                &&snapshot.sourceFiles.value(m_change.path)==target.toString();
            // Imports can legitimately add/remove dependency files from the
            // compiled closure. Retained, unrelated files must stay unchanged.
            for(auto it=m_preflight.sourceFiles.cbegin();matches&&it!=m_preflight.sourceFiles.cend();++it)
                if(it.key()!=m_change.path&&snapshot.sourceFiles.contains(it.key()))
                    matches=snapshot.sourceFiles.value(it.key())==it.value();
        } else {
            const auto *field=fieldIn(snapshot,m_change.entity,m_change.field);
            matches=matches&&field&&semanticValue(*field).isValid()&&sameValue(semanticValue(*field),target);
        }
        if(!matches){resetHistory();finish(&snapshot,QStringLiteral("写入已接受，但回读的 revision 或内容不符；未记录成功，历史已重置。"));return;}
        if(m_intent==Intent::Undo)--m_cursor;
        else if(m_intent==Intent::Redo)++m_cursor;
        else if(!sameValue(m_change.before,m_change.after)) {
            m_history.resize(m_cursor);m_history.append(m_change);++m_cursor;
        }
        finish(&snapshot,{},true);return;
    }
    case Phase::Recover:
        if(!m_keepHistory||snapshot.sourceFingerprint!=m_preflight.sourceFingerprint)resetHistory();
        finish(&snapshot,m_recoveryError);return;
    case Phase::RestorePoll:
        if(snapshot.sourceFingerprint!=m_preflight.sourceFingerprint) {
            resetHistory();finish(&snapshot,m_recoveryError+QStringLiteral(" 恢复回读检测到其他源码变化，历史已重置。"));return;
        }
        finish(&snapshot,m_recoveryError+QStringLiteral(" 已恢复原文件并确认有效会话，本次编辑未应用。"));return;
    }
}
void EditorController::writeFailed(StudioWriter::WriteFailure kind,const QString &error) {
    if(m_phase!=Phase::Write)return;
    m_guard.clear();m_recoveryError=error;
    m_keepHistory=kind==StudioWriter::Rejected||kind==StudioWriter::InvalidRequest;
    if(!m_keepHistory)resetHistory();
    m_phase=Phase::Recover;m_reader.fetchSession(m_url);
}
void EditorController::readFailed(int httpStatus,int serverRevision,const QString &error) {
    if(m_phase==Phase::Idle||m_phase==Phase::Write)return;
    if(m_phase==Phase::RestorePoll) {m_poll.start(100);return;}
    if(m_phase==Phase::Confirm&&m_change.source&&httpStatus==500&&serverRevision==m_ack) {
        m_recoveryError=QStringLiteral("源码提交后的编译或回读失败：%1。").arg(error);
        QString restoreError;
        if(!m_guard.restore(&restoreError)){resetHistory();finish(nullptr,m_recoveryError+restoreError);return;}
        m_phase=Phase::RestorePoll;m_reader.setTimeoutMs(qMin(m_timeoutMs,250));m_restoreDeadline.start(3000);m_poll.start(100);return;
    }
    resetHistory();finish(nullptr,m_recoveryError.isEmpty()?error:m_recoveryError+QStringLiteral(" 回读也失败：")+error);
}
void EditorController::pollRestore(){if(m_phase==Phase::RestorePoll)m_reader.fetchSession(m_url);}
void EditorController::finish(const Snapshot *snapshot,const QString &error,bool succeeded) {
    m_guard.clear();m_poll.stop();m_restoreDeadline.stop();m_reader.setTimeoutMs(m_timeoutMs);
    m_phase=Phase::Idle;m_ready=snapshot&&hasSources(*snapshot)&&!m_url.isEmpty();
    if(snapshot)m_snapshot=*snapshot;
    const auto generation=m_generation;emitState();if(generation!=m_generation)return;
    if(snapshot){emit snapshotReady(m_snapshot);if(generation!=m_generation)return;}
    if(!error.isEmpty()){emit failed(error);return;}
    if(succeeded)emit operationSucceeded();
}
}
