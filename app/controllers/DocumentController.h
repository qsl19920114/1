#pragma once
#include "domain/Project.h"
#include "services/VideoAssetImporter.h"
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcessEnvironment>
#include <optional>
Q_DECLARE_METATYPE(qvw::domain::Project)
namespace qvw::controllers {
class DocumentController : public QObject {
    Q_OBJECT
public:
    explicit DocumentController(QObject *parent=nullptr);
    bool create(const QString &templateDirectory,const QString &destination,const QString &name);
    bool open(const QString &manifest);
    bool save();
    bool importImage(const QString &path);
    bool importFiles(const QStringList &paths);
    bool retryIncompleteImports();
    void setMediaTools(const QString &ffprobe,const QString &ffmpeg,const QProcessEnvironment &environment);
    bool importVideo(const QString &path);
    bool importingVideo() const;
    void cancelVideoImport();
    void close();
    bool hasProject() const { return m_project.has_value(); }
    domain::Project project() const { return m_project.value_or(domain::Project{}); }
signals:
    void projectLoaded(const qvw::domain::Project &project);
    void projectChanged(const qvw::domain::Project &project);
    void documentClosed();
    void failed(const QString &message);
    void message(const QString &message);
    void videoImportStateChanged(bool importing);
    void importReportReady(const QJsonObject &report);
    void importProgress(int completed, int total, const QString &path);
    void importBatchFinished(int success, int total, const QStringList &errors);
    void assetImported(const qvw::domain::Asset &asset);
private:
    void publishImportState();
    void publishImportReport();
    void clearImportRecovery();
    void setEntryState(int index,const QString &state,const QString &error={});
    QJsonArray m_batchEntries;
    QString m_recoveryRoot;
    QStringList m_incompletePaths;
    void processNextImport(quint64 generation);
    void acceptImport(const domain::Project &project, const domain::Asset &asset, bool video);
    void rejectBatchEntry(const QString &error);
    void finishBatch(bool canceled);
    void scheduleNextImport();
    bool m_batchActive = false, m_batchPending = false, m_batchVideo = false, m_announcedBusy = false;
    QStringList m_batchPaths, m_batchErrors;
    int m_batchNext = 0, m_batchCompleted = 0, m_batchSuccess = 0;
    quint64 m_batchGeneration = 0;
    std::optional<domain::Project> m_project;
    services::VideoAssetImporter m_videoImporter;
    QString m_ffprobe,m_ffmpeg;
    quint64 m_documentGeneration=0,m_importGeneration=0;
};
}
