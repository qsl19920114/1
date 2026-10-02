#include "ui/MainWindow.h"
#include "ui/InspectorControls.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QInputDialog>
#include <QComboBox>
#include <QTimer>
#include <QTextDocument>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QGroupBox>
#include <QDir>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEnginePage>

namespace qvw::ui {
namespace {
QWidget *withTitle(const QString &title, QWidget *body) {
    auto *wrapper = new QWidget;
    auto *layout = new QVBoxLayout(wrapper);
    layout->setContentsMargins(8, 8, 8, 8);
    auto *caption = new QLabel(title);
    QFont font = caption->font(); font.setBold(true); caption->setFont(font);
    layout->addWidget(caption); layout->addWidget(body, 1);
    return wrapper;
}
}
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Qt 视频工作台")); resize(1480, 900);
    auto *bar = addToolBar(QStringLiteral("工程"));
    bar->setMovable(false);
    m_newAction = bar->addAction(QStringLiteral("新建工程…"), this, &MainWindow::newDocumentDialog);
    m_newAction->setShortcut(QKeySequence::New);
    m_documentOpenAction = bar->addAction(QStringLiteral("打开工程…"), this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("打开工作台工程"), {}, "工作台工程 (workbench.qvw.json)");
        if (!path.isEmpty()) emit openDocumentRequested(path);
    });
    m_documentOpenAction->setShortcut(QKeySequence::Open);
    m_saveAction = bar->addAction(QStringLiteral("保存工程"), this, &MainWindow::saveDocumentRequested);
    m_saveAction->setShortcut(QKeySequence::Save); m_saveAction->setEnabled(false);
    m_importAction = bar->addAction(QStringLiteral("导入图片…"), this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("导入 PNG/JPEG 图片"), {}, "图片 (*.png *.jpg *.jpeg);;所有文件 (*)");
        if (!path.isEmpty()) emit importImageRequested(path);
    });
    m_importAction->setEnabled(false);
    m_exportAction=bar->addAction(QStringLiteral("导出 MP4…"),this,[this] {
        const auto path=QFileDialog::getSaveFileName(this,QStringLiteral("导出验证后的 MP4"),QStringLiteral("校园社团介绍.mp4"),"MP4 (*.mp4)");
        if(!path.isEmpty())emit exportRequested(path.endsWith(".mp4",Qt::CaseInsensitive)?path:path+".mp4");
    });
    m_exportAction->setEnabled(false);
    bar->addSeparator();
    m_undoAction=bar->addAction(QStringLiteral("撤销"),this,&MainWindow::undoRequested);m_undoAction->setShortcut(QKeySequence::Undo);
    m_redoAction=bar->addAction(QStringLiteral("重做"),this,&MainWindow::redoRequested);m_redoAction->setShortcut(QKeySequence::Redo);
    m_sourceAction=bar->addAction(QStringLiteral("编辑源码…"),this,&MainWindow::sourceDialog);
    m_undoAction->setEnabled(false);m_redoAction->setEnabled(false);m_sourceAction->setEnabled(false);
    bar->addSeparator();
    m_openAction = bar->addAction(QStringLiteral("打开 Run…"), this, &MainWindow::openProjectDialog);
    m_refreshAction = bar->addAction(QStringLiteral("刷新会话"), this, &MainWindow::refreshRequested);
    m_refreshAction->setShortcut(QKeySequence::Refresh);
    m_closeAction = bar->addAction(QStringLiteral("关闭工程"), this, &MainWindow::closeRequested);
    bar->addSeparator();
    bar->addAction(QStringLiteral("选择配置…"), this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择 version-lock.json"), {}, "JSON (*.json)");
        if (!path.isEmpty()) emit configurationRequested(path);
    });
    setBackendAvailable(false);
    auto *columns = new QSplitter(Qt::Horizontal, this);
    columns->addWidget(buildProjectPanel());
    auto *middle = new QSplitter(Qt::Vertical);
    middle->addWidget(buildPreviewPanel()); middle->addWidget(buildTaskPanel());
    middle->setStretchFactor(0, 4); middle->setStretchFactor(1, 1);
    columns->addWidget(middle); columns->addWidget(buildInspectorPanel());
    columns->setSizes({220, 850, 410});
    setCentralWidget(columns);
    statusBar()->showMessage(QStringLiteral("正在检查环境…"));
    connect(m_componentTree, &QTreeWidget::currentItemChanged, this, [this] { showSelectedInspector(); });
}
void MainWindow::setBackendAvailable(bool ready) {
    m_newAction->setEnabled(ready); m_documentOpenAction->setEnabled(ready);
    m_openAction->setEnabled(ready); m_closeAction->setEnabled(ready);
    m_refreshAction->setEnabled(ready && !m_previewUrl.isEmpty());
    if (ready) statusBar()->showMessage(QStringLiteral("环境就绪，请打开一个视频工程。"));
}
MainWindow::~MainWindow() {
    // All pages must die before their profile; QObject child creation order
    // alone would otherwise destroy the earlier-created profile first.
    delete m_webView;
    m_webView = nullptr;
    delete m_webProfile;
}
QWidget *MainWindow::buildProjectPanel() {
    m_componentTree = new QTreeWidget;
    m_componentTree->setHeaderLabels({QStringLiteral("组件"), QStringLiteral("帧范围")});
    m_componentTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    auto *body = new QWidget; auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(0,0,0,0);
    m_documentTitle = new QLabel(QStringLiteral("新建标题卡工程，或打开已保存的工程。"));
    m_documentTitle->setWordWrap(true); layout->addWidget(m_documentTitle);
    layout->addWidget(m_componentTree, 2);
    layout->addWidget(new QLabel(QStringLiteral("图片素材 · 双击复制相对路径")));
    m_assetTree = new QTreeWidget;
    m_assetTree->setHeaderLabels({QStringLiteral("素材"),QStringLiteral("尺寸")});
    m_assetTree->setRootIsDecorated(false);
    m_assetTree->header()->setSectionResizeMode(0,QHeaderView::Stretch);
    connect(m_assetTree,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *item) {
        QApplication::clipboard()->setText(item->data(0,Qt::UserRole).toString());
        statusBar()->showMessage(QStringLiteral("已复制图片路径；也可点击“应用到选中组件的图片”。"));
    });
    layout->addWidget(m_assetTree,1);
    m_applyAssetButton=new QPushButton(QStringLiteral("应用到选中组件的图片"));m_applyAssetButton->setEnabled(false);
    connect(m_applyAssetButton,&QPushButton::clicked,this,&MainWindow::applySelectedAsset);
    layout->addWidget(m_applyAssetButton);
    return withTitle(QStringLiteral("工程与素材"),body);
}
QWidget *MainWindow::buildPreviewPanel() {
    m_previewStack = new QStackedWidget;
    m_previewPlaceholder = new QLabel(QStringLiteral("新建标题卡工程或打开工程后显示预览"));
    m_previewPlaceholder->setAlignment(Qt::AlignCenter); m_previewPlaceholder->setMinimumHeight(280);
    m_previewStack->addWidget(m_previewPlaceholder);
    auto *body = new QWidget; auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0); layout->addWidget(m_previewStack, 1);
    m_spaceSummary = new QLabel(QStringLiteral("画幅：未加载")); layout->addWidget(m_spaceSummary);
    return withTitle(QStringLiteral("Studio 预览"), body);
}
QWidget *MainWindow::buildInspectorPanel() {
    m_inspectorTable = new QTreeWidget;
    m_inspectorTable->setHeaderLabels({QStringLiteral("属性"), QStringLiteral("类型"), QStringLiteral("当前值"), QStringLiteral("状态")});
    m_inspectorTable->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_inspectorTable->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_inspectorTable->setColumnWidth(1, 55); m_inspectorTable->setColumnWidth(3, 65);
    m_inspectorTable->setRootIsDecorated(false);
    auto *body=new QWidget;auto *layout=new QVBoxLayout(body);layout->setContentsMargins(0,0,0,0);
    layout->addWidget(m_inspectorTable,3);
    auto *proposals=new QGroupBox(QStringLiteral("编辑提案 · 本地模拟"));auto *form=new QVBoxLayout(proposals);
    auto *scope=new QLabel(QStringLiteral("模拟支持：标题改为…、主题色改为#RRGGBB、图片使用第1张。确认后才修改；未连接真实模型。"));
    scope->setWordWrap(true);scope->setTextFormat(Qt::PlainText);form->addWidget(scope);
    m_proposalRequest=new QLineEdit;m_proposalRequest->setMaxLength(4096);
    m_proposalRequest->setPlaceholderText(QStringLiteral("例如：标题改为校园摄影社"));form->addWidget(m_proposalRequest);
    auto *generate=new QHBoxLayout;m_generateProposal=new QPushButton(QStringLiteral("生成模拟提案"));m_importProposal=new QPushButton(QStringLiteral("导入提案 JSON…"));
    m_generateProposal->setEnabled(false);m_importProposal->setEnabled(false);generate->addWidget(m_generateProposal);generate->addWidget(m_importProposal);form->addLayout(generate);
    m_proposalSummary=new QPlainTextEdit;m_proposalSummary->setReadOnly(true);m_proposalSummary->setMaximumHeight(160);
    m_proposalSummary->setPlainText(QStringLiteral("尚无待确认提案。"));form->addWidget(m_proposalSummary);
    auto *actions=new QHBoxLayout;m_confirmProposal=new QPushButton(QStringLiteral("确认修改"));m_confirmProposal->setEnabled(false);
    auto *discard=new QPushButton(QStringLiteral("放弃提案"));actions->addWidget(m_confirmProposal);actions->addWidget(discard);form->addLayout(actions);
    connect(m_generateProposal,&QPushButton::clicked,this,[this]{emit demoProposalRequested(m_proposalRequest->text());});
    connect(m_importProposal,&QPushButton::clicked,this,[this]{
        const auto path=QFileDialog::getOpenFileName(this,QStringLiteral("导入外部提案（来源未核验）"),{},"JSON (*.json)");
        if(path.isEmpty())return;QFile file(path);
        if(!QFileInfo(path).isFile()||!file.open(QIODevice::ReadOnly)||file.size()>65536){appendLog(QStringLiteral("提案文件无法读取或超过64KiB。"));return;}
        const auto data=file.read(65537);if(data.size()>65536){appendLog(QStringLiteral("提案超过64KiB。"));return;}emit importProposalRequested(data);
    });
    connect(m_confirmProposal,&QPushButton::clicked,this,&MainWindow::confirmProposalRequested);
    connect(discard,&QPushButton::clicked,this,&MainWindow::discardProposalRequested);
    layout->addWidget(proposals,2);return withTitle(QStringLiteral("原生属性与提案"),body);
}
void MainWindow::showProposal(const QString &summary,bool pending) {
    m_hasProposal=pending;m_proposalSummary->setPlainText(summary.isEmpty()?QStringLiteral("尚无待确认提案。"):summary);
    m_confirmProposal->setEnabled(pending&&m_hasDocument&&m_editorReady&&!m_editorBusy);
}
QWidget *MainWindow::buildTaskPanel() {
    m_taskLog = new QPlainTextEdit; m_taskLog->setReadOnly(true); m_taskLog->setMaximumBlockCount(2000);
    auto *body=new QWidget;auto *layout=new QVBoxLayout(body);layout->setContentsMargins(0,0,0,0);
    m_exportSummary=new QLabel(QStringLiteral("尚无导出任务。编辑完成后点击“导出 MP4…”。"));m_exportSummary->setWordWrap(true);
    layout->addWidget(m_exportSummary);
    auto *buttons=new QHBoxLayout;
    m_cancelExport=new QPushButton(QStringLiteral("取消构建"));m_stopExport=new QPushButton(QStringLiteral("停止观察"));m_resumeExport=new QPushButton(QStringLiteral("恢复观察"));
    for(auto *button:{m_cancelExport,m_stopExport,m_resumeExport}){button->setEnabled(false);buttons->addWidget(button);}
    buttons->addStretch();layout->addLayout(buttons);layout->addWidget(m_taskLog,1);
    connect(m_cancelExport,&QPushButton::clicked,this,&MainWindow::cancelExportRequested);
    connect(m_stopExport,&QPushButton::clicked,this,&MainWindow::stopExportObservationRequested);
    connect(m_resumeExport,&QPushButton::clicked,this,&MainWindow::resumeExportRequested);
    return withTitle(QStringLiteral("任务与日志"), body);
}
void MainWindow::showExportTask(const domain::ExportTask &task) {
    const bool observing=QStringList{"planning","submitting","working","cancelling","getting","validating"}.contains(task.phase);
    m_exportActive=task.active||observing;
    const QMap<QString,QString> labels{{"idle","尚无任务"},{"planning","检查构建计划"},{"submitting","提交构建"},{"working","正在构建"},{"cancelling","等待取消终态"},{"stopped","已停止观察"},{"getting","获取成片"},{"validating","校验视频与全片解码"},{"complete","导出及校验通过"},{"failed","任务失败"},{"cancelled","构建已取消"}};
    QString summary=labels.value(task.phase,task.phase);
    if(!task.buildId.isEmpty())summary+=QStringLiteral(" · %1 · 源码 revision %2").arg(task.buildId).arg(task.revision);
    if(!task.destination.isEmpty())summary+=QStringLiteral("\n%1").arg(task.destination);
    if(!task.error.isEmpty())summary+=QStringLiteral("\n%1").arg(task.error);
    m_exportSummary->setText(summary);
    m_cancelExport->setEnabled(task.active&&!task.buildId.isEmpty()&&task.phase!="cancelling");
    m_stopExport->setEnabled(observing);
    m_resumeExport->setEnabled(task.phase=="stopped"&&(task.active||!task.buildId.isEmpty()));
    m_exportAction->setEnabled(m_hasDocument&&m_editorReady&&!m_editorBusy&&!m_exportActive);
}
void MainWindow::showSnapshot(const domain::Snapshot &snapshot) {
    QString selectedId;
    if(const auto *item=m_componentTree->currentItem())selectedId=item->toolTip(0);
    m_snapshot = snapshot; m_componentTree->clear(); m_inspectorTable->clear();
    if (!snapshot.isLoaded()) {
        m_spaceSummary->setText(QStringLiteral("画幅：未加载"));
        statusBar()->showMessage(QStringLiteral("未加载工程")); return;
    }
    m_spaceSummary->setText(QStringLiteral("画幅：%1 · Source：%2").arg(snapshot.space.describe(), snapshot.sourcePath));
    QTreeWidgetItem *first = nullptr;
    for (int t = 0; t < snapshot.tracks.size(); ++t) {
        const auto &track = snapshot.tracks[t];
        auto *trackItem = new QTreeWidgetItem(m_componentTree);
        trackItem->setText(0, track.label.isEmpty() ? track.id : track.label);
        for (int c = 0; c < track.clips.size(); ++c) {
            const auto &clip = track.clips[c];
            auto *item = new QTreeWidgetItem(trackItem);
            item->setText(0, clip.label.isEmpty() ? clip.id : clip.label);
            item->setToolTip(0, clip.id);
            item->setText(1, QStringLiteral("%1–%2").arg(clip.startFrame).arg(clip.endFrameExclusive));
            item->setData(0, Qt::UserRole, t); item->setData(0, Qt::UserRole + 1, c);
            if (!first || clip.id==selectedId) first = item;
        }
    }
    m_sourceAction->setEnabled(m_editorReady&&!m_editorBusy&&!snapshot.sourceFiles.isEmpty());
    m_componentTree->expandAll(); if (first) m_componentTree->setCurrentItem(first);
    statusBar()->showMessage(QStringLiteral("revision %1 · 可写字段 %2 个 · 修改经后端确认后记录历史")
        .arg(snapshot.revision).arg(snapshot.writableFieldCount()));
}
void MainWindow::showSelectedInspector() {
    m_inspectorTable->clear();
    const auto *item = m_componentTree->currentItem();
    if (!item || !item->data(0, Qt::UserRole).isValid()) return;
    const int t = item->data(0, Qt::UserRole).toInt(), c = item->data(0, Qt::UserRole + 1).toInt();
    if (t < 0 || t >= m_snapshot.tracks.size() || c < 0 || c >= m_snapshot.tracks[t].clips.size()) return;
    for (const auto &field : m_snapshot.tracks[t].clips[c].inspector) {
        auto *row = new QTreeWidgetItem(m_inspectorTable);
        row->setText(0, field.label.isEmpty() ? field.id : field.label);
        row->setText(1, domain::controlKindLabel(field.control));
        row->setText(3, field.isEditable() ? QStringLiteral("可编辑") : QStringLiteral("只读"));
        row->setToolTip(3, field.disabledReason.isEmpty()
            ? QStringLiteral("确认成功后才记录历史；外部修改会要求刷新。") : field.disabledReason);
        const auto entity=m_snapshot.tracks[t].clips[c].id;
        std::function<void(const QVariant &)> commit;
        if(m_editorReady)commit=[this,entity,id=field.id](const QVariant &value){QTimer::singleShot(0,this,[this,entity,id,value]{emit editRequested(entity,id,value);});};
        m_inspectorTable->setItemWidget(row, 2, createInspectorControl(field, m_inspectorTable,commit));
    }
}
void MainWindow::showPreview(const QUrl &url) {
    m_previewUrl = url;
    if (!m_webView) {
        m_webView = new QWebEngineView;
        // An off-the-record profile avoids writing browser state into the user's profile.
        m_webProfile = new QWebEngineProfile(this);
        m_webView->setPage(new QWebEnginePage(m_webProfile, m_webView));
        m_previewStack->addWidget(m_webView);
        connect(m_webView, &QWebEngineView::loadFinished, this, [this](bool ok) {
            if (m_previewUrl.isEmpty()) return;
            appendLog(ok ? QStringLiteral("Studio 页面加载完成。") : QStringLiteral("Studio 页面加载失败。"));
            emit previewLoaded(ok);
        });
    }
    m_previewStack->setCurrentWidget(m_webView);
    m_refreshAction->setEnabled(true); m_webView->load(url);
}
void MainWindow::clearProject() {
    m_previewUrl = QUrl(); m_refreshAction->setEnabled(false);
    if (m_webView) { m_webView->stop(); m_webView->setUrl(QUrl("about:blank")); }
    m_previewStack->setCurrentWidget(m_previewPlaceholder); showSnapshot({});
}
void MainWindow::showError(const QString &error) {
    showSnapshot({}); appendLog(QStringLiteral("错误：%1").arg(error)); statusBar()->showMessage(error);
}
void MainWindow::appendLog(const QString &line) { m_taskLog->appendPlainText(line); }
void MainWindow::setEditorState(bool ready,bool busy,bool canUndo,bool canRedo) {
    const bool readinessChanged=m_editorReady!=ready;
    m_editorReady=ready;m_editorBusy=busy;
    m_generateProposal->setEnabled(m_hasDocument&&ready&&!busy);
    m_importProposal->setEnabled(m_hasDocument&&ready&&!busy);
    m_confirmProposal->setEnabled(m_hasProposal&&m_hasDocument&&ready&&!busy);
    m_exportAction->setEnabled(m_hasDocument&&ready&&!busy&&!m_exportActive);
    m_inspectorTable->setEnabled(ready&&!busy);
    m_undoAction->setEnabled(ready&&!busy&&canUndo);m_redoAction->setEnabled(ready&&!busy&&canRedo);
    m_sourceAction->setEnabled(ready&&!busy&&!m_snapshot.sourceFiles.isEmpty());
    m_applyAssetButton->setEnabled(ready&&!busy);
    m_refreshAction->setEnabled(!busy&&!m_previewUrl.isEmpty());
    if(readinessChanged)showSelectedInspector();
    if(busy)statusBar()->showMessage(QStringLiteral("正在校验并提交修改…"));
}
void MainWindow::applySelectedAsset() {
    if(!m_editorReady||m_editorBusy)return;
    const auto *asset=m_assetTree->currentItem(),*item=m_componentTree->currentItem();
    if(!asset||!item||!item->data(0,Qt::UserRole).isValid()) {appendLog(QStringLiteral("请先选择图片素材和组件。"));return;}
    const int t=item->data(0,Qt::UserRole).toInt(),c=item->data(0,Qt::UserRole+1).toInt();
    if(t<0||t>=m_snapshot.tracks.size()||c<0||c>=m_snapshot.tracks[t].clips.size())return;
    const auto &clip=m_snapshot.tracks[t].clips[c];
    for(const auto &field:clip.inspector)if(field.binding=="image"&&field.isEditable()) {
        emit editRequested(clip.id,field.id,asset->data(0,Qt::UserRole));return;
    }
    appendLog(QStringLiteral("选中组件没有可写的图片字段。"));
}
void MainWindow::sourceDialog() {
    if(!m_editorReady||m_editorBusy||m_snapshot.sourceFiles.isEmpty())return;
    const auto files=m_snapshot.sourceFiles;
    QDialog dialog(this);dialog.setWindowTitle(QStringLiteral("编辑当前工程源码"));dialog.resize(850,600);
    auto *layout=new QVBoxLayout(&dialog);auto *paths=new QComboBox;paths->addItems(files.keys());
    paths->setCurrentText(m_snapshot.sourcePath);auto *text=new QPlainTextEdit;
    text->setPlainText(files.value(paths->currentText()));
    layout->addWidget(new QLabel(QStringLiteral("提交后检查编译；失败时尝试恢复本次预像，检测到外部改动则停止恢复。")));
    layout->addWidget(paths);layout->addWidget(text,1);
    connect(text->document(),&QTextDocument::modificationChanged,paths,[paths](bool modified){paths->setEnabled(!modified);});
    paths->setToolTip(QStringLiteral("每次提交一个文件；编辑后可取消本次操作再选择其他文件。"));
    connect(paths,&QComboBox::currentTextChanged,&dialog,[&](const QString &path){text->setPlainText(files.value(path));});
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted&&text->toPlainText()!=files.value(paths->currentText()))
        emit sourceEditRequested(paths->currentText(),text->toPlainText());
}
void MainWindow::showDocument(const domain::Project &project) {
    m_hasDocument=true;
    m_generateProposal->setEnabled(m_editorReady&&!m_editorBusy);m_importProposal->setEnabled(m_editorReady&&!m_editorBusy);
    m_exportAction->setEnabled(m_editorReady&&!m_editorBusy&&!m_exportActive);
    setWindowTitle(QStringLiteral("%1 — Qt 视频工作台").arg(project.name));
    m_documentTitle->setText(QStringLiteral("%1\n模板：%2").arg(project.name,project.templateId));
    m_documentTitle->setToolTip(project.rootPath); m_assetTree->clear();
    for (const auto &asset : project.assets) {
        auto *item = new QTreeWidgetItem(m_assetTree);
        item->setText(0,asset.originalName);
        item->setText(1,QStringLiteral("%1×%2").arg(asset.width).arg(asset.height));
        item->setToolTip(0,QStringLiteral("已导入；点击应用到选中组件后才修改画面。\n%1").arg(asset.path));
        item->setData(0,Qt::UserRole,"./"+asset.path);
    }
    m_saveAction->setEnabled(true); m_importAction->setEnabled(true);
}
void MainWindow::clearDocument() {
    m_hasDocument=false;m_exportAction->setEnabled(false);
    m_generateProposal->setEnabled(false);m_importProposal->setEnabled(false);showProposal({},false);
    setWindowTitle(QStringLiteral("Qt 视频工作台"));
    m_documentTitle->setText(QStringLiteral("新建标题卡工程，或打开已保存的工程。"));
    m_assetTree->clear(); m_saveAction->setEnabled(false); m_importAction->setEnabled(false);
}
void MainWindow::newDocumentDialog() {
    bool ok = false;
    const auto name = QInputDialog::getText(this,QStringLiteral("新建图片标题卡"),QStringLiteral("工程名称"),QLineEdit::Normal,QStringLiteral("校园社团介绍"),&ok).trimmed();
    if (!ok || name.isEmpty()) return;
    const auto parent = QFileDialog::getExistingDirectory(this,QStringLiteral("选择保存位置，将创建同名子目录"));
    if (parent.isEmpty()) return;
    if (name=="." || name==".." || name.contains('/') || name.contains('\\') || name.contains(':')) {
        appendLog(QStringLiteral("工程名称不能包含路径分隔符。")); return;
    }
    emit newDocumentRequested(QDir(parent).filePath(name),name);
}
void MainWindow::openProjectDialog() {
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("打开视频工程")); dialog.resize(680, 240);
    auto *layout = new QVBoxLayout(&dialog); auto *form = new QFormLayout;
    auto *run = new QLineEdit; auto *workspace = new QLineEdit; auto *runtime = new QLineEdit;
    auto addPath = [&](const QString &label, QLineEdit *edit, auto browse) {
        auto *row = new QWidget; auto *horizontal = new QHBoxLayout(row); horizontal->setContentsMargins(0,0,0,0);
        auto *button = new QPushButton(QStringLiteral("选择…")); horizontal->addWidget(edit,1); horizontal->addWidget(button);
        connect(button, &QPushButton::clicked, &dialog, browse); form->addRow(label,row);
    };
    addPath(QStringLiteral("Run 文件"), run, [&] {
        const auto path = QFileDialog::getOpenFileName(&dialog, QStringLiteral("选择 .svrun 文件"), run->text(), "Hypit Run (*.svrun)");
        if (path.isEmpty()) return;
        run->setText(path);
        if (workspace->text().isEmpty()) workspace->setText(QFileInfo(path).absolutePath());
        const QString suggested = QDir(workspace->text()).filePath("hypit.runtime.json");
        if (runtime->text().isEmpty() && QFileInfo::exists(suggested)) runtime->setText(suggested);
    });
    addPath(QStringLiteral("工程目录"), workspace, [&] {
        const auto path = QFileDialog::getExistingDirectory(&dialog, QStringLiteral("选择工程目录"), workspace->text());
        if (!path.isEmpty()) workspace->setText(path);
    });
    addPath(QStringLiteral("Runtime 配置"), runtime, [&] {
        const auto path = QFileDialog::getOpenFileName(&dialog, QStringLiteral("选择本地 Runtime 配置"), workspace->text(), "JSON (*.json)");
        if (!path.isEmpty()) runtime->setText(path);
    });
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) emit openRequested(workspace->text(), run->text(), runtime->text());
}
}
