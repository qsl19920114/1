#include "ui/ImportResultsPanel.h"
#include <QLabel>
#include <QTreeWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFileInfo>
#include <QJsonArray>
namespace qvw::ui {
ImportResultsPanel::ImportResultsPanel(QWidget *parent):QWidget(parent){
    setObjectName("importResultsPanel");auto *layout=new QVBoxLayout(this);
    m_summary=new QLabel("导入素材后可在这里查看逐项结果。");m_summary->setTextFormat(Qt::PlainText);m_summary->setWordWrap(true);layout->addWidget(m_summary);
    m_progress=new QProgressBar;m_progress->setRange(0,1);m_progress->setValue(0);layout->addWidget(m_progress);
    m_files=new QTreeWidget;m_files->setObjectName("importFiles");m_files->setHeaderLabels({"文件","状态","结果与恢复提示"});m_files->setRootIsDecorated(false);m_files->header()->setSectionResizeMode(0,QHeaderView::ResizeToContents);m_files->header()->setSectionResizeMode(2,QHeaderView::Stretch);layout->addWidget(m_files,1);
    m_retry=new QPushButton("重试失败 / 继续未完成项");m_retry->setObjectName("retryImports");m_retry->setProperty("primary",true);m_retry->setToolTip("修复原文件后，重新导入本次会话中未完成的文件；已经成功的文件保留。");layout->addWidget(m_retry);
    connect(m_retry,&QPushButton::clicked,this,[this]{if(m_retry->isEnabled())emit retryRequested();});updateAction();
}
void ImportResultsPanel::setProjectRoot(const QString &root){if(m_root==root)return;m_root=root;showReport({});}
void ImportResultsPanel::setAvailable(bool available){m_available=available;updateAction();}
void ImportResultsPanel::showReport(const QJsonObject &report){
    if(!report.isEmpty()&&(m_root.isEmpty()||report["projectRoot"].toString()!=m_root))return;
    m_report=report;m_files->clear();int failed=0,pending=0;const bool active=report["active"].toBool();
    const QMap<QString,QString> states{{"imported","已导入"},{"failed","未完成"},{"working","正在校验"},{"pending",active?"等待导入":"待继续"}};
    for(const auto &v:report["entries"].toArray()){const auto entry=v.toObject();const auto path=entry["path"].toString(),state=entry["state"].toString();auto *item=new QTreeWidgetItem(m_files,{QFileInfo(path).fileName(),states.value(state,state),entry["error"].toString()});item->setToolTip(0,path);failed+=state=="failed";pending+=state=="pending"||state=="working";}
    const int total=report["total"].toInt(),completed=report["completed"].toInt();m_progress->setRange(0,qMax(1,total));m_progress->setValue(completed);
    m_summary->setText(report.isEmpty()?QStringLiteral("导入素材后可在这里查看逐项结果。"):QString("%1 · 已完成 %2 / %3 · 成功 %4 · 失败 %5 · 剩余 %6").arg(active?"正在导入":"本批结果").arg(completed).arg(total).arg(report["success"].toInt()).arg(failed).arg(pending));updateAction();
}
void ImportResultsPanel::updateAction(){bool incomplete=false;for(const auto &v:m_report["entries"].toArray())if(v.toObject()["state"]!="imported")incomplete=true;m_retry->setEnabled(m_available&&!m_report["active"].toBool()&&!m_root.isEmpty()&&m_report["projectRoot"].toString()==m_root&&incomplete);}
}
