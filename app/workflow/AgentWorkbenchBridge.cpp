#include "workflow/AgentWorkbenchBridge.h"
#include "agent/AgentController.h"
#include "ui/MainWindow.h"
#include "ui/AgentPanel.h"
namespace qvw::workflow {
void bindAgentWorkbench(ui::MainWindow &window,agent::AgentController &agent) {
    QObject::connect(window.agentPanel(),&qvw::ui::AgentPanel::imagesRequested,&agent,&qvw::agent::AgentController::setImages);
    QObject::connect(window.agentPanel(),&qvw::ui::AgentPanel::generateRequested,&agent,&qvw::agent::AgentController::generate);
    QObject::connect(window.agentPanel(),&qvw::ui::AgentPanel::scopeChanged,&agent,&qvw::agent::AgentController::setScope);
    QObject::connect(window.agentPanel(),&qvw::ui::AgentPanel::selectionChanged,&agent,&qvw::agent::AgentController::setSelection);
    QObject::connect(window.agentPanel(),&qvw::ui::AgentPanel::approveRequested,&agent,[&agent](const QJsonObject &edited,const QString &destination){
        agent.updatePlan(edited);
        if(agent.plan()&&agent.plan()->content==edited&&agent.status().canApprove)agent.approve(destination);
    });
    QObject::connect(window.agentPanel(),&qvw::ui::AgentPanel::stopRequested,&agent,&qvw::agent::AgentController::stop);
    QObject::connect(window.agentPanel(),&qvw::ui::AgentPanel::retryRequested,&agent,&qvw::agent::AgentController::retry);
    QObject::connect(window.agentPanel(),&qvw::ui::AgentPanel::repairRequested,&agent,&qvw::agent::AgentController::repair);
    QObject::connect(window.agentPanel(),&qvw::ui::AgentPanel::undoRequested,&agent,&qvw::agent::AgentController::undoLast);
    QObject::connect(window.agentPanel(),&qvw::ui::AgentPanel::restoreRequested,&agent,&qvw::agent::AgentController::restore);
    QObject::connect(&agent,&qvw::agent::AgentController::statusChanged,&window,&qvw::ui::MainWindow::showAgentStatus);
    QObject::connect(&agent,&qvw::agent::AgentController::planReady,&window,&qvw::ui::MainWindow::showAgentPlan);
    QObject::connect(&agent,&qvw::agent::AgentController::assetsChanged,window.agentPanel(),&qvw::ui::AgentPanel::showAssets);
    QObject::connect(&agent,&qvw::agent::AgentController::failed,&window,[&window](const QString &text){window.appendLog("Agent 未完成："+text);});
    QObject::connect(&agent,&qvw::agent::AgentController::message,&window,&qvw::ui::MainWindow::appendLog);
    QObject::connect(&agent,&qvw::agent::AgentController::publicMessageReceived,window.agentPanel(),&qvw::ui::AgentPanel::showPublicMessage);
 }
}
