#include "agent/AgentController.h"
#include "agent/AgentTaskStore.h"
#include "agent/ContextBuilder.h"
#include "agent/PlanService.h"
#include "agent/StoryTemplate.h"
#include "agent/ToolDispatcher.h"
#include "services/AssetService.h"
#include "services/ProjectStore.h"
#include "services/ProposalService.h"
#include <QDateTime>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTimer>
#include <QUuid>
namespace qvw::agent {
namespace {
bool busyPhase(const QString &p){return QStringList{"thinking","creating","applying","undoing"}.contains(p);}
QJsonObject baseJson(const domain::AgentBaseline &b){return {{"root",b.root},{"revision",b.revision},{"fingerprint",QString::fromLatin1(b.fingerprint.toHex())},{"scope",b.scope}};}
domain::AgentBaseline readBase(const QJsonObject &o){return {o["root"].toString(),o["revision"].toInt(-1),QByteArray::fromHex(o["fingerprint"].toString().toLatin1()),o["scope"].toString()};}
}
AgentController::AgentController(controllers::DocumentController &d,controllers::EditorController &e,controllers::ExportController &x,ModelClient &m,QString t,QObject *parent)
 :QObject(parent),m_document(d),m_editor(e),m_exporter(x),m_model(m),m_templates(std::move(t)) {
 m_creationDeadline.setSingleShot(true);m_creationDeadline.setInterval(60000);
 m_settlePoll.setSingleShot(true);m_settlePoll.setInterval(150);
 m_settleDeadline.setSingleShot(true);m_settleDeadline.setInterval(10000);
 connect(&m_settlePoll,&QTimer::timeout,this,[this]{if(m_settling){if(m_editor.isBusy())m_settlePoll.start();else m_editor.refresh();}});
 connect(&m_settleDeadline,&QTimer::timeout,this,[this]{if(m_settling)operationFailed("等待编译稳定超时，已完成步骤保留；请刷新后重新审阅剩余操作。");});
 connect(&m_creationDeadline,&QTimer::timeout,this,[this]{backendFailed("创建后等待真实编译快照超时，后续未执行；工程已保留。");});
 connect(&m,&ModelClient::completed,this,[this](const QJsonObject &json){
  if(m_status.phase!="thinking")return;
  if(!sameBase(m_requestBase)){publish("stale","等待模型期间工程已改变，请重新生成。");return;}
  domain::AgentPlan plan;QString error;
  if(!PlanService::parse(json,m_requestBase,&plan,&error)||!PlanService::validate(plan,m_document.project(),m_editor.snapshot(),m_assets,&error)){publish("failed",error);emit failed(error);return;}
  m_plan=plan;m_approval.clear();m_status.canApprove=plan.kind()!="clarify";
  publish(plan.kind()=="clarify"?"clarify":"review",plan.kind()=="clarify"?plan.content["question"].toString():QStringLiteral("方案待审阅，尚未修改工程。"));record("submit_edit_proposal","proposed");emit planReady(plan);
 });
 connect(&m,&ModelClient::failed,this,[this](const QString &error){if(m_status.phase=="thinking"){publish("failed",error);emit failed(error);}});
 connect(&d,&controllers::DocumentController::projectLoaded,this,&AgentController::projectLoaded);
 connect(&d,&controllers::DocumentController::documentClosed,this,[this]{if(m_ownProjectSignal)return;stop();clearExecution();m_scope.clear();m_status={};emit statusChanged(m_status);});
 connect(&e,&controllers::EditorController::snapshotReady,this,&AgentController::snapshotChanged);
 connect(&e,&controllers::EditorController::operationSucceeded,this,&AgentController::operationDone);
 connect(&e,&controllers::EditorController::failed,this,&AgentController::operationFailed);
 connect(&e,&controllers::EditorController::stateChanged,this,[this](bool ready,bool busy,bool undo,bool){m_canUndo=undo;if(ready&&!busy){if(m_creating)QTimer::singleShot(0,this,&AgentController::materializeCreation);else if(m_status.phase=="restoring")QTimer::singleShot(0,this,&AgentController::restore);}});
 connect(&x,&controllers::ExportController::taskChanged,this,[this](const domain::ExportTask &task){if(!m_document.hasProject()||m_status.taskId.isEmpty())return;QString error;
  AgentTaskStore::append(m_document.project(),{{"taskId",m_status.taskId},{"tool","get_build_status"},{"phase",task.phase},{"buildId",task.buildId},{"revision",task.revision},{"fingerprint",QString::fromLatin1(task.sourceFingerprint.toHex())},{"artifact",task.phase=="complete"?task.destination:QString()}},&error);
  if(!error.isEmpty())emit failed(error);persist();});
}
AgentController::~AgentController(){m_model.cancel();}
domain::AgentBaseline AgentController::currentBase() const {const auto s=m_editor.snapshot();return {m_document.hasProject()?m_document.project().rootPath:QString(),s.revision,s.sourceFingerprint,m_scope};}
bool AgentController::sameBase(const domain::AgentBaseline &b) const {const auto now=currentBase();return b.root==now.root&&(b.root.isEmpty()||(b.revision==now.revision&&b.fingerprint==now.fingerprint));}
bool AgentController::setImages(const QStringList &paths){
 if(busyPhase(m_status.phase)||m_inflight||m_settling){emit failed("请等待当前创作步骤完成。");return false;}
 if(paths.isEmpty()||paths.size()>12){emit failed("请选择1–12张本地PNG/JPEG图片。");return false;}
 auto stage=std::make_unique<QTemporaryDir>();domain::Project staged;QString error;
 if(!stage->isValid()||!services::ProjectStore::create(QDir(m_templates).filePath("title-card"),stage->filePath("images"),"素材暂存",&staged,&error)){emit failed(error);return false;}
 domain::AgentAssets assets;
 for(const auto &path:paths){domain::Asset a;if(!services::AssetService::importImage(staged,path,&a,&error)){emit failed(error);return false;}bool exists=false;for(const auto &old:assets)if(old.asset.hash==a.hash)exists=true;if(!exists)assets.append({a,QDir(staged.rootPath).filePath(a.path)});}
 m_staging=std::move(stage);m_stagingProject=staged;m_assets=assets;m_plan.reset();m_approval.clear();m_status.canApprove=false;
 emit assetsChanged(m_assets);publish("idle",QStringLiteral("已准备%1张图片。模型仅接收名称和尺寸，图片保留在本地。").arg(assets.size()));return true;
}
void AgentController::setScope(const QString &id){
 if(m_scope==id)return;m_scope=id;
 // Selection configures the next request. A task keeps the scope it was reviewed with.
 if(m_status.taskId.isEmpty())m_status.scope=id;
 if(m_plan&&m_status.phase=="review"&&m_plan->base.scope!=id){m_status.canApprove=false;m_approval.clear();publish("stale","下一次请求的作用范围已改变，请重新生成方案。");}
}
void AgentController::setSelection(const QString &id){m_selectedEntity=id;}
void AgentController::generate(const QString &goal){
 if(m_inflight||m_creating||m_settling||m_editor.isBusy()||m_document.importingVideo()||m_exporter.isBusy()){emit failed("当前有编辑、导入或导出正在执行。");return;}
 if(goal.trimmed().isEmpty()||goal.toUtf8().size()>8192){emit failed("请输入最多8192字节的创作目标。");return;}
 if(m_document.hasProject()&&(!m_editor.isReady()||m_editor.workspace()!=m_document.project().rootPath)){emit failed("请等待当前工程编译就绪。");return;}
 m_model.cancel();m_plan.reset();m_approval.clear();m_operations={};m_next=0;m_retries=0;m_effectiveDone=0;m_stopRequested=false;
 m_status={};m_status.taskId=QUuid::createUuid().toString(QUuid::WithoutBraces);m_status.goal=goal;m_status.scope=m_scope;m_requestBase=currentBase();m_executionBase=m_requestBase;
 publish("thinking","正在读取工程与素材，等待 Codex 制定方案…");record("get_project_context","read");
 m_model.request(ContextBuilder::prompt(goal,ContextBuilder::build(m_document.project(),m_editor.snapshot(),m_assets,m_scope,m_selectedEntity),m_lastResult),PlanService::schema());
}
void AgentController::updatePlan(const QJsonObject &json){if(!m_plan||m_status.phase!="review")return;domain::AgentPlan p;QString error;if(!PlanService::parse(json,m_plan->base,&p,&error)||!PlanService::validate(p,m_document.project(),m_editor.snapshot(),m_assets,&error)){emit failed(error);return;}m_plan=p;m_approval.clear();m_status.canApprove=true;persist();emit planReady(p);}
void AgentController::approve(const QString &destination){
 if(!m_plan||!m_status.canApprove||m_editor.isBusy()||m_document.importingVideo()||m_exporter.isBusy()){emit failed("没有可执行的当前方案。");return;}
 QString error;if(!PlanService::validate(*m_plan,m_document.project(),m_editor.snapshot(),m_assets,&error)){m_status.canApprove=false;publish("stale",error);emit failed(error);return;}
 m_approval.approve(*m_plan);m_executionBase=m_plan->base;m_status.canApprove=false;m_status.canRetry=false;m_stopRequested=false;m_retries=0;record("apply_approved_proposal","approved-locally");if(m_stopRequested)return;
 if(m_plan->kind()=="create"){
  if(destination.isEmpty()||!m_staging){m_approval.clear();publish("failed","请选择新工程目录，并准备本地图片。");return;}
  const auto prepared=m_staging->filePath("template-"+QUuid::createUuid().toString(QUuid::WithoutBraces));
  if(!StoryTemplate::prepare(QDir(m_templates).filePath("story-reel"),m_plan->content["scenes"].toArray(),prepared,&error)){publish("failed",error);emit failed(error);return;}
  m_creating=true;m_creationDeadline.start();m_ownProjectSignal=true;m_expectedRoot=QFileInfo(destination).absoluteFilePath();publish("creating","正在创建工程、导入图片并编译三段时间线…");
  const bool created=m_document.create(prepared,destination,m_plan->content["projectName"].toString());m_ownProjectSignal=false;
  if(!created){m_creating=false;m_approval.clear();publish("failed","创建未完成，已保留原工程；请检查目标目录。");return;}
  record("create_project_from_template","created");
  for(const auto &a:m_assets)if(!m_document.importImage(a.localPath)){m_creating=false;m_approval.clear();publish("failed","图片导入未完成，工程已保留；请修复后重新生成修改方案。");return;}
  materializeCreation();
 }else if(m_plan->kind()=="edit"){
  m_operations=m_plan->content["operations"].toArray();m_next=0;m_effectiveDone=0;m_status.total=m_operations.size();publish("applying","开始逐项应用已批准的修改…");QTimer::singleShot(0,this,&AgentController::executeNext);
 }
}
void AgentController::projectLoaded(const domain::Project &p){
 if(m_ownProjectSignal){m_expectedRoot=p.rootPath;m_executionBase={p.rootPath,-1,{},{}};m_scope.clear();m_selectedEntity.clear();m_status.scope.clear();return;}
 m_model.cancel();m_stopRequested=true;clearExecution();m_scope.clear();m_status={};emit statusChanged(m_status);QTimer::singleShot(0,this,&AgentController::restore);
}
void AgentController::clearExecution(){
 m_creationDeadline.stop();m_settlePoll.stop();m_settleDeadline.stop();m_settling=false;m_settleReads=0;
 m_creating=false;m_inflight=false;m_undoing=false;m_plan.reset();m_approval.clear();m_operations={};m_next=0;m_effectiveDone=0;
 m_executionBase={};m_requestBase={};m_selectedEntity.clear();m_expectedRoot.clear();m_lastResult.clear();
}
void AgentController::materializeCreation(){
 if(!m_creating||m_stopRequested||!m_editor.isReady()||m_editor.isBusy())return;
 if(m_document.project().rootPath!=m_expectedRoot||m_editor.workspace()!=m_expectedRoot)return;
 QMap<QString,domain::Asset> registered;for(const auto &a:m_document.project().assets)registered.insert(a.hash,a);
 QString error;m_operations=ToolDispatcher::creationEdits(*m_plan,m_editor.snapshot(),registered,&error);
 if(!error.isEmpty()){m_creating=false;m_approval.clear();publish("failed",error);emit failed(error);return;}
 m_creating=false;m_creationDeadline.stop();m_executionBase=currentBase();m_next=0;m_effectiveDone=0;m_status.total=m_operations.size();publish("applying","工程已编译，逐项绑定图片和文案…");QTimer::singleShot(0,this,&AgentController::executeNext);
}
void AgentController::executeNext(){
 if(m_inflight||m_settling||m_stopRequested||m_status.phase!="applying")return;
 if(!m_plan||!m_approval.allows(*m_plan)||!sameBase(m_executionBase)){m_status.canRetry=false;m_approval.clear();publish("stale","工程已改变，剩余操作需要重新审阅。");return;}
 if(m_next>=m_operations.size()){m_status.canRetry=false;m_lastResult=QStringLiteral("实际完成%1项；当前v%2，指纹%3。未自动导出。").arg(m_next).arg(m_editor.snapshot().revision).arg(QString::fromLatin1(m_editor.snapshot().sourceFingerprint.toHex()));publish("complete","方案已应用并回读确认，可继续修改或导出当前版本。");record("refresh_preview","confirmed");return;}
 if(m_editor.isBusy()||!m_editor.isReady()){m_status.canRetry=true;publish("paused","编辑器未就绪，当前步骤未执行。");return;}
 const auto op=m_operations[m_next].toObject();domain::EditProposal edit;edit.revision=m_editor.snapshot().revision;edit.sourceFingerprint=m_editor.snapshot().sourceFingerprint;edit.entityId=op["entityId"].toString();edit.fieldId=op["fieldId"].toString();edit.value=op["value"].toVariant();QString error;
 if((!m_executionBase.scope.isEmpty()&&edit.entityId!=m_executionBase.scope)||!services::ProposalService::validate(edit,m_editor.snapshot(),m_document.project(),&error)){m_approval.clear();publish("failed",error.isEmpty()?"当前操作超出批准范围。":error);emit failed(m_status.message);return;}
 const auto *field=ToolDispatcher::field(m_editor.snapshot(),edit.entityId,edit.fieldId);m_before=field?field->rawValue:QVariant();persist();if(m_stopRequested)return;m_inflight=true;m_editor.edit(edit.entityId,edit.fieldId,edit.value);
}
void AgentController::snapshotChanged(const domain::Snapshot &){
 if(m_settling){settleSnapshot();return;}
 if(m_inflight)return;if(m_status.phase=="restoring"){QTimer::singleShot(0,this,&AgentController::restore);return;}if(m_creating){QTimer::singleShot(0,this,&AgentController::materializeCreation);return;}
 if((m_status.phase=="review"||m_status.phase=="thinking")&&!sameBase(m_requestBase)){m_model.cancel();m_status.canApprove=false;m_approval.clear();publish("stale","人工编辑或刷新改变了工程，旧方案已失效。");}
 else if((m_status.phase=="applying"||m_status.phase=="paused"||m_status.phase=="failed")&&!sameBase(m_executionBase)){m_status.canRetry=false;m_approval.clear();publish("stale","工程已改变，已完成步骤保留；剩余步骤需重新生成方案。");}
}
void AgentController::operationDone(){
 if(!m_inflight)return;m_inflight=false;const auto approvedScope=m_executionBase.scope;m_executionBase=currentBase();m_executionBase.scope=approvedScope;
 if(m_undoing){m_effectiveDone=qMax(0,m_effectiveDone-1);m_approval.clear();m_settling=true;m_settleReads=0;m_settleDeadline.start();m_settlePoll.start();publish("undoing","撤销已写入，正在回读编译版本…");record("undo","confirmed");return;}
 const auto op=m_operations[m_next].toObject();const auto *f=ToolDispatcher::field(m_editor.snapshot(),op["entityId"].toString(),op["fieldId"].toString());
 if(!f||f->rawValue.toString()!=op["value"].toVariant().toString()){m_approval.clear();publish("failed","编辑器返回成功，但当前字段值不符；后续已停止。");return;}
 if(m_before.toString()!=op["value"].toVariant().toString())++m_effectiveDone;++m_next;m_retries=0;m_status.completed=m_next;record("apply_approved_proposal","confirmed");
 m_status.canRetry=false;
 m_settling=true;m_settleReads=0;m_settleDeadline.start();m_settlePoll.start();
 publish("applying",QStringLiteral("已确认%1/%2项修改，正在回读编译版本…").arg(m_next).arg(m_operations.size()));
}
void AgentController::settleSnapshot(){
 auto now=currentBase();now.scope=m_executionBase.scope;
 if(now.root!=m_executionBase.root||now.fingerprint!=m_executionBase.fingerprint){
  m_settling=false;m_settlePoll.stop();m_settleDeadline.stop();m_approval.clear();m_status.canRetry=false;
  publish("stale","回读发现其他源码改动，已完成步骤保留；剩余操作需要重新审阅。");return;
 }
 m_executionBase=now;
 if(++m_settleReads<2){m_settlePoll.start();return;}
 m_settling=false;m_settleDeadline.stop();
 record("get_project_context","same-source-recompiled");
 if(m_undoing){m_undoing=false;m_status.canRetry=false;publish("paused","已通过共享编辑历史撤销上一步；剩余操作需重新审阅。");return;}
 if(m_stopRequested){m_status.canRetry=m_next<m_operations.size()&&m_plan&&m_approval.allows(*m_plan);publish("paused","已停止后续操作，已完成步骤与编译版本已确认。");return;}
 QTimer::singleShot(0,this,&AgentController::executeNext);
}
void AgentController::operationFailed(const QString &error){
 if(m_settling){m_settling=false;m_settlePoll.stop();m_settleDeadline.stop();m_status.canRetry=false;m_approval.clear();m_lastResult=error;publish("failed",error);record("get_project_context","read-failed");emit failed(error);return;}
 if(!m_inflight)return;m_inflight=false;m_undoing=false;auto now=currentBase();now.scope=m_executionBase.scope;const bool preserved=now.root==m_executionBase.root&&now.fingerprint==m_executionBase.fingerprint;
 if(preserved)m_executionBase=now;else m_approval.clear();m_status.canRetry=preserved&&m_retries<2;m_lastResult=QStringLiteral("第%1步执行失败：%2；成功前缀%3项保留。").arg(m_next+1).arg(error).arg(m_next);publish(preserved?"failed":"stale",m_lastResult);record("apply_approved_proposal","failed");emit failed(error);
}
void AgentController::stop(){
 m_model.cancel();m_creationDeadline.stop();m_stopRequested=true;m_creating=false;m_status.canApprove=false;
 if(busyPhase(m_status.phase)||m_status.phase=="review"){m_status.canRetry=!m_inflight&&!m_settling&&!m_operations.isEmpty()&&sameBase(m_executionBase)&&m_plan&&m_approval.allows(*m_plan);publish("paused",m_inflight?"当前写入正在确认，确认后停止后续步骤。":"已停止后续操作，已完成步骤保留。");}
}
void AgentController::retry(){if(!m_status.canRetry||m_inflight||m_retries>=2||!m_plan||!m_approval.allows(*m_plan)||!sameBase(m_executionBase)){emit failed("无法直接重试，请重新生成并审阅剩余修改。");return;}++m_retries;m_stopRequested=false;m_status.canRetry=false;publish("applying",QStringLiteral("重试未完成步骤（%1/2），保留成功前缀。").arg(m_retries));record("apply_approved_proposal","retry");QTimer::singleShot(0,this,&AgentController::executeNext);}
void AgentController::repair(){if(m_inflight)return;const auto goal=m_status.goal;if(!goal.isEmpty())generate(goal+"\n请依据当前工程只提出未完成或需要修复的内容，保留已正确完成的内容。");}
void AgentController::undoLast(){if(!m_canUndo||m_effectiveDone<=0||m_editor.isBusy()||!sameBase(m_executionBase)||m_inflight||m_settling){emit failed("当前版本不能安全撤销本次任务的上一步；可使用编辑器历史。");return;}m_stopRequested=true;m_approval.clear();m_status.canRetry=false;m_status.canApprove=false;m_inflight=true;m_undoing=true;publish("undoing","正在通过共享编辑历史撤销上一步…");m_editor.undo();}
void AgentController::restore(){
 if(!m_document.hasProject()||busyPhase(m_status.phase)||m_inflight)return;QJsonObject state;QString error;
 if(!AgentTaskStore::load(m_document.project(),&state,&error)){
  const QFileInfo savedFile(QDir(m_document.project().rootPath).filePath(".workbench/agent/current.json"));
  if(savedFile.exists()||savedFile.isSymLink()){m_approval.clear();m_status.canApprove=false;m_status.canRetry=false;m_status.phase="failed";m_status.message=error;emit statusChanged(m_status);emit failed(error);}return;
 }
 const auto saved=readBase(state["executionBase"].toObject());if(saved.root!=m_document.project().rootPath)return;
 m_scope=state["scope"].toString();m_executionBase=saved;m_status.scope=m_scope;
 m_lastResult=state["lastResult"].toString().left(4096);
 m_status.taskId=state["taskId"].toString();m_status.goal=state["goal"].toString();m_status.completed=state["next"].toInt();m_status.total=state["operations"].toArray().size();m_next=m_status.completed;m_operations=state["operations"].toArray();m_retries=state["retries"].toInt();m_approval.clear();m_effectiveDone=0;
 m_status.events.clear();for(const auto &v:state["events"].toArray())if(v.isString())m_status.events.append(v.toString());
 if(!m_editor.isReady()||m_editor.workspace()!=m_document.project().rootPath){m_status.phase="restoring";m_status.message="已读取任务记录，等待当前工程的实际快照检查版本。";emit statusChanged(m_status);return;}
 if(saved.fingerprint!=m_editor.snapshot().sourceFingerprint){m_status.canApprove=false;publish("stale","任务已恢复，但工程有新改动；旧方案不会自动执行。");return;}
 m_executionBase=currentBase();m_requestBase=m_executionBase;
 if(m_operations.isEmpty()){
  const auto original=state["plan"].toObject();domain::AgentPlan proposal;
  if(QStringList{"edit","clarify"}.contains(original["kind"].toString())&&PlanService::parse(original,m_requestBase,&proposal,&error)&&PlanService::validate(proposal,m_document.project(),m_editor.snapshot(),{},&error)){m_plan=proposal;m_status.canApprove=proposal.kind()=="edit";publish(proposal.kind()=="clarify"?"clarify":"review",proposal.kind()=="clarify"?original["question"].toString():QStringLiteral("已恢复待审阅方案，请重新确认。"));emit planReady(proposal);}
  else publish("paused","已恢复目标与执行记录；创建方案需重新选择本地图片并生成。 ");return;
 }
 if(m_next<0||m_next>m_operations.size()||m_operations.size()>24){m_operations={};publish("failed","任务步骤记录损坏，未恢复执行。");return;}
 if(m_next==m_operations.size()){m_status.canApprove=false;publish("complete","已恢复完成任务，工程内容与保存指纹一致。");return;}
 QJsonArray remaining;for(int i=m_next;i<m_operations.size();++i)remaining.append(m_operations[i]);
 QJsonObject content{{"format","qvw.agent-plan@1"},{"kind","edit"},{"summary",QStringLiteral("成功%1项保留，审阅剩余%2项。").arg(m_next).arg(remaining.size())},{"question",""},{"projectName",""},{"scenes",QJsonArray{}},{"operations",remaining}};
 domain::AgentPlan proposal;if(!PlanService::parse(content,m_requestBase,&proposal,&error)||!PlanService::validate(proposal,m_document.project(),m_editor.snapshot(),{},&error)){publish("failed",error);return;}m_plan=proposal;m_status.canApprove=true;publish("review","已恢复剩余修改；必须重新确认，旧批准不会恢复。");emit planReady(proposal);
}
void AgentController::startExport(const QString &destination){if(m_inflight||m_settling||busyPhase(m_status.phase)||m_editor.isBusy()||!m_editor.isReady()||!m_document.hasProject()){emit failed("请先完成当前修改再导出。");return;}record("start_build","requested-by-user");m_exporter.startExport(m_editor.snapshot(),destination);}
void AgentController::backendFailed(const QString &error){if(!m_creating)return;m_creating=false;m_creationDeadline.stop();m_approval.clear();m_status.canRetry=false;publish("failed",error);record("create_project_from_template","compile-failed");emit failed(error);}
void AgentController::publish(const QString &phase,const QString &text){m_status.phase=phase;m_status.message=text;if(phase=="failed")m_lastResult=text;m_status.revision=m_editor.snapshot().revision;m_status.canUndo=m_canUndo&&m_effectiveDone>0&&!m_inflight&&!m_settling&&sameBase(m_executionBase);if(phase!="review")m_status.canApprove=false;persist();emit statusChanged(m_status);emit message(text);}
void AgentController::persist(){
 if(!m_document.hasProject()||m_status.taskId.isEmpty())return;
 // Never save a previous project's task into a newly opened document.
 if(!m_executionBase.root.isEmpty()&&m_executionBase.root!=m_document.project().rootPath&&!m_creating)return;
 const auto task=m_exporter.task();QJsonArray events;for(const auto &e:m_status.events)events.append(e);
 QJsonObject state{{"format","qvw.agent-task@1"},{"projectId",m_document.project().rootPath},{"taskId",m_status.taskId},{"goal",m_status.goal},{"scope",m_status.scope},{"phase",m_status.phase},{"plan",m_plan?m_plan->content:QJsonObject{}},{"planBase",m_plan?baseJson(m_plan->base):QJsonObject{}},{"approvalIdentity",QString::fromLatin1(m_approval.identity().toHex())},{"executionBase",baseJson(m_executionBase.root.isEmpty()?currentBase():m_executionBase)},{"operations",m_operations},{"next",m_next},{"retries",m_retries},{"events",events},{"buildId",task.buildId},{"exportFingerprint",QString::fromLatin1(task.sourceFingerprint.toHex())},{"artifact",task.phase=="complete"?task.destination:QString()}};
 state["message"]=m_status.message.left(4096);state["lastResult"]=m_lastResult.left(4096);
 state["selectedEntity"]=m_selectedEntity;
 QString error;if(!AgentTaskStore::save(m_document.project(),state,&error)){m_stopRequested=true;m_status.canApprove=false;m_status.canRetry=false;m_approval.clear();emit failed(error);}
}
void AgentController::record(const QString &tool,const QString &outcome){
 m_status.events.append(QStringLiteral("%1 · %2 · v%3").arg(tool,outcome).arg(m_editor.snapshot().revision));while(m_status.events.size()>120)m_status.events.removeFirst();
 if(m_document.hasProject()&&!m_status.taskId.isEmpty()){
  QString error;QJsonObject event{{"at",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"projectId",m_document.project().rootPath},{"taskId",m_status.taskId},{"tool",tool},{"outcome",outcome},{"step",m_next},{"revision",m_editor.snapshot().revision},{"fingerprint",QString::fromLatin1(m_editor.snapshot().sourceFingerprint.toHex())}};
  event["approvalIdentity"]=QString::fromLatin1(m_approval.identity().toHex());
  if(m_plan)event["planIdentity"]=QString::fromLatin1(PlanService::identity(*m_plan).toHex());
  if(outcome.contains("failed"))event["error"]=m_status.message.left(1024);
  const int index=outcome=="confirmed"?m_next-1:m_next;
  if(tool=="apply_approved_proposal"&&index>=0&&index<m_operations.size()&&outcome!="approved-locally"){
   const auto op=m_operations[index].toObject();event["operationIndex"]=index;event["entityId"]=op["entityId"];event["fieldId"]=op["fieldId"];
   event["valueHash"]=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(QJsonArray{op["value"]}).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
  }
  if(!AgentTaskStore::append(m_document.project(),event,&error)){m_stopRequested=true;m_approval.clear();emit failed(error);}
 }persist();
}
}
