#pragma once
#include "domain/Snapshot.h"
#include "domain/Project.h"
#include "domain/ExportTask.h"
#include "domain/VideoSample.h"
#include "domain/AgentPlan.h"
#include <QMainWindow>
#include <QUrl>
#include <QJsonObject>
#include <QJsonArray>
class QLabel;
class QPlainTextEdit;
class QTreeWidget;
class QStackedWidget;
class QWebEngineView;
class QWebEngineProfile;
class QAction;
class QPushButton;
class QLineEdit;
class QTabWidget;
class QSlider;
class QProgressBar;
class QComboBox;
class QSplitter;
class QMenu;
namespace qvw::ui {
class HistoryPanel;
class ImportResultsPanel;
class SceneStrip;
class StudioTransport;
class AgentPanel;
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
    void setImportBusy(bool busy);
    void showImportProgress(int completed,int total,const QString &path);
    void selectImportedAsset(const domain::Asset &asset);
    void setSamples(const domain::VideoSamples &samples);
    QWebEngineView *previewView() const { return m_webView; }
    AgentPanel *agentPanel() const { return m_agentPanel; }
    domain::PreviewVersion previewVersion() const;
    void showAgentStatus(const domain::AgentStatus &status);
    void showAgentPlan(const domain::AgentPlan &plan);
    QJsonObject workspaceLayout() const;
    void restoreWorkspaceLayout(const QJsonObject &layout);
    void restorePlayhead(int frame);
    void showRecentProjects(const QJsonArray &recent);
    void showHistory(const QJsonArray &history);
    void showImportReport(const QJsonObject &report);
signals:
    void historyRecorded(const QJsonObject &record);
    void playheadChanged(const QString &manifest,int frame);
    void openRequested(const QString &workspace, const QString &run, const QString &runtime);
    void newDocumentRequested(const QString &directory, const QString &name);
    void openDocumentRequested(const QString &manifest);
    void saveDocumentRequested();
    void importFilesRequested(const QStringList &paths);
    void importImageRequested(const QString &path);
    void importVideoRequested(const QString &path);
    void cancelImportRequested();
    void retryImportRequested();
    void newTemplateDocumentRequested(const QString &templateId,const QString &directory,const QString &name);
    void sampleDocumentRequested(const QString &video,const QString &directory,const QString &name);
    void editRequested(const QString &entityId,const QString &fieldId,const QVariant &value);
    void sourceEditRequested(const QString &path,const QString &text);
    void undoRequested();
    void redoRequested();
    void exportRequested(const QString &destination);
    void cancelExportRequested();
    void stopExportObservationRequested();
    void resumeExportRequested();
    void clearExportCacheRequested();
    void demoProposalRequested(const QString &request);
    void importProposalRequested(const QByteArray &json);
    void confirmProposalRequested();
    void discardProposalRequested();
    void closeRequested();
    void refreshRequested();
    void configurationRequested(const QString &path);
    void previewLoaded(bool ok);
    void previewVersionChanged(const qvw::domain::PreviewVersion &version);
private:
    QWidget *buildProjectPanel();
    QWidget *buildPreviewPanel();
    QWidget *buildInspectorPanel();
    QWidget *buildTaskPanel();
    void openProjectDialog();
    void newDocumentDialog(const QString &samplePath={});
    void updateActions();
    QString compatibleAssetField() const;
    void useSelectedSample();
    void sourceDialog();
    void applySelectedAsset();
    void previewSelectedAsset();
    QPushButton *m_previewAsset=nullptr;
    QLabel *m_assetSelection=nullptr;
    QLineEdit *m_assetSearch=nullptr;
    QComboBox *m_assetType=nullptr;
    QLabel *m_assetCount=nullptr;
    void filterAssets();
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
    QPushButton *m_clearExportCache = nullptr;
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
    domain::Project m_document;
    HistoryPanel *m_history=nullptr;
    ImportResultsPanel *m_importResults=nullptr;
    bool m_importReportActive=false;
    QTabWidget *m_inspectorTabs=nullptr;
    QSplitter *m_columns=nullptr,*m_middle=nullptr;
    QMenu *m_recentMenu=nullptr;
    int m_pendingFrame=-1;
    SceneStrip *m_scenes=nullptr;
    void selectScene(const QString &entityId,int frame);
    domain::VideoSamples m_samples;
    domain::ExportTask m_exportTask;
    StudioTransport *m_transport=nullptr;
    AgentPanel *m_agentPanel=nullptr;
    bool m_agentBusy=false;
    bool m_agentRestoring=false;
    QLabel *m_backendStatus=nullptr;
    QLabel *m_transportTime=nullptr;
    QSlider *m_seekSlider=nullptr;
    QProgressBar *m_exportProgress=nullptr;
    QTabWidget *m_libraryTabs=nullptr;
    QTabWidget *m_taskTabs=nullptr;
    QTreeWidget *m_sampleTree=nullptr;
    QWidget *m_leftPanel=nullptr;
    QWidget *m_rightPanel=nullptr;
    QWidget *m_welcome=nullptr;
    QPushButton *m_welcomeNew=nullptr;
    QPushButton *m_welcomeOpen=nullptr;
    QPushButton *m_sampleUse=nullptr;
    QPushButton *m_cancelImport=nullptr;
    QPushButton *m_samplePreview=nullptr;
    QPushButton *m_play=nullptr;
    QPushButton *m_previous=nullptr;
    QPushButton *m_next=nullptr;
    QPushButton *m_discardProposal=nullptr;
    QPushButton *m_playFilm=nullptr;
    QPushButton *m_revealFilm=nullptr;
    QAction *m_configAction=nullptr;
    QString m_backendError;
    bool m_backendReady=false;
    bool m_importBusy=false;
    bool m_canUndo=false,m_canRedo=false;
    quint64 m_editGeneration=0;
};
}
