# Review groups

## Group 1: 历史目标复用
Files: app/ui/AgentPanel.*, app/ui/HistoryPanel.*, app/infrastructure/WorkspaceStore.cpp, app/ui/MainWindow.*, app/main.cpp, tests/unit/{AgentPanel,HistoryPanel,WorkspaceStore,ProductExperience}Test.cpp, tests/history-efficiency.cmake.
Focus: full goal preservation, no inherited approval/execution, busy state and filtering visibility.

## Group 2: 素材筛选和恢复
Files: app/controllers/DocumentController.*, app/ui/ImportResultsPanel.*, app/ui/MainWindow.*, app/main.cpp, app/CMakeLists.txt, tests/CMakeLists.txt, CMakeLists.txt, tests/unit/{AssetLibrary,ProductExperience,ImportResultsPanel}Test.cpp, tests/integration/CreationEfficiencyE2ETest.cpp.
Focus: same-project recovery, reentrancy/cancellation, real states, visible-only actions.

Shared MainWindow/main are boundary files included in both groups. Documentation and evidence checked by spec reviewer/root.
