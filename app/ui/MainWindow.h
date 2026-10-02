#pragma once
#include "domain/Snapshot.h"
#include "domain/Project.h"
#include "domain/ExportTask.h"
#include <QMainWindow>
#include <QUrl>
class QLabel;
class QPlainTextEdit;
class QTreeWidget;
class QStackedWidget;
class QWebEngineView;
class QWebEngineProfile;
class QAction;
class QPushButton;
class QLineEdit;
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
    void showProposal(const QString &summary,bool pending);
    void setEditorState(bool ready,bool busy,bool canUndo,bool canRedo);
    void showExportTask(const domain::ExportTask &task);
    QWebEngineView *previewView() const { return m_webView; }
signals:
    void openRequested(const QString &workspace, const QString &run, const QString &runtime);
    void newDocumentRequested(const QString &directory, const QString &name);
    void openDocumentRequested(const QString &manifest);
    void saveDocumentRequested();
    void importImageRequested(const QString &path);
    void editRequested(const QString &entityId,const QString &fieldId,const QVariant &value);
    void sourceEditRequested(const QString &path,const QString &text);
    void undoRequested();
    void redoRequested();
    void exportRequested(const QString &destination);
    void cancelExportRequested();
    void stopExportObservationRequested();
    void resumeExportRequested();
    void demoProposalRequested(const QString &request);
    void importProposalRequested(const QByteArray &json);
    void confirmProposalRequested();
    void discardProposalRequested();
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
    void sourceDialog();
    void applySelectedAsset();
    void showSelectedInspector();
    QTreeWidget *m_componentTree = nullptr;
    QTreeWidget *m_inspectorTable = nullptr;
    QLabel *m_previewPlaceholder = nullptr;
    QLabel *m_spaceSummary = nullptr;
    QPlainTextEdit *m_taskLog = nullptr;
    QStackedWidget *m_previewStack = nullptr;
    QWebEngineView *m_webView = nullptr;
    QWebEngineProfile *m_webProfile = nullptr;
    bool m_editorReady=false;
    bool m_editorBusy=false;
    QAction *m_undoAction=nullptr;
    QAction *m_redoAction=nullptr;
    QAction *m_sourceAction=nullptr;
    QPushButton *m_applyAssetButton=nullptr;
    QTreeWidget *m_assetTree = nullptr;
    QLabel *m_documentTitle = nullptr;
    QAction *m_newAction = nullptr;
    QAction *m_documentOpenAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_importAction = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_closeAction = nullptr;
    QAction *m_refreshAction = nullptr;
    QAction *m_exportAction = nullptr;
    QLabel *m_exportSummary = nullptr;
    QPushButton *m_cancelExport = nullptr;
    QPushButton *m_stopExport = nullptr;
    QPushButton *m_resumeExport = nullptr;
    bool m_hasDocument = false;
    bool m_exportActive = false;
    QLineEdit *m_proposalRequest = nullptr;
    QPlainTextEdit *m_proposalSummary = nullptr;
    QPushButton *m_generateProposal = nullptr;
    QPushButton *m_importProposal = nullptr;
    QPushButton *m_confirmProposal = nullptr;
    bool m_hasProposal = false;
    QUrl m_previewUrl;
    domain::Snapshot m_snapshot;
};
}
