#include "ui/AssetTree.h"
#include <QDragEnterEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QUrl>
namespace qvw::ui {
namespace {QStringList paths(const QMimeData *data){QStringList result;if(!data->hasUrls())return result;for(const auto &url:data->urls()){if(!url.isLocalFile())return {};const auto path=url.toLocalFile();const auto ext=QFileInfo(path).suffix().toLower();if(!QStringList{"png","jpg","jpeg","mp4"}.contains(ext)||!QFileInfo(path).isFile())return {};result.append(path);if(result.size()>64)return {};}return result;}}
AssetTree::AssetTree(QWidget *parent):QTreeWidget(parent){setAcceptDrops(true);setDragDropMode(QAbstractItemView::DropOnly);setSelectionMode(QAbstractItemView::ExtendedSelection);}
void AssetTree::dragEnterEvent(QDragEnterEvent *event){if(!paths(event->mimeData()).isEmpty())event->acceptProposedAction();else event->ignore();}
void AssetTree::dragMoveEvent(QDragMoveEvent *event){if(!paths(event->mimeData()).isEmpty())event->acceptProposedAction();else event->ignore();}
void AssetTree::dropEvent(QDropEvent *event){const auto files=paths(event->mimeData());if(files.isEmpty()){event->ignore();return;}event->acceptProposedAction();emit filesDropped(files);}
}
