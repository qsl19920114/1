#pragma once
#include <QWidget>
#include <QJsonArray>
class QTreeWidget;
class QPlainTextEdit;
class QPushButton;
namespace qvw::ui {
class HistoryPanel:public QWidget {
 Q_OBJECT
public:
 explicit HistoryPanel(QWidget *parent=nullptr);
 void showRecords(const QJsonArray &records);
signals:
 void openProjectRequested(const QString &manifest);
private:
 void selected();QTreeWidget *m_tree;QPlainTextEdit *m_detail;QPushButton *m_open,*m_play,*m_reveal;
};
}
