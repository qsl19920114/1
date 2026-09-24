#pragma once

#include "domain/Snapshot.h"

#include <QMainWindow>

class QLabel;
class QPlainTextEdit;
class QTreeWidget;

namespace qvw::ui {

/// The four-region shell from PROJECT_PLAN §6: project tree, preview, inspector
/// and task log. T008 only builds the layout; the preview stays a placeholder
/// until the Studio process is wired up in T010.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void showSnapshot(const domain::Snapshot &snapshot);
    void appendLog(const QString &line);

private:
    QWidget *buildProjectPanel();
    QWidget *buildPreviewPanel();
    QWidget *buildInspectorPanel();
    QWidget *buildTaskPanel();

    QTreeWidget *m_componentTree = nullptr;
    QTreeWidget *m_inspectorTable = nullptr;
    QLabel *m_previewPlaceholder = nullptr;
    QLabel *m_spaceSummary = nullptr;
    QPlainTextEdit *m_taskLog = nullptr;
};

}
