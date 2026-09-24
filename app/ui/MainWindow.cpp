#include "ui/MainWindow.h"
#include "ui/InspectorControls.h"
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
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
    m_openAction = bar->addAction(QStringLiteral("打开工程…"), this, &MainWindow::openProjectDialog);
    m_openAction->setShortcut(QKeySequence::Open);
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
    return withTitle(QStringLiteral("工程与组件"), m_componentTree);
}
QWidget *MainWindow::buildPreviewPanel() {
    m_previewStack = new QStackedWidget;
    m_previewPlaceholder = new QLabel(QStringLiteral("打开工程后显示 Studio 预览"));
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
    return withTitle(QStringLiteral("原生属性 · 只读查看"), m_inspectorTable);
}
QWidget *MainWindow::buildTaskPanel() {
    m_taskLog = new QPlainTextEdit; m_taskLog->setReadOnly(true); m_taskLog->setMaximumBlockCount(2000);
    return withTitle(QStringLiteral("任务与日志"), m_taskLog);
}
void MainWindow::showSnapshot(const domain::Snapshot &snapshot) {
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
            if (!first) first = item;
        }
    }
    m_componentTree->expandAll(); if (first) m_componentTree->setCurrentItem(first);
    statusBar()->showMessage(QStringLiteral("revision %1 · 后端可写字段 %2 个 · 原生面板只读，网页修改后请刷新会话")
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
        row->setText(3, field.isEditable() ? QStringLiteral("可写¹") : QStringLiteral("只读"));
        row->setToolTip(3, field.disabledReason.isEmpty()
            ? QStringLiteral("¹后端支持写入；本阶段原生面板仅展示，不提交修改。") : field.disabledReason);
        m_inspectorTable->setItemWidget(row, 2, createInspectorControl(field, m_inspectorTable));
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
