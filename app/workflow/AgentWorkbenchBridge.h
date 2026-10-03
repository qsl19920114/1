#pragma once
namespace qvw::ui { class MainWindow; }
namespace qvw::agent { class AgentController; }
namespace qvw::workflow {
// Bind once per window/controller pair. Connections use QObject lifetimes.
void bindAgentWorkbench(ui::MainWindow &window,agent::AgentController &agent);
}
