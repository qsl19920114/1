#include "ui/PlanReviewPanel.h"
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

namespace qvw::ui {
namespace {
QLabel *plainLabel(const QString &text, const QString &name) {
    auto *label = new QLabel(text); label->setObjectName(name); label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true); label->setTextInteractionFlags(Qt::TextSelectableByMouse); return label;
}
QPixmap thumbnail(const QString &path) {
    if (path.isEmpty()) return {};
    QImageReader reader(path); reader.setAutoTransform(true); const auto size = reader.size();
    if (size.isValid()) reader.setScaledSize(size.scaled(76, 60, Qt::KeepAspectRatio));
    return QPixmap::fromImage(reader.read());
}
QString wireText(const QVariant &value, const QString &fallback) {
    if (!value.isValid()) return fallback;
    const auto json = QJsonValue::fromVariant(value);
    if (json.isString()) return json.toString();
    if (json.isBool()) return json.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (json.isDouble()) return QString::number(json.toDouble(), 'g', 16);
    return fallback;
}
QString secondsText(int frames) { return QString::number(frames / 30.0, 'g', 5); }
bool isHexColor(const QString &color) {
    static const QRegularExpression pattern(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    return pattern.match(color).hasMatch();
}
}

PlanReviewPanel::PlanReviewPanel(QWidget *parent):QWidget(parent) {
    setObjectName("agentPlanReview");
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(8);
    m_summary = plainLabel(QStringLiteral("生成后，可在这里编辑并确认方案。"), "planSummary"); layout->addWidget(m_summary);
    m_question = plainLabel({}, "planQuestion"); m_question->hide(); layout->addWidget(m_question);
    m_changes = plainLabel({}, "planChanges"); m_changes->setTextFormat(Qt::RichText); m_changes->setAlignment(Qt::AlignTop);
    m_overview = new QScrollArea(this); m_overview->setObjectName("planOverview"); m_overview->setWidgetResizable(true);
    m_overview->setFrameShape(QFrame::NoFrame); m_overview->setMinimumHeight(110); m_overview->setWidget(m_changes); layout->addWidget(m_overview, 1); m_overview->hide();
    m_modify = new QPushButton(QStringLiteral("修改方案")); m_modify->setObjectName("planModify"); m_modify->hide(); layout->addWidget(m_modify);
    connect(m_modify, &QPushButton::clicked, this, [this] {
        m_modifying = !m_modifying; m_scroll->setVisible(m_modifying); m_overview->setVisible(!m_modifying);
        m_modify->setText(m_modifying ? QStringLiteral("查看修改摘要") : QStringLiteral("修改方案"));
    });
    m_scroll = new QScrollArea(this); m_scroll->setObjectName("planScroll"); m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame); m_scroll->setMinimumHeight(160); layout->addWidget(m_scroll, 1);
    m_totalDuration = plainLabel({}, "planTotalDuration"); m_totalDuration->hide(); layout->addWidget(m_totalDuration);
    m_validation = plainLabel({}, "planValidation"); m_validation->hide(); layout->addWidget(m_validation);
    m_approve = new QPushButton(QStringLiteral("确认方案"), this); m_approve->setObjectName("planApprove");
    m_approve->setProperty("primary", true); m_approve->setEnabled(false); m_approve->hide();
    layout->addWidget(m_approve); connect(m_approve, &QPushButton::clicked, this, &PlanReviewPanel::approve); m_scroll->hide();
}
void PlanReviewPanel::showPlan(const domain::AgentPlan &plan, const domain::Snapshot &snapshot) {
    m_content = plan.content; m_snapshot = snapshot; m_modifying = false; rebuild();
}
void PlanReviewPanel::clearPlan() {
    m_content = {}; m_snapshot = {}; m_approvalEnabled = false; m_modifying = false;
    if (auto *dialog = findChild<QDialog *>("agentCreateDialog")) dialog->reject();
    rebuild();
}
void PlanReviewPanel::showAssets(const domain::AgentAssets &assets) {
    m_assets = assets; if (m_content.value("kind") == "create") rebuild();
}
void PlanReviewPanel::setEditingEnabled(bool enabled) {
    m_editingEnabled = enabled; if (m_body) m_body->setEnabled(enabled); m_modify->setEnabled(enabled); updateValidation();
}
void PlanReviewPanel::setApprovalEnabled(bool enabled) { m_approvalEnabled = enabled; updateValidation(); }
QJsonObject PlanReviewPanel::editedPlan() const { return m_content; }

void PlanReviewPanel::rebuild() {
    if (auto *old = m_scroll->takeWidget()) { old->setParent(nullptr); old->deleteLater(); }
    m_body = new QWidget; auto *layout = new QVBoxLayout(m_body); layout->setContentsMargins(0, 0, 5, 0); layout->setSpacing(10);
    const auto kind = m_content.value("kind").toString();
    m_summary->setText(m_content.isEmpty() ? QStringLiteral("生成后，可在这里编辑并确认方案。") : m_content.value("summary").toString());
    m_question->setText(m_content.value("question").toString()); m_question->setVisible(kind == "clarify");
    m_approve->setVisible(kind == "create" || kind == "edit");
    m_approve->setText(kind == "create" ? QStringLiteral("确认执行 · 创建…") : QStringLiteral("确认执行修改"));
    m_modify->setVisible(kind == "create" || kind == "edit");
    m_modify->setText(m_modifying ? QStringLiteral("查看修改摘要") : QStringLiteral("修改方案"));
    m_totalDuration->setVisible(kind == "create");
    if (kind == "create") {
        auto *nameForm = new QFormLayout;
        auto *name = new QLineEdit(m_content.value("projectName").toString()); name->setObjectName("planProjectName"); name->setMaxLength(160);
        nameForm->addRow(QStringLiteral("工程名称"), name); layout->addLayout(nameForm);
        connect(name, &QLineEdit::textChanged, this, [this](const QString &value) { m_content["projectName"] = value; updateValidation(); });
        const auto scenes = m_content.value("scenes").toArray();
        for (int index = 0; index < scenes.size(); ++index) {
            const auto scene = scenes[index].toObject();
            auto *card = new QGroupBox(QStringLiteral("场景 %1 · %2").arg(index + 1).arg(scene["sceneId"].toString()));
            card->setObjectName(QString("sceneCard_%1").arg(index)); auto *cardLayout = new QVBoxLayout(card);
            auto *imageRow = new QHBoxLayout;
            auto *preview = plainLabel({}, QString("sceneThumbnail_%1").arg(index)); preview->setFixedSize(76, 60); preview->setAlignment(Qt::AlignCenter);
            auto *image = new QComboBox; image->setObjectName(QString("sceneAsset_%1").arg(index));
            image->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); image->setMinimumContentsLength(8);
            for (const auto &asset : m_assets) image->addItem(asset.asset.originalName, asset.asset.hash);
            int selected = image->findData(scene["assetId"].toString());
            if (selected < 0) { image->addItem(QStringLiteral("素材不可用"), scene["assetId"].toString()); selected = image->count() - 1; }
            image->setCurrentIndex(selected);
            const auto updateThumbnail = [this, image, preview] {
                preview->clear(); preview->setText(QStringLiteral("无缩略图"));
                for (const auto &asset : m_assets) if (asset.asset.hash == image->currentData().toString()) {
                    const auto pixmap = thumbnail(asset.localPath); if (!pixmap.isNull()) preview->setPixmap(pixmap); break;
                }
            };
            updateThumbnail(); imageRow->addWidget(preview); imageRow->addWidget(image, 1); cardLayout->addLayout(imageRow);
            connect(image, &QComboBox::currentIndexChanged, this, [this, index, image, updateThumbnail] { updateScene(index, "assetId", image->currentData().toString()); updateThumbnail(); });
            auto *form = new QFormLayout;
            auto *title = new QLineEdit(scene["title"].toString()); title->setObjectName(QString("sceneTitle_%1").arg(index)); title->setMaxLength(60);
            auto *subtitle = new QLineEdit(scene["subtitle"].toString()); subtitle->setObjectName(QString("sceneSubtitle_%1").arg(index)); subtitle->setMaxLength(160);
            auto *color = new QLineEdit(scene["color"].toString()); color->setObjectName(QString("sceneColor_%1").arg(index)); color->setMaxLength(7); color->setPlaceholderText("#RRGGBB");
            auto *font = new QSpinBox; font->setObjectName(QString("sceneFont_%1").arg(index)); font->setRange(32, 76); font->setValue(scene["fontSize"].toInt()); font->setSuffix(QStringLiteral(" px"));
            auto *duration = new QDoubleSpinBox; duration->setObjectName(QString("sceneDuration_%1").arg(index)); duration->setRange(1, 30); duration->setDecimals(3);
            duration->setSingleStep(1.0 / 30.0); duration->setValue(scene["durationFrames"].toInt() / 30.0); duration->setSuffix(QStringLiteral(" 秒"));
            form->addRow(QStringLiteral("标题"), title); form->addRow(QStringLiteral("副标题"), subtitle); form->addRow(QStringLiteral("主题色"), color);
            form->addRow(QStringLiteral("字号"), font); form->addRow(QStringLiteral("时长 · 30fps"), duration); cardLayout->addLayout(form);
            connect(title, &QLineEdit::textChanged, this, [this, index](const QString &v) { updateScene(index, "title", v); });
            connect(subtitle, &QLineEdit::textChanged, this, [this, index](const QString &v) { updateScene(index, "subtitle", v); });
            connect(color, &QLineEdit::textChanged, this, [this, index](const QString &v) { updateScene(index, "color", v); });
            connect(font, &QSpinBox::valueChanged, this, [this, index](int v) { updateScene(index, "fontSize", v); });
            connect(duration, &QDoubleSpinBox::valueChanged, this, [this, index](double v) { updateScene(index, "durationFrames", qRound(v * 30)); });
            auto *order = new QHBoxLayout; auto *up = new QPushButton(QStringLiteral("上移")); auto *down = new QPushButton(QStringLiteral("下移"));
            up->setObjectName(QString("sceneUp_%1").arg(index)); down->setObjectName(QString("sceneDown_%1").arg(index)); up->setEnabled(index > 0); down->setEnabled(index < scenes.size() - 1);
            connect(up, &QPushButton::clicked, this, [this, index] { moveScene(index, -1); }); connect(down, &QPushButton::clicked, this, [this, index] { moveScene(index, 1); });
            order->addStretch(); order->addWidget(up); order->addWidget(down); cardLayout->addLayout(order); layout->addWidget(card);
        }
    } else if (kind == "edit") {
        const auto operations = m_content.value("operations").toArray();
        for (int index = 0; index < operations.size(); ++index) {
            const auto operation = operations[index].toObject(); const auto *field = fieldFor(operation);
            auto *card = new QGroupBox; card->setObjectName(QString("operationCard_%1").arg(index)); auto *form = new QFormLayout(card);
            auto *identity = plainLabel(QStringLiteral("%1 · %2").arg(operation["entityId"].toString(), field ? field->label : operation["fieldId"].toString()), QString("operationIdentity_%1").arg(index));
            identity->setToolTip(QStringLiteral("entityId: %1\nfieldId: %2").arg(operation["entityId"].toString(), operation["fieldId"].toString())); form->addRow(identity);
            form->addRow(QStringLiteral("当前值 →"), plainLabel(field ? wireText(field->rawValue, field->value) : QStringLiteral("字段不存在"), QString("operationBefore_%1").arg(index)));
            QWidget *editor = nullptr; const auto value = operation["value"];
            if (field && field->isEditable()) switch (field->control) {
            case domain::ControlKind::Number: {
                auto *spin = new QDoubleSpinBox; spin->setRange(-std::numeric_limits<double>::max(), std::numeric_limits<double>::max()); spin->setDecimals(12); spin->setValue(value.toDouble()); editor = spin;
                connect(spin, &QDoubleSpinBox::valueChanged, this, [this, index](double v) { updateOperation(index, v); }); break;
            }
            case domain::ControlKind::Boolean: {
                auto *box = new QCheckBox(QStringLiteral("启用")); box->setChecked(value.toBool()); editor = box;
                connect(box, &QCheckBox::toggled, this, [this, index](bool v) { updateOperation(index, v); }); break;
            }
            case domain::ControlKind::Select: {
                auto *combo = new QComboBox; int selected = -1;
                for (const auto &option : field->options) { combo->addItem(option.label, option.value); if (QJsonValue::fromVariant(option.value) == value) selected = combo->count() - 1; }
                if (selected < 0) { combo->addItem(QStringLiteral("选项不可用"), value.toVariant()); selected = combo->count() - 1; }
                combo->setCurrentIndex(selected); editor = combo;
                connect(combo, &QComboBox::currentIndexChanged, this, [this, index, combo] { updateOperation(index, QJsonValue::fromVariant(combo->currentData())); }); break;
            }
            case domain::ControlKind::Text:
            case domain::ControlKind::Color: {
                auto *line = new QLineEdit(value.toString()); line->setMaxLength(8192); editor = line;
                connect(line, &QLineEdit::textChanged, this, [this, index](const QString &v) { updateOperation(index, v); }); break;
            }
            case domain::ControlKind::Unsupported: break;
            }
            if (!editor) { auto *line = new QLineEdit(wireText(value.toVariant(), {})); line->setReadOnly(true); line->setEnabled(false); editor = line; }
            editor->setObjectName(QString("operationValue_%1").arg(index)); form->addRow(QStringLiteral("拟值"), editor); layout->addWidget(card);
        }
    }
    layout->addStretch(); m_body->setEnabled(m_editingEnabled); m_scroll->setWidget(m_body); m_scroll->setVisible(m_modifying && (kind == "create" || kind == "edit"));
    m_overview->setVisible(!m_modifying && (kind == "create" || kind == "edit")); updateValidation();
}
void PlanReviewPanel::updateScene(int index, const QString &key, const QJsonValue &value) {
    auto scenes = m_content.value("scenes").toArray(); if (index < 0 || index >= scenes.size()) return;
    auto scene = scenes[index].toObject(); scene[key] = value; scenes[index] = scene; m_content["scenes"] = scenes; updateValidation();
}
void PlanReviewPanel::updateOperation(int index, const QJsonValue &value) {
    auto operations = m_content.value("operations").toArray(); if (index < 0 || index >= operations.size()) return;
    auto operation = operations[index].toObject(); operation["value"] = value; operations[index] = operation; m_content["operations"] = operations; updateValidation();
}
void PlanReviewPanel::moveScene(int index, int offset) {
    auto scenes = m_content.value("scenes").toArray(); const int target = index + offset;
    if (!m_editingEnabled || index < 0 || target < 0 || index >= scenes.size() || target >= scenes.size()) return;
    const QJsonValue scene = scenes.at(index); scenes[index] = scenes.at(target); scenes[target] = scene; m_content["scenes"] = scenes; rebuild();
}
const domain::InspectorField *PlanReviewPanel::fieldFor(const QJsonObject &operation) const {
    for (const auto &track : m_snapshot.tracks) for (const auto &clip : track.clips) if (clip.id == operation["entityId"].toString()) {
        for (const auto &field : clip.inspector) if (field.id == operation["fieldId"].toString()) return &field;
    }
    return nullptr;
}
QString PlanReviewPanel::validationError() const {
    const auto kind = m_content.value("kind").toString();
    if (kind == "create") {
        const auto name = m_content.value("projectName").toString();
        if (name.trimmed().isEmpty() || name.toUtf8().size() > 160) return QStringLiteral("请填写工程名称（最多160字节）。");
        const auto scenes = m_content.value("scenes").toArray(); if (scenes.size() != 3) return QStringLiteral("需要三个场景。"); int total = 0;
        for (const auto &item : scenes) {
            const auto scene = item.toObject(); const auto frames = scene["durationFrames"].toDouble();
            if (!std::isfinite(frames) || std::floor(frames) != frames || frames < 30 || frames > 900) return QStringLiteral("每个场景需为1–30秒的整数帧时长。");
            total += static_cast<int>(frames);
            bool found = false; for (const auto &asset : m_assets) if (asset.asset.hash == scene["assetId"].toString()) { found = true; break; }
            if (!found) return QStringLiteral("请选择已登记的图片素材。");
            const auto title = scene["title"].toString(), subtitle = scene["subtitle"].toString();
            if (title.trimmed().isEmpty() || title.size() > 60 || title.toUtf8().size() > 240 || subtitle.size() > 160 || subtitle.toUtf8().size() > 600) return QStringLiteral("请填写标题（最多60字），副标题最多160字。");
            if (!isHexColor(scene["color"].toString())) return QStringLiteral("主题色请使用 #RRGGBB。");
            const double font = scene["fontSize"].toDouble(); if (font < 32 || font > 76 || std::floor(font) != font) return QStringLiteral("字号需为32–76的整数。");
        }
        if (total > 1800 || total < 90) return QStringLiteral("总时长需为3–60秒，请调整场景时长。"); return {};
    }
    if (kind == "edit") {
        const auto operations = m_content.value("operations").toArray(); if (operations.isEmpty() || operations.size() > 24) return QStringLiteral("修改方案需包含1–24项字段操作。");
        for (const auto &item : operations) {
            const auto operation = item.toObject(); const auto *field = fieldFor(operation);
            if (!field || !field->isEditable()) return QStringLiteral("方案包含不可编辑或不存在的字段，请根据现状修订。"); const auto value = operation["value"];
            switch (field->control) {
            case domain::ControlKind::Number:
                if (!value.isDouble() || !std::isfinite(value.toDouble())) return QStringLiteral("数值字段需使用数值。"); break;
            case domain::ControlKind::Boolean:
                if (!value.isBool()) return QStringLiteral("开关字段需使用布尔值。"); break;
            case domain::ControlKind::Select: {
                bool found = false; for (const auto &option : field->options) if (QJsonValue::fromVariant(option.value) == value) { found = true; break; }
                if (!found) return QStringLiteral("请选择字段的有效选项。"); break;
            }
            case domain::ControlKind::Text:
            case domain::ControlKind::Color:
                if (!value.isString() || value.toString().toUtf8().size() > 8192) return QStringLiteral("文字字段需使用有效文本。");
                if (field->control == domain::ControlKind::Color && !QColor(value.toString()).isValid()) return QStringLiteral("请填写有效的颜色。"); break;
            case domain::ControlKind::Unsupported: return QStringLiteral("当前字段不支持编辑。");
            }
        }
        return {};
    }
    return {};
}
void PlanReviewPanel::updateOverview() {
    const auto escaped = [](const QString &text) { return text.toHtmlEscaped().replace('\n', "<br>"); };
    QStringList changes;
    const auto kind = m_content.value("kind").toString();
    if (kind == "create") {
        int start = 0;
        for (const auto &value : m_content.value("scenes").toArray()) {
            const auto scene = value.toObject(); const int end = start + scene["durationFrames"].toInt();
            QString imageName = QStringLiteral("素材不可用");
            for (const auto &asset : m_assets) if (asset.asset.hash == scene["assetId"].toString()) { imageName = asset.asset.originalName; break; }
            changes.append(QStringLiteral("<p><b>%1</b> · %2<br>%3–%4 秒 · %5–%6 帧<br>图片：%7<br>副标题：%8</p>")
                .arg(escaped(scene["title"].toString()), escaped(scene["sceneId"].toString()), secondsText(start), secondsText(end))
                .arg(start).arg(end).arg(escaped(imageName), escaped(scene["subtitle"].toString())));
            start = end;
        }
    } else if (kind == "edit") {
        for (const auto &value : m_content.value("operations").toArray()) {
            const auto operation = value.toObject(); const auto *field = fieldFor(operation);
            const domain::Clip *target = nullptr;
            for (const auto &track : m_snapshot.tracks) for (const auto &clip : track.clips)
                if (clip.id == operation["entityId"].toString()) target = &clip;
            QString affected = target ? (target->label.isEmpty() ? target->id : target->label) : operation["entityId"].toString();
            affected = escaped(affected);
            if (target) {
                if (std::isfinite(m_snapshot.space.frameRate) && m_snapshot.space.frameRate > 0)
                    affected += QStringLiteral(" · %1–%2 秒").arg(QString::number(target->startFrame / m_snapshot.space.frameRate, 'g', 8), QString::number(target->endFrameExclusive / m_snapshot.space.frameRate, 'g', 8));
                affected += QStringLiteral(" · %1–%2 帧（右端不含）").arg(target->startFrame).arg(target->endFrameExclusive);
            }
            changes.append(QStringLiteral("<p>影响：%1<br>%2：<s>%3</s> → <b>%4</b></p>").arg(affected,
                escaped(field ? field->label : operation["fieldId"].toString()),
                escaped(field ? wireText(field->rawValue, field->value) : QStringLiteral("字段不存在")),
                escaped(wireText(operation["value"].toVariant(), QStringLiteral("无效值")))));
        }
    }
    m_changes->setText(changes.join(QString()));
}
void PlanReviewPanel::updateValidation() {
    updateOverview();
    int frames = 0; for (const auto &scene : m_content.value("scenes").toArray()) frames += scene.toObject()["durationFrames"].toInt();
    m_totalDuration->setText(QStringLiteral("总时长 %1 秒 · %2 帧 · 30fps").arg(secondsText(frames)).arg(frames));
    const auto error = validationError(); m_validation->setText(error); m_validation->setVisible(!error.isEmpty()); const auto kind = m_content.value("kind").toString();
    m_approve->setEnabled(m_editingEnabled && m_approvalEnabled && error.isEmpty() && (kind == "create" || kind == "edit"));
}
void PlanReviewPanel::approve() {
    if (!m_approve->isEnabled()) return;
    if (m_content.value("kind") == "edit") { emit approveRequested(m_content, {}); return; }
    const auto reviewed = m_content;
    QDialog dialog(this); dialog.setObjectName("agentCreateDialog"); dialog.setWindowTitle(QStringLiteral("确认创建图片故事")); dialog.setMinimumWidth(460); auto *layout = new QVBoxLayout(&dialog);
    int frames = 0; for (const auto &scene : reviewed["scenes"].toArray()) frames += scene.toObject()["durationFrames"].toInt();
    layout->addWidget(plainLabel(QStringLiteral("将创建「%1」，总时长 %2 秒。创建后会切换当前工程。请选择父目录和新的文件夹名称，已有目录不会被覆写。")
        .arg(reviewed["projectName"].toString(), secondsText(frames)), "createExplanation"));
    auto *form = new QFormLayout; auto *parentRow = new QWidget; auto *parentLayout = new QHBoxLayout(parentRow); parentLayout->setContentsMargins(0, 0, 0, 0);
    auto *parent = new QLineEdit(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)); parent->setObjectName("createParentDirectory");
    auto *browse = new QPushButton(QStringLiteral("选择…")); browse->setObjectName("createBrowseParent"); parentLayout->addWidget(parent, 1); parentLayout->addWidget(browse);
    auto *folder = new QLineEdit(reviewed["projectName"].toString()); folder->setObjectName("createDirectoryName"); folder->setMaxLength(160);
    form->addRow(QStringLiteral("父目录"), parentRow); form->addRow(QStringLiteral("新文件夹"), folder); layout->addLayout(form);
    auto *actual = plainLabel({}, "createActualPath"); auto *error = plainLabel({}, "createDestinationError"); layout->addWidget(actual); layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); auto *confirm = buttons->button(QDialogButtonBox::Ok);
    confirm->setText(QStringLiteral("创建并切换工程")); confirm->setObjectName("createConfirm"); confirm->setProperty("primary", true);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消")); layout->addWidget(buttons); QString destination;
    const auto validate = [&] {
        const QFileInfo parentInfo(parent->text().trimmed()); const auto name = folder->text().trimmed(); const auto parentPath = parentInfo.canonicalFilePath();
        const bool safeName = !name.isEmpty() && name != "." && name != ".." && !name.contains('/') && !name.contains('\\') && !name.contains(QChar::Null);
        destination = QDir(parentPath.isEmpty() ? parentInfo.absoluteFilePath() : parentPath).filePath(name); actual->setText(QStringLiteral("新工程路径：%1").arg(destination)); QString problem;
        if (!parentInfo.isDir() || !QDir::isAbsolutePath(parent->text().trimmed())) problem = QStringLiteral("请选择已存在的父目录。");
        else if (!safeName) problem = QStringLiteral("请填写单个新文件夹名称。");
        else if (QFileInfo::exists(destination) || QFileInfo(destination).isSymLink()) problem = QStringLiteral("目标目录已存在，请换一个名称。不会覆写已有工程。");
        error->setText(problem); error->setVisible(!problem.isEmpty()); confirm->setEnabled(problem.isEmpty());
    };
    connect(parent, &QLineEdit::textChanged, &dialog, validate); connect(folder, &QLineEdit::textChanged, &dialog, validate);
    connect(browse, &QPushButton::clicked, &dialog, [&] { const auto selected = QFileDialog::getExistingDirectory(&dialog, QStringLiteral("选择新工程的父目录"), parent->text()); if (!selected.isEmpty()) parent->setText(selected); });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] { validate(); if (confirm->isEnabled()) dialog.accept(); });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject); validate();
    if (dialog.exec() == QDialog::Accepted && m_approve->isEnabled() && m_content == reviewed) { validate(); if (confirm->isEnabled()) emit approveRequested(reviewed, destination); }
}
}
