#include "ui/HistoryPanel.h"
#include "ui/MediaPlayerDialog.h"
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFileInfo>
#include <QDesktopServices>
#include <QJsonObject>
#include <QHeaderView>
namespace qvw::ui {
HistoryPanel::HistoryPanel(QWidget *parent):QWidget(parent){
 setObjectName("historyPanel");auto *layout=new QVBoxLayout(this);auto *splitter=new QSplitter;
 m_tree=new QTreeWidget;m_tree->setObjectName("historyRecords");m_tree->setHeaderLabels({"任务 / 修改 / 成片","工程","时间"});m_tree->setRootIsDecorated(false);m_tree->header()->setSectionResizeMode(0,QHeaderView::Stretch);splitter->addWidget(m_tree);
 m_detail=new QPlainTextEdit;m_detail->setObjectName("historyDetail");m_detail->setReadOnly(true);m_detail->setPlaceholderText("选择记录，查看目标、执行结果与修改详情。历史记录不会自动执行。");splitter->addWidget(m_detail);layout->addWidget(splitter);
 auto *buttons=new QHBoxLayout;m_open=new QPushButton("打开所属工程");m_play=new QPushButton("播放成片");m_reveal=new QPushButton("打开成片文件夹");for(auto *button:{m_open,m_play,m_reveal}){button->setEnabled(false);buttons->addWidget(button);}buttons->addStretch();layout->addLayout(buttons);
 connect(m_tree,&QTreeWidget::currentItemChanged,this,[this]{selected();});
 connect(m_open,&QPushButton::clicked,this,[this]{if(auto *item=m_tree->currentItem()){const auto path=item->data(0,Qt::UserRole).toJsonObject()["project"].toString();if(QFileInfo(path).isFile())emit openProjectRequested(path);}});
 connect(m_play,&QPushButton::clicked,this,[this]{if(auto *item=m_tree->currentItem()){const auto path=item->data(0,Qt::UserRole).toJsonObject()["artifact"].toString();if(QFileInfo(path).isFile())(new MediaPlayerDialog(path,"历史成片",this))->show();}});
 connect(m_reveal,&QPushButton::clicked,this,[this]{if(auto *item=m_tree->currentItem()){const auto path=item->data(0,Qt::UserRole).toJsonObject()["artifact"].toString();if(QFileInfo(path).isFile())QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));}});
}
void HistoryPanel::showRecords(const QJsonArray &records){QString selectedId;if(auto *item=m_tree->currentItem())selectedId=item->data(0,Qt::UserRole).toJsonObject()["id"].toString();m_tree->clear();QTreeWidgetItem *selection=nullptr;
 for(const auto &v:records){const auto record=v.toObject();auto *item=new QTreeWidgetItem(m_tree,{record["title"].toString(),record["projectName"].toString(),record["time"].toString()});item->setData(0,Qt::UserRole,record);if(record["id"]==selectedId)selection=item;}
 if(!selection&&m_tree->topLevelItemCount())selection=m_tree->topLevelItem(0);m_tree->setCurrentItem(selection);selected();
}
void HistoryPanel::selected(){auto *item=m_tree->currentItem();const auto record=item?item->data(0,Qt::UserRole).toJsonObject():QJsonObject{};
 m_detail->setPlainText(record["detail"].toString()+ (record.isEmpty()?QString():"\n\n工程："+record["project"].toString()));
 const auto project=record["project"].toString(),artifact=record["artifact"].toString();m_open->setEnabled(QFileInfo(project).isFile());m_play->setEnabled(!artifact.isEmpty()&&QFileInfo(artifact).isFile());m_reveal->setEnabled(m_play->isEnabled());if(!artifact.isEmpty()&&!QFileInfo(artifact).isFile())m_detail->appendPlainText("\n成片文件已移动或不存在："+artifact);
}
}
