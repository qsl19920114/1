#pragma once
#include <QWidget>
#include <QJsonArray>
class QTreeWidget;
class QPlainTextEdit;
class QPushButton;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;
namespace qvw::ui {
class HistoryPanel:public QWidget {
 Q_OBJECT
public:
 explicit HistoryPanel(QWidget *parent=nullptr);
 void showRecords(const QJsonArray &records);
 void setCurrentProject(const QString &manifest);
 void setAvailable(bool available);
signals:
 void openProjectRequested(const QString &manifest);
 void reuseGoalRequested(const QString &goal);
private:
 void refreshRecords();
 void selected();
 QJsonArray m_records;
 QString m_currentProject;
 bool m_available=false;
 QTreeWidget *m_tree;
 QPlainTextEdit *m_detail;
 QPushButton *m_open,*m_play,*m_reveal,*m_reuse;
 QLineEdit *m_search;
 QComboBox *m_kind;
 QCheckBox *m_currentOnly;
 QLabel *m_count;
};
}
