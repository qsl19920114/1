#include "ui/AgentPanel.h"
#include "ui/PlanReviewPanel.h"
#include <QTest>
#include <QSignalSpy>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QJsonArray>
#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>

using namespace qvw;
namespace {
domain::AgentPlan creation() {
    QJsonArray scenes;
    for (int i = 0; i < 3; ++i) scenes.append(QJsonObject{
        {"sceneId", QString("scene-%1").arg(i + 1)}, {"assetId", QString("hash-%1").arg(i)},
        {"title", QString("标题%1").arg(i + 1)}, {"subtitle", "副标题"},
        {"color", "#123456"}, {"durationFrames", i == 1 ? 240 : (i == 0 ? 90 : 120)}, {"fontSize", 52}});
    return {{{"format", "qvw.agent-plan@1"}, {"kind", "create"}, {"summary", "<b>三段图片故事</b>"},
             {"question", ""}, {"projectName", "我的故事"}, {"scenes", scenes}, {"operations", QJsonArray{}}}, {}};
}
domain::AgentAssets assets(const QString &path = {}) {
    domain::AgentAssets result;
    for (int i = 0; i < 3; ++i) {
        domain::AgentAsset a; a.asset.hash = QString("hash-%1").arg(i);
        a.asset.originalName = QString("图片%1.png").arg(i + 1); a.asset.mime = "image/png";
        a.asset.width = 720; a.asset.height = 1280; a.localPath = path; result.append(a);
    }
    return result;
}
domain::Snapshot snapshot() {
    domain::Snapshot s; s.revision = 7; s.sourceFingerprint = "current";
    domain::Track track; domain::Clip clip; clip.id = "ending"; clip.label = "结尾";
    for (const auto &name : {QString("title"), QString("font"), QString("visible"), QString("mode")}) {
        domain::InspectorField f; f.id = name; f.label = name; f.writable = true;
        if (name == "title") { f.control = domain::ControlKind::Text; f.rawValue = "旧标题"; f.value = "旧标题"; }
        if (name == "font") { f.control = domain::ControlKind::Number; f.rawValue = "52"; f.value = "52"; }
        if (name == "visible") { f.control = domain::ControlKind::Boolean; f.rawValue = "true"; f.value = "true"; }
        if (name == "mode") { f.control = domain::ControlKind::Select; f.rawValue = 1; f.value = "1";
            f.options = {{"一", 1}, {"二", 2}}; }
        clip.inspector.append(f);
    }
    track.clips.append(clip); s.tracks.append(track); return s;
}
domain::AgentPlan editPlan() {
    auto p = creation(); p.content["kind"] = "edit"; p.content["scenes"] = QJsonArray{};
    p.content["operations"] = QJsonArray{
        QJsonObject{{"entityId", "ending"}, {"fieldId", "title"}, {"value", "新标题"}},
        QJsonObject{{"entityId", "ending"}, {"fieldId", "font"}, {"value", 60}},
        QJsonObject{{"entityId", "ending"}, {"fieldId", "visible"}, {"value", false}},
        QJsonObject{{"entityId", "ending"}, {"fieldId", "mode"}, {"value", 2}}}; return p;
}
}
class AgentPanelTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QApplication::setFont(QFont("PingFang SC")); }
    void composingAgainHidesPreviousTaskActions() {
        ui::AgentPanel panel; panel.setAvailable(true);
        domain::AgentStatus status; status.taskId = "old-task"; status.phase = "failed"; status.canRetry = true; status.canUndo = true;
        panel.showStatus(status); panel.findChild<QPushButton *>("agentContinue")->click();
        for (const auto &name : {"agentRetry", "agentRepair", "agentUndo", "agentRestore"})
            QVERIFY2(panel.findChild<QPushButton *>(name)->isHidden(), name);
    }
    void reusedGoalRevokesReviewAndWaitsForExplicitGeneration() {
        ui::AgentPanel panel; panel.setAvailable(true); panel.setSelection("ending", "结尾");
        auto *scope = panel.findChild<QCheckBox *>("agentScope"); scope->setChecked(true);
        domain::AgentStatus status; status.taskId = "old-task"; status.phase = "thinking";
        panel.showStatus(status); panel.showPublicMessage("旧方案公开输出");
        status.phase = "review"; status.canApprove = true; panel.showStatus(status); panel.showPlan(editPlan(), snapshot());
        QSignalSpy stopped(&panel, &ui::AgentPanel::stopRequested), generated(&panel, &ui::AgentPanel::generateRequested);
        QSignalSpy approved(&panel, &ui::AgentPanel::approveRequested), retry(&panel, &ui::AgentPanel::retryRequested);
        connect(&panel, &ui::AgentPanel::stopRequested, &panel, [&] { status.phase = "paused"; status.canApprove = false; panel.showStatus(status); });
        const QString goal = "完整的历史目标\n保留最后一段与留白";
        bool accepted = false;
        QVERIFY(QMetaObject::invokeMethod(&panel, "composeGoal", Q_RETURN_ARG(bool, accepted), Q_ARG(QString, goal)));
        QVERIFY(accepted); QCOMPARE(stopped.count(), 1); QCOMPARE(generated.count(), 0); QCOMPARE(approved.count(), 0); QCOMPARE(retry.count(), 0);
        QCOMPARE(panel.findChild<QPlainTextEdit *>("agentGoal")->toPlainText(), goal); QVERIFY(!scope->isChecked());
        QVERIFY(panel.findChild<QPlainTextEdit *>("agentPublicOutput")->toPlainText().isEmpty());
        QVERIFY(panel.findChild<ui::PlanReviewPanel *>("agentPlanReview")->editedPlan().isEmpty());
        auto *confirm = panel.findChild<QPushButton *>("planApprove"); QVERIFY(!confirm->isEnabled()); confirm->click(); QCOMPARE(approved.count(), 0);
        for (const auto &name : {"agentRetry", "agentRepair", "agentUndo", "agentRestore"}) QVERIFY(panel.findChild<QPushButton *>(name)->isHidden());
        panel.showStatus(status); QCOMPARE(panel.findChild<QPlainTextEdit *>("agentGoal")->toPlainText(), goal);
        panel.findChild<QPushButton *>("agentGenerate")->click(); QCOMPARE(generated.count(), 1); QCOMPARE(generated.first().first().toString(), goal);
    }
    void assetHandoffRequiresSelectionAndResetsApproval(){
        ui::AgentPanel panel;panel.setAvailable(true);QVERIFY(!panel.composeAssetGoal("图片","./assets/a.png"));panel.setSelection("actual/component","当前组件");QVERIFY(!panel.composeAssetGoal("图片","../escape.png"));domain::AgentStatus status;status.phase="review";status.canApprove=true;panel.showStatus(status);panel.showPlan(editPlan(),snapshot());
        QSignalSpy generated(&panel,&ui::AgentPanel::generateRequested),approved(&panel,&ui::AgentPanel::approveRequested),stopped(&panel,&ui::AgentPanel::stopRequested);QVERIFY(panel.composeAssetGoal("本地图片","./assets/a.png"));QCOMPARE(stopped.size(),1);QCOMPARE(generated.size(),0);QCOMPARE(approved.size(),0);QVERIFY(panel.findChild<QCheckBox*>("agentScope")->isChecked());QVERIFY(!panel.findChild<QPushButton*>("planApprove")->isEnabled());
        status.phase="restoring";panel.showStatus(status);QVERIFY(!panel.composeAssetGoal("图片","./assets/b.png"));
    }
    void composeGoalUsesTheControllersUtf8ByteLimit_data() {
        QTest::addColumn<QString>("goal");
        QTest::newRow("ascii-5000-bytes") << QString(5000, QChar('x'));
        QTest::newRow("ascii-8192-bytes") << QString(8192, QChar('x'));
        QTest::newRow("chinese-8190-bytes") << QString(2730, QChar(u'长'));
        QTest::newRow("exact-whitespace-and-newlines") << QString("  保留原始目标\n与换行  ");
    }
    void composeGoalUsesTheControllersUtf8ByteLimit() {
        QFETCH(QString, goal); ui::AgentPanel panel; panel.setAvailable(true);
        QSignalSpy generated(&panel, &ui::AgentPanel::generateRequested), approved(&panel, &ui::AgentPanel::approveRequested);
        QVERIFY(panel.composeGoal(goal)); QCOMPARE(panel.findChild<QPlainTextEdit *>("agentGoal")->toPlainText(), goal);
        QCOMPARE(generated.count(), 0); QCOMPARE(approved.count(), 0);
    }
    void reusedGoalRejectsUnavailableBusyAndInvalidInput_data() {
        QTest::addColumn<bool>("available"); QTest::addColumn<QString>("phase"); QTest::addColumn<QString>("goal");
        QTest::newRow("unavailable") << false << QString("idle") << QString("历史目标");
        for (const auto &phase : {"thinking", "applying", "creating", "undoing", "restoring"}) QTest::newRow(phase) << true << QString(phase) << QString("历史目标");
        QTest::newRow("empty") << true << QString("idle") << QString("  \n ");
        QTest::newRow("ascii-8193-bytes") << true << QString("idle") << QString(8193, QChar('x'));
        QTest::newRow("chinese-8193-bytes") << true << QString("idle") << QString(2731, QChar(u'长'));
    }
    void reusedGoalRejectsUnavailableBusyAndInvalidInput() {
        QFETCH(bool, available); QFETCH(QString, phase); QFETCH(QString, goal);
        ui::AgentPanel panel; panel.setAvailable(available); domain::AgentStatus status; status.phase = phase; panel.showStatus(status);
        auto *input = panel.findChild<QPlainTextEdit *>("agentGoal"); input->setPlainText("已有草稿");
        QSignalSpy stopped(&panel, &ui::AgentPanel::stopRequested), generated(&panel, &ui::AgentPanel::generateRequested), approved(&panel, &ui::AgentPanel::approveRequested);
        bool accepted = true;
        QVERIFY(QMetaObject::invokeMethod(&panel, "composeGoal", Q_RETURN_ARG(bool, accepted), Q_ARG(QString, goal)));
        QVERIFY(!accepted); QCOMPARE(input->toPlainText(), QString("已有草稿")); QCOMPARE(stopped.count(), 0); QCOMPARE(generated.count(), 0); QCOMPARE(approved.count(), 0);
    }
    void reviewCanReturnToGoalWithoutExecutingOrKeepingApproval() {
        ui::AgentPanel panel; panel.setAvailable(true); panel.showAssets(assets());
        auto *goal = panel.findChild<QPlainTextEdit *>("agentGoal"); goal->setPlainText("原目标");
        domain::AgentStatus status; status.phase = "review"; status.canApprove = true;
        panel.showStatus(status); panel.showPlan(editPlan(), snapshot());
        auto *revise = panel.findChild<QPushButton *>("agentRevise"); QVERIFY(revise); QVERIFY(!revise->isHidden());
        QSignalSpy stopped(&panel, &ui::AgentPanel::stopRequested), approved(&panel, &ui::AgentPanel::approveRequested);
        connect(&panel, &ui::AgentPanel::stopRequested, &panel, [&] { status.phase = "paused"; status.canApprove = false; panel.showStatus(status); });
        revise->click(); QCOMPARE(stopped.size(), 1); QCOMPARE(approved.size(), 0);
        QVERIFY(!goal->isHidden()); QVERIFY(goal->isEnabled()); QCOMPARE(goal->toPlainText(), QString("原目标"));
        QVERIFY(!panel.findChild<QPushButton *>("agentImages")->isHidden());
        auto *generate = panel.findChild<QPushButton *>("agentGenerate"); QVERIFY(!generate->isHidden()); QVERIFY(generate->isEnabled());
        auto *review = panel.findChild<ui::PlanReviewPanel *>("agentPlanReview"); QVERIFY(review->isHidden()); QVERIFY(review->editedPlan().isEmpty());
        auto *confirm = panel.findChild<QPushButton *>("planApprove"); QVERIFY(!confirm->isEnabled()); confirm->click(); QCOMPARE(approved.size(), 0);
        goal->setPlainText("新的目标"); QSignalSpy generated(&panel, &ui::AgentPanel::generateRequested); generate->click();
        QCOMPARE(generated.size(), 1); QCOMPARE(generated[0][0].toString(), QString("新的目标"));
    }
    void stagesShowOnlyRelevantActionsAndKeepRealCounts() {
        ui::AgentPanel panel; panel.setAvailable(true);
        auto *goal = panel.findChild<QPlainTextEdit *>("agentGoal");
        auto *review = panel.findChild<ui::PlanReviewPanel *>("agentPlanReview");
        auto *stop = panel.findChild<QPushButton *>("agentStop");
        QVERIFY(!goal->isHidden()); QVERIFY(review->isHidden()); QVERIFY(stop->isHidden());
        domain::AgentStatus status; status.taskId = "new-task"; status.phase = "thinking";
        panel.showStatus(status); QVERIFY(goal->isHidden()); QVERIFY(!stop->isHidden());
        QVERIFY(panel.findChild<QPushButton *>("agentGenerate")->isHidden());
        QVERIFY(QMetaObject::invokeMethod(&panel, "showPublicMessage", Q_ARG(QString, "真实公开片段 <b>不是HTML</b>")));
        auto *output = panel.findChild<QPlainTextEdit *>("agentPublicOutput"); QVERIFY(output);
        QCOMPARE(output->toPlainText(), QString("真实公开片段 <b>不是HTML</b>"));
        QVERIFY(!panel.findChild<QPushButton *>("planApprove")->isEnabled());
        status.phase = "review"; status.canApprove = true; panel.showStatus(status); panel.showPlan(editPlan(), snapshot());
        QVERIFY(!review->isHidden()); QVERIFY(stop->isHidden()); QVERIFY(goal->isHidden());
        QVERIFY(output->isHidden()); panel.findChild<QToolButton *>("agentEventsToggle")->click();
        QVERIFY(!output->isHidden()); QCOMPARE(output->toPlainText(), QString("真实公开片段 <b>不是HTML</b>"));
        panel.findChild<QToolButton *>("agentEventsToggle")->click(); QVERIFY(output->isHidden());
        status.phase = "applying"; status.canApprove = false; status.total = 4; status.completed = 1; panel.showStatus(status);
        QVERIFY(review->isHidden()); QVERIFY(!stop->isHidden());
        QVERIFY(panel.findChild<QLabel *>("agentProgress")->text().contains("1 / 4"));
        status.phase = "failed"; status.canRetry = true; panel.showStatus(status);
        QVERIFY(!panel.findChild<QPushButton *>("agentRetry")->isHidden());
        QVERIFY(!panel.findChild<QPushButton *>("agentRepair")->isHidden());
        status.phase = "complete"; status.completed = 4; panel.showStatus(status);
        QVERIFY(panel.findChild<QPushButton *>("agentRetry")->isHidden());
        auto *next = panel.findChild<QPushButton *>("agentContinue"); QVERIFY(next); QVERIFY(!next->isHidden()); next->click();
        QVERIFY(!goal->isHidden()); QVERIFY(!panel.findChild<QPushButton *>("agentGenerate")->isHidden());
        status.phase = "thinking"; status.taskId = "next-task"; panel.showStatus(status); QVERIFY(output->toPlainText().isEmpty());
        status.phase = "paused"; panel.showStatus(status);
        QVERIFY(QMetaObject::invokeMethod(&panel, "showPublicMessage", Q_ARG(QString, "late output")));
        QVERIFY(output->toPlainText().isEmpty()); QVERIFY(!panel.findChild<QPushButton *>("planApprove")->isEnabled());
    }
    void reviewSummarizesEscapedChangesAndExactImpactsBeforeEditing() {
        ui::PlanReviewPanel review; auto current = snapshot(); current.space.frameRate = 24;
        current.tracks[0].clips[0].startFrame = 48; current.tracks[0].clips[0].endFrameExclusive = 120;
        current.tracks[0].clips[0].label = "结尾 <script>";
        current.tracks[0].clips[0].inspector[0].rawValue = "旧 <b>标题</b>";
        auto plan = editPlan(); auto ops = plan.content["operations"].toArray(); auto op = ops[0].toObject(); op["value"] = "新 <img src=x>"; ops[0] = op; plan.content["operations"] = ops;
        review.showPlan(plan, current); review.setApprovalEnabled(true);
        auto *changes = review.findChild<QLabel *>("planChanges"); QVERIFY(changes);
        QCOMPARE(changes->textFormat(), Qt::RichText);
        QVERIFY(changes->text().contains("结尾 &lt;script&gt;")); QVERIFY(changes->text().contains("2–5 秒"));
        QVERIFY(changes->text().contains("48–120")); QVERIFY(changes->text().contains("旧 &lt;b&gt;标题&lt;/b&gt;"));
        QVERIFY(changes->text().contains("<b>新 &lt;img src=x&gt;</b>")); QVERIFY(!changes->text().contains("<img src=x>"));
        auto *modify = review.findChild<QPushButton *>("planModify"); QVERIFY(modify);
        QVERIFY(review.findChild<QWidget *>("planScroll")->isHidden()); modify->click();
        QVERIFY(!review.findChild<QWidget *>("planScroll")->isHidden());
        review.findChild<QLineEdit *>("operationValue_0")->setText("用户修订");
        QVERIFY(changes->text().contains("<b>用户修订</b>"));
        QVERIFY(review.findChild<QPushButton *>("planApprove")->text().contains("确认执行"));
        review.showAssets(assets()); review.showPlan(creation(), {});
        QVERIFY(changes->text().contains("标题1")); QVERIFY(changes->text().contains("0–3 秒"));
        QVERIFY(changes->text().contains("3–11 秒")); QVERIFY(changes->text().contains("11–15 秒"));
    }
    void actualSelectionChangesWithoutRestrictingRequestScope() {
        ui::AgentPanel panel; panel.setAvailable(true);
        auto *checkbox = panel.findChild<QCheckBox *>("agentScope"); QVERIFY(checkbox); QVERIFY(!checkbox->isChecked());
        QSignalSpy selected(&panel, &ui::AgentPanel::selectionChanged);
        QSignalSpy scoped(&panel, &ui::AgentPanel::scopeChanged);
        panel.setSelection("ending", "结尾");
        QCOMPARE(selected.size(), 1); QCOMPARE(selected.last().first().toString(), QString("ending"));
        QCOMPARE(scoped.size(), 0); QVERIFY(!checkbox->isChecked());
        panel.setSelection("ending", "结尾重命名");
        QCOMPARE(selected.size(), 1); QCOMPARE(scoped.size(), 0);
        QVERIFY(panel.findChild<QLabel *>("agentSelection")->text().contains("结尾重命名"));
        panel.findChild<QPlainTextEdit *>("agentGoal")->setPlainText("调整当前工程");
        panel.findChild<QPushButton *>("agentGenerate")->click();
        QCOMPARE(scoped.size(), 1); QCOMPARE(scoped.last().first().toString(), QString());
        QCOMPARE(selected.size(), 1);
        panel.setSelection({}, {});
        QCOMPARE(selected.size(), 2); QCOMPARE(selected.last().first().toString(), QString());
        QCOMPARE(scoped.size(), 1); QVERIFY(!checkbox->isChecked());
        QVERIFY(panel.findChild<QLabel *>("agentSelection")->text().contains("无"));
        panel.setSelection({}, {}); QCOMPARE(selected.size(), 2);
    }
    void multilineGoalUsesCurrentCodexAndExplicitScope() {
        ui::AgentPanel panel; panel.setAvailable(true); panel.setSelection("ending", "结尾组件");
        auto *goal = panel.findChild<QPlainTextEdit *>("agentGoal"); QVERIFY(goal);
        auto *generate = panel.findChild<QPushButton *>("agentGenerate"); QVERIFY(generate); QVERIFY(!generate->isEnabled());
        auto *scope = panel.findChild<QCheckBox *>("agentScope"); QVERIFY(scope); QVERIFY(!scope->isChecked());
        QVERIFY(panel.findChild<QLabel *>("agentModel")->text().contains("当前 Codex 登录"));
        QVERIFY(panel.findChild<QLabel *>("agentSelection")->text().contains("结尾组件"));
        QSignalSpy request(&panel, &ui::AgentPanel::generateRequested); QSignalSpy scoped(&panel, &ui::AgentPanel::scopeChanged);
        goal->setPlainText("做一段图片故事\n结尾温暖"); QVERIFY(generate->isEnabled()); generate->click();
        QCOMPARE(request.count(), 1); QCOMPARE(request.at(0).at(0).toString(), goal->toPlainText());
        scope->setChecked(true); QCOMPARE(scoped.last().at(0).toString(), QString("ending"));
        panel.setSelection("intro", "开场"); QCOMPARE(scoped.last().at(0).toString(), QString("intro"));
        scope->setChecked(false); QCOMPARE(scoped.last().at(0).toString(), QString());
    }
    void creationEditsCardsAndOrderWithIntegerFrames() {
        ui::PlanReviewPanel review; review.showAssets(assets()); review.showPlan(creation(), {}); review.setApprovalEnabled(true);
        auto *title = review.findChild<QLineEdit *>("sceneTitle_0"); QVERIFY(title); title->setText("<img src=x>文字");
        auto *image = review.findChild<QComboBox *>("sceneAsset_0"); QVERIFY(image); image->setCurrentIndex(2);
        auto *seconds = review.findChild<QDoubleSpinBox *>("sceneDuration_0"); QVERIFY(seconds);
        QCOMPARE(seconds->minimum(), 1.0); QCOMPARE(seconds->maximum(), 30.0); seconds->setValue(4.5);
        auto *font = review.findChild<QSpinBox *>("sceneFont_0"); QVERIFY(font); font->setValue(64);
        auto *color = review.findChild<QLineEdit *>("sceneColor_0"); QVERIFY(color); color->setText("#abcdef");
        auto before = review.editedPlan()["scenes"].toArray(); QCOMPARE(before[0].toObject()["durationFrames"].toInt(), 135);
        QCOMPARE(before[0].toObject()["assetId"].toString(), QString("hash-2"));
        QCOMPARE(before[0].toObject()["title"].toString(), QString("<img src=x>文字"));
        QCOMPARE(review.findChild<QLabel *>("planSummary")->textFormat(), Qt::PlainText);
        QVERIFY(review.findChild<QLabel *>("planTotalDuration")->text().contains("16.5"));
        review.findChild<QPushButton *>("sceneDown_0")->click(); QCoreApplication::processEvents();
        auto after = review.editedPlan()["scenes"].toArray(); QCOMPARE(after[1], before[0]); QCOMPARE(after[0], before[1]);
    }
    void editReviewKeepsActualFieldIdsAndPrimitiveTypes() {
        ui::PlanReviewPanel review; review.showPlan(editPlan(), snapshot()); review.setApprovalEnabled(true);
        auto *title = review.findChild<QLineEdit *>("operationValue_0"); QVERIFY(title); title->setText("用户修改");
        auto *font = review.findChild<QDoubleSpinBox *>("operationValue_1"); QVERIFY(font); font->setValue(66);
        auto *visible = review.findChild<QCheckBox *>("operationValue_2"); QVERIFY(visible); visible->setChecked(true);
        auto *mode = review.findChild<QComboBox *>("operationValue_3"); QVERIFY(mode); mode->setCurrentIndex(0);
        QVERIFY(review.findChild<QLabel *>("operationBefore_0")->text().contains("旧标题"));
        QSignalSpy approved(&review, &ui::PlanReviewPanel::approveRequested);
        auto *confirm = review.findChild<QPushButton *>("planApprove"); QVERIFY(confirm); QVERIFY(confirm->isEnabled()); confirm->click();
        QCOMPARE(approved.count(), 1); const auto p = approved.first().first().toJsonObject(); const auto ops = p["operations"].toArray();
        QCOMPARE(ops[0].toObject()["entityId"].toString(), QString("ending")); QCOMPARE(ops[0].toObject()["fieldId"].toString(), QString("title"));
        QCOMPARE(ops[0].toObject()["value"].toString(), QString("用户修改"));
        QVERIFY(ops[1].toObject()["value"].isDouble()); QCOMPARE(ops[1].toObject()["value"].toDouble(), 66.0);
        QVERIFY(ops[2].toObject()["value"].isBool()); QVERIFY(ops[2].toObject()["value"].toBool());
        QVERIFY(ops[3].toObject()["value"].isDouble()); QCOMPARE(ops[3].toObject()["value"].toInt(), 1);
        QCOMPARE(approved.first().at(1).toString(), QString());
    }
    void clarificationAndUnknownFieldsCannotBeApproved() {
        ui::PlanReviewPanel review; auto clarify = creation(); clarify.content["kind"] = "clarify";
        clarify.content["scenes"] = QJsonArray{}; clarify.content["question"] = "<b>需要多长？</b>";
        review.showPlan(clarify, {}); review.setApprovalEnabled(true);
        auto *question = review.findChild<QLabel *>("planQuestion"); QVERIFY(question);
        QCOMPARE(question->text(), QString("<b>需要多长？</b>")); QCOMPARE(question->textFormat(), Qt::PlainText);
        auto *confirm = review.findChild<QPushButton *>("planApprove"); QVERIFY(confirm); QVERIFY(confirm->isHidden());
        auto p = editPlan(); auto ops = p.content["operations"].toArray(); auto op = ops[0].toObject(); op["fieldId"] = "unknown"; ops[0] = op; p.content["operations"] = ops;
        review.showPlan(p, snapshot()); QVERIFY(!confirm->isEnabled());
    }
    void realProgressLocksMutationAndKeepsStopAvailable() {
        ui::AgentPanel panel; panel.setAvailable(true); panel.setSelection("ending", "结尾"); panel.showAssets(assets()); panel.showPlan(editPlan(), snapshot());
        auto *goal = panel.findChild<QPlainTextEdit *>("agentGoal"); QVERIFY(goal); goal->setPlainText("调整结尾");
        domain::AgentStatus s; s.phase = "applying"; s.completed = 1; s.total = 4; s.message = "正在确认第二个字段"; s.events = {"已确认第一步"};
        panel.showStatus(s); QVERIFY(!goal->isEnabled()); QVERIFY(!panel.findChild<QPushButton *>("agentImages")->isEnabled());
        QVERIFY(!panel.findChild<QLineEdit *>("operationValue_0")->isEnabled()); QVERIFY(panel.findChild<QPushButton *>("agentStop")->isEnabled());
        auto *progress = panel.findChild<QLabel *>("agentProgress"); QVERIFY(progress); QVERIFY(progress->text().contains("1 / 4")); QVERIFY(!progress->text().contains('%'));
        QVERIFY(panel.findChild<QPlainTextEdit *>("agentEvents")->toPlainText().contains("已确认第一步"));
        QSignalSpy stop(&panel, &ui::AgentPanel::stopRequested); panel.findChild<QPushButton *>("agentStop")->click(); QCOMPARE(stop.count(), 1);
        s.phase = "failed"; s.message = "第二步失败"; s.canRetry = true; s.canUndo = true; panel.showStatus(s);
        QVERIFY(goal->isEnabled()); QVERIFY(panel.findChild<QPushButton *>("agentRetry")->isEnabled());
        QCOMPARE(panel.findChild<QPushButton *>("agentUndo")->text(), QString("撤销上一步"));
        QSignalSpy retry(&panel, &ui::AgentPanel::retryRequested), repair(&panel, &ui::AgentPanel::repairRequested), undo(&panel, &ui::AgentPanel::undoRequested), restore(&panel, &ui::AgentPanel::restoreRequested);
        panel.findChild<QPushButton *>("agentRetry")->click(); panel.findChild<QPushButton *>("agentRepair")->click(); panel.findChild<QPushButton *>("agentUndo")->click(); panel.findChild<QPushButton *>("agentRestore")->click();
        QCOMPARE(retry.count(), 1); QCOMPARE(repair.count(), 1); QCOMPARE(undo.count(), 1); QCOMPARE(restore.count(), 1);
    }
    void stalePlanIsVisibleAndVersionsCompareFingerprint() {
        ui::AgentPanel panel; panel.setAvailable(true); panel.showPlan(editPlan(), snapshot());
        domain::AgentStatus s; s.phase = "stale"; s.revision = 8; s.message = "请根据现状修订"; panel.showStatus(s);
        QVERIFY(panel.findChild<QLineEdit *>("operationValue_0")); QVERIFY(!panel.findChild<QPushButton *>("planApprove")->isEnabled());
        domain::ExportTask exported; exported.phase = "complete"; exported.revision = 7; exported.sourceFingerprint = "old";
        panel.setVersions(snapshot(), exported); auto *version = panel.findChild<QLabel *>("agentVersions"); QVERIFY(version);
        QVERIFY(version->text().contains("工程 v8")); QVERIFY(version->text().contains("编译快照 v7")); QVERIFY(version->text().contains("中央可视预览等待 v7")); QVERIFY(version->text().contains("最近导出 v7")); QVERIFY(version->text().contains("过期"));
        exported.sourceFingerprint = "current"; panel.setVersions(snapshot(), exported); QVERIFY(!version->text().contains("过期"));
    }
    void assetsShowLocalThumbnailsWithoutPaths() {
        QTemporaryDir dir; QVERIFY(dir.isValid()); const auto path = dir.filePath("local.png"); QImage image(20, 30, QImage::Format_RGB32); image.fill(Qt::red); QVERIFY(image.save(path));
        ui::AgentPanel panel; panel.showAssets(assets(path)); auto *list = panel.findChild<QListWidget *>("agentAssets"); QVERIFY(list); QCOMPARE(list->count(), 3);
        QVERIFY(!list->item(0)->icon().isNull()); QVERIFY(list->item(0)->text().contains("720 × 1280")); QVERIFY(!list->item(0)->text().contains(dir.path()));
    }
    void createConfirmationEmitsEditedPlanAndExplicitNewPath() {
        QTemporaryDir dir; QVERIFY(dir.isValid()); ui::AgentPanel panel; panel.setAvailable(true); panel.showAssets(assets()); panel.showPlan(creation(), {});
        domain::AgentStatus s; s.phase = "review"; s.canApprove = true; panel.showStatus(s);
        auto *name = panel.findChild<QLineEdit *>("planProjectName"); QVERIFY(name); name->setText("修改后的故事");
        QSignalSpy approved(&panel, &ui::AgentPanel::approveRequested); bool visited = false;
        QTimer::singleShot(0, &panel, [&] {
            auto *dialog = panel.findChild<QDialog *>("agentCreateDialog"); if (!dialog) return;
            auto *parent = dialog->findChild<QLineEdit *>("createParentDirectory"); auto *folder = dialog->findChild<QLineEdit *>("createDirectoryName");
            auto *confirm = dialog->findChild<QPushButton *>("createConfirm"); if (!parent || !folder || !confirm) { dialog->reject(); return; }
            parent->setText(dir.path()); folder->setText("new-story");
            visited = dialog->findChild<QLabel *>("createActualPath")->text().contains(dir.filePath("new-story")) && dialog->findChild<QLabel *>("createExplanation")->text().contains("切换当前工程");
            confirm->click();
        });
        auto *confirm = panel.findChild<QPushButton *>("planApprove"); QVERIFY(confirm); confirm->click();
        QVERIFY(visited); QCOMPARE(approved.count(), 1); QCOMPARE(approved.first().at(0).toJsonObject()["projectName"].toString(), QString("修改后的故事"));
        QCOMPARE(approved.first().at(1).toString(), QDir(QFileInfo(dir.path()).canonicalFilePath()).filePath("new-story")); QVERIFY(!QFile::exists(dir.filePath("new-story")));
    }
    void creationRejectsExistingDestinationAndInvalidTotals() {
        QTemporaryDir dir; QVERIFY(dir.isValid()); ui::PlanReviewPanel review; review.showAssets(assets()); review.showPlan(creation(), {}); review.setApprovalEnabled(true);
        bool blocked = false; QSignalSpy approved(&review, &ui::PlanReviewPanel::approveRequested);
        QTimer::singleShot(0, &review, [&] {
            auto *dialog = review.findChild<QDialog *>("agentCreateDialog"); if (!dialog) return;
            dialog->findChild<QLineEdit *>("createParentDirectory")->setText(QFileInfo(dir.path()).absolutePath());
            dialog->findChild<QLineEdit *>("createDirectoryName")->setText(QFileInfo(dir.path()).fileName());
            blocked = !dialog->findChild<QPushButton *>("createConfirm")->isEnabled(); dialog->reject();
        });
        auto *confirm = review.findChild<QPushButton *>("planApprove"); QVERIFY(confirm); confirm->click(); QVERIFY(blocked); QCOMPARE(approved.count(), 0);
        for (int i = 0; i < 3; ++i) review.findChild<QDoubleSpinBox *>(QString("sceneDuration_%1").arg(i))->setValue(30);
        QVERIFY(!confirm->isEnabled()); QVERIFY(review.findChild<QLabel *>("planValidation")->text().contains("60"));
    }
    void unverifiedExportDoesNotLookLikeACompletedArtifact() {
        ui::AgentPanel panel; domain::ExportTask task; task.phase = "failed"; task.revision = 7; task.sourceFingerprint = "current";
        panel.setVersions(snapshot(), task); auto *version = panel.findChild<QLabel *>("agentVersions"); QVERIFY(version);
        QVERIFY(!version->text().contains("最近导出 v7")); QVERIFY(version->text().contains("未完成"));
        task.phase = "complete"; task.sourceFingerprint.clear(); panel.setVersions(snapshot(), task);
        QVERIFY(version->text().contains("待核对")); QVERIFY(!version->text().contains("过期"));
    }
    void restoredTaskDisplaysItsGoalAndActualScope() {
        ui::AgentPanel panel; panel.setAvailable(true); domain::AgentStatus s; s.taskId = "restored-task";
        s.phase = "review"; s.goal = "只调整最后一段\n保留开场"; s.scope = "ending"; panel.showStatus(s);
        auto *goal = panel.findChild<QPlainTextEdit *>("agentGoal"); QVERIFY(goal); QCOMPARE(goal->toPlainText(), s.goal);
        auto *scope = panel.findChild<QLabel *>("agentTaskScope"); QVERIFY(scope); QVERIFY(scope->text().contains("ending"));
        s.phase = "complete"; panel.showStatus(s); QVERIFY(panel.findChild<QLabel *>("agentPhase")->text().contains("已完成"));
        goal->setPlainText("下一轮改开场"); panel.showStatus(s); QCOMPARE(goal->toPlainText(), QString("下一轮改开场"));
    }
    void generatingAfterRestoreUsesVisibleScopeBeforeGoal_data() {
        QTest::addColumn<bool>("onlyCurrentComponent");
        QTest::newRow("unchecked-clears-restored-ending-scope") << false;
        QTest::newRow("checked-uses-real-intro-selection") << true;
    }
    void generatingAfterRestoreUsesVisibleScopeBeforeGoal() {
        QFETCH(bool, onlyCurrentComponent); ui::AgentPanel panel; panel.setAvailable(true);
        panel.setSelection("intro", "开场组件"); panel.showPlan(editPlan(), snapshot());
        auto *checkbox = panel.findChild<QCheckBox *>("agentScope"); QVERIFY(checkbox);
        checkbox->setChecked(onlyCurrentComponent);
        QStringList eventOrder;
        connect(&panel, &ui::AgentPanel::scopeChanged, &panel, [&](const QString &scope) { eventOrder.append("scope:" + scope); });
        connect(&panel, &ui::AgentPanel::generateRequested, &panel, [&](const QString &goal) { eventOrder.append("generate:" + goal); });
        domain::AgentStatus restored; restored.taskId = "restored-task"; restored.phase = "review";
        restored.goal = "只调整结尾"; restored.scope = "ending"; restored.canApprove = true;
        panel.showStatus(restored);
        QVERIFY(eventOrder.isEmpty()); QVERIFY(panel.findChild<QPushButton *>("planApprove")->isEnabled());
        QCOMPARE(checkbox->isChecked(), onlyCurrentComponent);
        QVERIFY(panel.findChild<QLabel *>("agentTaskScope")->text().contains("ending"));
        auto *goal = panel.findChild<QPlainTextEdit *>("agentGoal"); QVERIFY(goal); goal->setPlainText("下一轮调整画面");
        auto *generate = panel.findChild<QPushButton *>("agentGenerate"); QVERIFY(generate); QVERIFY(generate->isEnabled()); generate->click();
        const QStringList expected{"scope:" + (onlyCurrentComponent ? QString("intro") : QString()), "generate:下一轮调整画面"};
        QCOMPARE(eventOrder, expected); QVERIFY(checkbox->text().contains("下一次"));
    }
    void staleWhileCreationDialogIsOpenRevokesConfirmation() {
        QTemporaryDir dir; ui::AgentPanel panel; panel.setAvailable(true); panel.showAssets(assets()); panel.showPlan(creation(), {});
        domain::AgentStatus s; s.phase = "review"; s.canApprove = true; panel.showStatus(s);
        QSignalSpy approved(&panel, &ui::AgentPanel::approveRequested); bool visited = false;
        QTimer::singleShot(0, &panel, [&] {
            auto *dialog = panel.findChild<QDialog *>("agentCreateDialog"); if (!dialog) return; visited = true;
            dialog->findChild<QLineEdit *>("createParentDirectory")->setText(dir.path());
            dialog->findChild<QLineEdit *>("createDirectoryName")->setText("never-created");
            s.phase = "stale"; s.canApprove = false; panel.showStatus(s);
            dialog->findChild<QPushButton *>("createConfirm")->click();
        });
        panel.findChild<QPushButton *>("planApprove")->click(); QVERIFY(visited); QCOMPARE(approved.count(), 0);
    }
    void taskPanelFitsWorkbenchWidth() {
        ui::AgentPanel panel; panel.setStyleSheet("QWidget{font-size:13px;} QPushButton{padding:7px 12px;} QLineEdit,QDoubleSpinBox,QSpinBox{padding:6px;}");
        panel.setAvailable(true); panel.showAssets(assets()); panel.showPlan(creation(), {}); panel.resize(440, 900);
        domain::AgentStatus status; status.phase = "review"; status.canApprove = true; panel.showStatus(status);
        panel.show(); QCoreApplication::processEvents(); QVERIFY2(panel.width() <= 440, qPrintable(QString("minimum width %1").arg(panel.width())));
        auto *card = panel.findChild<QGroupBox *>("sceneCard_0"); QVERIFY(card); QVERIFY(card->width() < 440);
    }
    void newerSnapshotAdvancesEngineeringVersionAfterManualEdit() {
        ui::AgentPanel panel; domain::AgentStatus s; s.phase = "complete"; s.revision = 6; panel.showStatus(s);
        panel.setVersions(snapshot(), {}); auto *version = panel.findChild<QLabel *>("agentVersions"); QVERIFY(version);
        QVERIFY(version->text().contains("工程 v7")); QVERIFY(version->text().contains("编译快照 v7"));
    }
    void compiledSnapshotDoesNotConfirmVisualPreviewVersion() {
        ui::AgentPanel panel; domain::ExportTask exported; exported.phase = "complete";
        exported.revision = 6; exported.sourceFingerprint = "older-source"; panel.setVersions(snapshot(), exported);
        auto *versions = panel.findChild<QLabel *>("agentVersions"); QVERIFY(versions);
        QVERIFY(versions->text().contains("编译快照 v7"));
        QVERIFY(versions->text().contains("中央可视预览等待 v7"));
        QVERIFY(!versions->text().contains("预览确认"));
        QVERIFY(versions->text().contains("工程 v7")); QVERIFY(versions->text().contains("最近导出 v6")); QVERIFY(versions->text().contains("过期"));
    }
    void visualPreviewVersionMustMatchCurrentCompiledSnapshot() {
        ui::AgentPanel panel; auto current = snapshot(); domain::ExportTask exported;
        exported.phase = "complete"; exported.revision = 6; exported.sourceFingerprint = "older-source";
        panel.setVersions(current, exported); auto *versions = panel.findChild<QLabel *>("agentVersions"); QVERIFY(versions);
        panel.setPreviewVersion({7, "wrong-source"}); QVERIFY(!versions->text().contains("可视预览 v7"));
        panel.setPreviewVersion({current.revision, current.sourceFingerprint}); QVERIFY(versions->text().contains("中央可视预览 v7"));
        QVERIFY(versions->text().contains("编译快照 v7")); QVERIFY(versions->text().contains("最近导出 v6")); QVERIFY(versions->text().contains("过期"));
        panel.setVersions(current, exported); QVERIFY(versions->text().contains("中央可视预览 v7"));
        current.revision = 8; current.sourceFingerprint = "next-source"; panel.setVersions(current, exported);
        QVERIFY(versions->text().contains("中央可视预览等待 v8")); QVERIFY(!versions->text().contains("可视预览 v7"));
        panel.setPreviewVersion({7, "current"}); QVERIFY(versions->text().contains("中央可视预览等待 v8"));
        panel.setPreviewVersion({8, "next-source"}); QVERIFY(versions->text().contains("中央可视预览 v8"));
        panel.setPreviewVersion({}); QVERIFY(versions->text().contains("中央可视预览等待 v8"));
    }
    void idleWithoutTaskClearsPreviousReview_data() {
        QTest::addColumn<QString>("kind");
        QTest::newRow("create") << QString("create");
        QTest::newRow("edit") << QString("edit");
        QTest::newRow("clarify") << QString("clarify");
    }
    void idleWithoutTaskClearsPreviousReview() {
        QFETCH(QString, kind); ui::AgentPanel panel; panel.setAvailable(true); panel.showAssets(assets());
        auto oldPlan = kind == "edit" ? editPlan() : creation();
        if (kind == "clarify") { oldPlan.content["kind"] = kind; oldPlan.content["scenes"] = QJsonArray{}; oldPlan.content["question"] = "上个工程应保留哪段？"; }
        panel.showPlan(oldPlan, snapshot()); domain::AgentStatus oldStatus;
        oldStatus.taskId = "previous-project-task"; oldStatus.phase = kind == "clarify" ? "clarify" : "review"; oldStatus.canApprove = kind != "clarify"; panel.showStatus(oldStatus);
        auto *review = panel.findChild<ui::PlanReviewPanel *>("agentPlanReview"); QVERIFY(review); QVERIFY(!review->editedPlan().isEmpty());
        QSignalSpy approved(&panel, &ui::AgentPanel::approveRequested);
        panel.showStatus(domain::AgentStatus{});
        QVERIFY(review->editedPlan().isEmpty());
        QVERIFY(!panel.findChild<QLineEdit *>("sceneTitle_0")); QVERIFY(!panel.findChild<QLineEdit *>("operationValue_0"));
        auto *confirm = panel.findChild<QPushButton *>("planApprove"); QVERIFY(confirm); QVERIFY(confirm->isHidden()); QVERIFY(!confirm->isEnabled());
        QVERIFY(panel.findChild<QLabel *>("planQuestion")->isHidden()); QVERIFY(panel.findChild<QLabel *>("planTotalDuration")->isHidden());
        QVERIFY(!panel.findChild<QLabel *>("planSummary")->text().contains(oldPlan.content["summary"].toString()));
        confirm->click(); QCOMPARE(approved.count(), 0);
    }
    void closingTaskDismissesPendingCreationDialog() {
        QTemporaryDir dir; ui::AgentPanel panel; panel.setAvailable(true); panel.showAssets(assets()); panel.showPlan(creation(), {});
        domain::AgentStatus s; s.taskId = "previous-project-task"; s.phase = "review"; s.canApprove = true; panel.showStatus(s);
        QSignalSpy approved(&panel, &ui::AgentPanel::approveRequested); bool dismissed = false;
        QTimer::singleShot(0, &panel, [&] {
            auto *dialog = panel.findChild<QDialog *>("agentCreateDialog"); if (!dialog) return;
            dialog->findChild<QLineEdit *>("createParentDirectory")->setText(dir.path());
            dialog->findChild<QLineEdit *>("createDirectoryName")->setText("never-created");
            panel.showStatus(domain::AgentStatus{});
            dismissed = !dialog->isVisible(); dialog->reject();
        });
        panel.findChild<QPushButton *>("planApprove")->click(); QVERIFY(dismissed); QCOMPARE(approved.count(), 0);
    }
};
QTEST_MAIN(AgentPanelTest)
#include "AgentPanelTest.moc"
