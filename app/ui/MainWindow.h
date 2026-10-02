#pragma once
#include "domain/Snapshot.h"
#include "domain/Project.h"
#include <QMainWindow>
#include <QUrl>
class QLabel;
class QPlainTextEdit;
class QTreeWidget;
class QStackedWidget;
class QWebEngineView;
class QWebEngineProfile;
class QAction;
namespace qvw::ui {
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void showSnapshot(const domain::Snapshot &snapshot);
    void appendLog(const QString &line);
    void showPreview(const QUrl &url);
    void clearProject();
    void showError(const QString &error);
    void setBackendAvailable(bool ready);
    void showDocument(const domain::Project &project);
    void clearDocument();
    QWebEngineView *previewView() const { return m_webView; }
signals:
    void openRequested(const QString &workspace, const QString &run, const QString &runtime);
    void newDocumentRequested(const QString &directory, const QString &name);
    void openDocumentRequested(const QString &manifest);
    void saveDocumentRequested();
    void importImageRequested(const QString &path);
    void closeRequested();
    void refreshRequested();
    void configurationRequested(const QString &path);
    void previewLoaded(bool ok);
private:
    QWidget *buildProjectPanel();
    QWidget *buildPreviewPanel();
    QWidget *buildInspectorPanel();
    QWidget *buildTaskPanel();
    void openProjectDialog();
    void newDocumentDialog();
    void showSelectedInspector();
    QTreeWidget *m_componentTree = nullptr;
    QTreeWidget *m_inspectorTable = nullptr;
    QLabel *m_previewPlaceholder = nullptr;
    QLabel *m_spaceSummary = nullptr;
    QPlainTextEdit *m_taskLog = nullptr;
    QStackedWidget *m_previewStack = nullptr;
    QWebEngineView *m_webView = nullptr;
    QWebEngineProfile *m_webProfile = nullptr;
    QTreeWidget *m_assetTree = nullptr;
    QLabel *m_documentTitle = nullptr;
    QAction *m_newAction = nullptr;
    QAction *m_documentOpenAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_importAction = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_closeAction = nullptr;
    QAction *m_refreshAction = nullptr;
    QUrl m_previewUrl;
    domain::Snapshot m_snapshot;
};
}
