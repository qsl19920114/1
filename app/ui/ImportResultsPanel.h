#pragma once
#include <QWidget>
#include <QJsonObject>
class QLabel;
class QTreeWidget;
class QProgressBar;
class QPushButton;
namespace qvw::ui {
class ImportResultsPanel : public QWidget {
    Q_OBJECT
public:
    explicit ImportResultsPanel(QWidget *parent=nullptr);
    void setProjectRoot(const QString &root);
    void setAvailable(bool available);
    void showReport(const QJsonObject &report);
signals:
    void retryRequested();
private:
    void updateAction();
    QString m_root;
    QJsonObject m_report;
    bool m_available=false;
    QLabel *m_summary;
    QProgressBar *m_progress;
    QTreeWidget *m_files;
    QPushButton *m_retry;
};
}
