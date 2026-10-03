#include "agent/AgentController.h"
#include <QTest>
#include <QImage>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QFile>
#include "../support/AgentProtocolFixture.h"
class AgentControllerTest : public QObject {
    Q_OBJECT
private slots:
    void publicOutputNeverAuthorizesAnIncompleteOrInvalidProposal() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        QVERIFY(f.useFakeModel(QJsonObject{{"kind", "invalid"}}, true));
        QSignalSpy output(f.agent.get(), SIGNAL(publicMessageReceived(QString))); QVERIFY(output.isValid());
        f.agent->generate("调整标题"); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(f.temp.filePath("model-started")), 5000);
        QVERIFY(QMetaObject::invokeMethod(&f.model, "publicMessageReceived", Q_ARG(QString, "真实公开内容")));
        QCOMPARE(output.size(), 1); QCOMPARE(output[0][0].toString(), QString("真实公开内容"));
        QCOMPARE(f.agent->status().phase, QString("thinking")); QVERIFY(!f.agent->status().canApprove); QVERIFY(!f.agent->plan());
        f.agent->approve(); QCOMPARE(f.http.writes, 0);
        QVERIFY(agent_test::writeFile(f.temp.filePath("model-release"), "release"));
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("failed"), 5000);
        QVERIFY(!f.agent->status().canApprove); QVERIFY(!f.agent->plan()); QCOMPARE(f.http.writes, 0);
        QVERIFY(QMetaObject::invokeMethod(&f.model, "publicMessageReceived", Q_ARG(QString, "late content"))); QCOMPARE(output.size(), 1);
    }
    void stoppingReviewRevokesApprovalAndAllowsAnotherGoal() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "old proposal")})));
        f.agent->generate("原目标"); QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        QVERIFY(f.agent->status().canApprove); f.agent->stop(); QVERIFY(!f.agent->status().canApprove);
        f.agent->approve(); f.agent->retry(); QCOMPARE(f.http.writes, 0);
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "new proposal")})));
        f.agent->generate("新的目标"); QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        QCOMPARE(f.agent->status().goal, QString("新的目标")); QCOMPARE(f.http.writes, 0);
        f.agent->approve(); QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("complete"), 5000);
        QCOMPARE(f.http.writes, 1); QCOMPARE(f.http.value("title"), QString("new proposal"));
    }
    void cancellationDiscardsFurtherPublicMessagesAndApproval() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "pending")}), true));
        QSignalSpy output(f.agent.get(), SIGNAL(publicMessageReceived(QString))); QVERIFY(output.isValid());
        f.agent->generate("调整标题"); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(f.temp.filePath("model-started")), 5000);
        f.agent->stop();
        QVERIFY(QMetaObject::invokeMethod(&f.model, "publicMessageReceived", Q_ARG(QString, "late content")));
        QCOMPARE(output.size(), 0); QVERIFY(!f.agent->status().canApprove); QVERIFY(!f.agent->plan()); QCOMPARE(f.http.writes, 0);
    }
    void navigationDuringApplyPreservesApprovedScopeForEveryStep() {
        agent_test::AgentProtocolFixture f;QVERIFY(f.initialize());f.agent->setScope("scene-1");
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title","accepted"),agent_test::operation("subtitle","also accepted")})));
        f.http.onWrite=[&]{if(f.http.writes==1){f.agent->setSelection("scene-2");f.agent->setScope("scene-2");}};
        f.agent->generate("修改这个组件");QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase,QString("review"),5000);f.agent->approve();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase,QString("complete"),5000);
        QCOMPARE(f.http.writes,2);QCOMPARE(f.agent->status().scope,QString("scene-1"));
        QCOMPARE(f.savedState()["scope"].toString(),QString("scene-1"));
        QCOMPARE(f.savedState()["executionBase"].toObject()["scope"].toString(),QString("scene-1"));
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title","next selected","scene-2")})));
        f.agent->generate("修改新选中的组件");QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase,QString("review"),5000);
        QCOMPARE(f.agent->plan()->base.scope,QString("scene-2"));QCOMPARE(f.agent->status().scope,QString("scene-2"));
    }
    void scopeChangedWhileThinkingDoesNotWidenTheReturnedProposal() {
        agent_test::AgentProtocolFixture f;QVERIFY(f.initialize());f.agent->setScope("scene-1");
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title","approved original scope")}),true));
        f.agent->generate("修改这个组件");QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(f.temp.filePath("model-started")),5000);
        f.agent->setScope("scene-2");QCOMPARE(f.agent->status().scope,QString("scene-1"));
        QVERIFY(agent_test::writeFile(f.temp.filePath("model-release"),"release"));
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase,QString("review"),5000);f.agent->approve();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase,QString("complete"),5000);
        QCOMPARE(f.http.value("title"),QString("approved original scope"));
        QCOMPARE(f.savedState()["executionBase"].toObject()["scope"].toString(),QString("scene-1"));
    }
    void creatingFromExistingScopedProjectRecordsNewRootBeforeCompilation() {
        agent_test::AgentProtocolFixture f;QVERIFY(f.initialize());QImage image(32,32,QImage::Format_RGB32);image.fill(Qt::red);
        const auto imagePath=f.temp.filePath("photo.png");QVERIFY(image.save(imagePath));QVERIFY(f.agent->setImages({imagePath}));
        f.agent->setScope("scene-1");const auto asset=f.agent->assets().first().asset.hash;QJsonArray scenes;
        for(int i=0;i<3;++i)scenes.append(QJsonObject{{"sceneId",QString("scene-%1").arg(i+1)},{"assetId",asset},{"title","校园"},{"subtitle","活动"},{"color","#35bca8"},{"durationFrames",150},{"fontSize",54}});
        auto plan=agent_test::editPlan({});plan["kind"]="create";plan["projectName"]="新作品";plan["scenes"]=scenes;
        QVERIFY(f.useFakeModel(plan));f.agent->generate("创建新作品");QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase,QString("review"),5000);
        const auto destination=f.temp.filePath("new-project");f.agent->approve(destination);
        const auto canonicalDestination=QFileInfo(destination).canonicalFilePath();
        QCOMPARE(f.agent->status().phase,QString("creating"));QCOMPARE(f.document.project().rootPath,canonicalDestination);
        QCOMPARE(f.savedState()["executionBase"].toObject()["root"].toString(),canonicalDestination);
        QVERIFY(f.savedState()["executionBase"].toObject()["scope"].toString().isEmpty());QVERIFY(f.agent->status().scope.isEmpty());
        QCOMPARE(f.savedState()["executionBase"].toObject()["revision"].toInt(),-1);
        f.agent->backendFailed("协议模拟：新工程编译失败");
        QCOMPARE(f.savedState()["phase"].toString(),QString("failed"));
        QCOMPARE(f.savedState()["lastResult"].toString(),QString("协议模拟：新工程编译失败"));
    }
    void actualSelectionIsContextEvenWhenApprovalScopeIsWholeProject() {
        agent_test::AgentProtocolFixture f;QVERIFY(f.initialize());
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title","selected", "scene-2")})));
        f.agent->setSelection("scene-2");f.agent->setScope({});f.agent->generate("改当前选中组件的标题");
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase,QString("review"),5000);
        const auto prompt=agent_test::readFile(f.temp.filePath("model-prompt.txt"));
        QVERIFY(prompt.contains("\"selectedEntity\":\"scene-2\""));
        QVERIFY(prompt.contains("\"editScope\":\"\""));QVERIFY(f.agent->plan()->base.scope.isEmpty());QCOMPARE(f.http.writes,0);
    }
    void failedStepFactsSurviveRestoreAndReachRepairModel() {
        agent_test::AgentProtocolFixture f;QVERIFY(f.initialize());f.http.writeStatuses={{1,422}};
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title","rejected")})));
        f.agent->generate("修改标题");QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase,QString("review"),5000);f.agent->approve();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase,QString("failed"),5000);
        const auto actualError=f.agent->status().message;QVERIFY(!actualError.isEmpty());
        QCOMPARE(f.savedState()["lastResult"].toString(),actualError);
        QFile events(QDir(f.document.project().rootPath).filePath(".workbench/agent/events.jsonl"));QVERIFY(events.open(QIODevice::ReadOnly));
        bool found=false;for(const auto &line:events.readAll().split('\n')){const auto event=QJsonDocument::fromJson(line).object();if(event["outcome"]=="failed"){found=true;QCOMPARE(event["error"].toString(),actualError);QCOMPARE(event["operationIndex"].toInt(-1),0);QCOMPARE(event["entityId"].toString(),QString("scene-1"));}}
        QVERIFY(found);f.agent.reset();f.agent=std::make_unique<qvw::agent::AgentController>(f.document,f.editor,f.exporter,f.model,QStringLiteral(QVW_SOURCE_DIR)+"/templates");
        f.agent->restore();QCOMPARE(f.agent->status().phase,QString("review"));f.agent->repair();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase,QString("review"),5000);
        QVERIFY(agent_test::readFile(f.temp.filePath("model-prompt.txt")).contains(actualError.toUtf8()));QCOMPARE(f.http.writes,1);
    }
    void initTestCase() {
        qInfo("AgentController tests use a fake model executable and local HTTP protocol simulation; they do not claim real Codex or Hypit verification.");
    }
    void restoredScopeRequiresNewApprovalAndRejectsOtherComponent() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        const QJsonArray operations{agent_test::operation("title", "first retained"),
                                    agent_test::operation("subtitle", "remaining")};
        QVERIFY(f.saveTask(f.storedTask(operations, 1, "scene-1")));
        f.agent->restore();
        QCOMPARE(f.agent->status().phase, QString("review"));
        QCOMPARE(f.agent->status().scope, QString("scene-1"));
        QVERIFY(f.agent->plan());
        QCOMPARE(f.agent->plan()->base.scope, QString("scene-1"));
        QCOMPARE(f.agent->plan()->content["operations"].toArray().size(), 1);
        QVERIFY(f.agent->status().canApprove);
        QTest::qWait(100); QCOMPARE(f.http.writes, 0);
        auto widened = f.agent->plan()->content;
        widened["operations"] = QJsonArray{agent_test::operation("title", "forbidden", "scene-2")};
        QSignalSpy errors(f.agent.get(), &qvw::agent::AgentController::failed);
        f.agent->updatePlan(widened);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(f.agent->plan()->content["operations"].toArray().first().toObject()["entityId"].toString(), QString("scene-1"));
        QCOMPARE(f.http.writes, 0);
    }
    void staleRestoreNeverRebasesSavedFingerprintAcrossRepeatedRestore() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        const auto original = f.storedTask(QJsonArray{agent_test::operation("title", "outdated")}, 0, "scene-1");
        QVERIFY(f.saveTask(original));
        f.http.sourceSalt = "manual source change"; ++f.http.revision;
        f.editor.acceptSnapshot(f.http.snapshot());
        for (int attempt = 0; attempt < 2; ++attempt) {
            f.agent->restore();
            QCOMPARE(f.agent->status().phase, QString("stale"));
            QVERIFY(!f.agent->status().canApprove);
            QCOMPARE(f.savedState()["executionBase"].toObject()["fingerprint"], original["executionBase"].toObject()["fingerprint"]);
            f.agent->approve();
            QCOMPARE(f.http.writes, 0);
        }
    }
    void damagedTaskJsonReportsFailureAndIsNotOverwritten() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        QVERIFY(f.saveTask(f.storedTask(QJsonArray{agent_test::operation("title", "pending")})));
        const QByteArray damaged = "{\"format\": \"qvw.agent-task@1\", broken json";
        QVERIFY(agent_test::writeFile(f.statePath(), damaged));
        QSignalSpy errors(f.agent.get(), &qvw::agent::AgentController::failed);
        f.agent->restore();
        QCOMPARE(f.agent->status().phase, QString("failed"));
        QVERIFY(!f.agent->status().message.isEmpty());
        QCOMPARE(errors.size(), 1);
        QVERIFY(!f.agent->status().canApprove);
        QCOMPARE(agent_test::readFile(f.statePath()), damaged);
        f.agent->restore();
        QCOMPARE(agent_test::readFile(f.statePath()), damaged);
        QCOMPARE(f.http.writes, 0);
    }
    void secondStep422RetainsPrefixAndRetryOnlyWritesFailedStep() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        f.http.writeStatuses[2] = 422;
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "accepted"),
                                                             agent_test::operation("subtitle", "recovered")})));
        f.agent->generate("修改标题和副标题");
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        f.agent->approve();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("failed"), 5000);
        QCOMPARE(f.agent->status().completed, 1);
        QCOMPARE(f.agent->status().total, 2);
        QVERIFY(f.agent->status().canRetry);
        f.agent->setSelection("scene-2");f.agent->setScope("scene-2");
        QVERIFY(f.agent->status().scope.isEmpty()); // This task was approved for the whole project.
        QVERIFY(f.savedState()["scope"].toString().isEmpty());
        QCOMPARE(f.http.value("title"), QString("accepted"));
        QCOMPARE(f.http.value("subtitle"), QString("old subtitle"));
        QCOMPARE(f.savedState()["next"].toInt(), 1);
        QCOMPARE(f.savedState()["executionBase"].toObject()["fingerprint"].toString(), QString::fromLatin1(f.editor.snapshot().sourceFingerprint.toHex()));
        f.agent->retry();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("complete"), 5000);
        QCOMPARE(f.http.writtenFields, QStringList({"title", "subtitle", "subtitle"}));
        QCOMPARE(f.http.value("title"), QString("accepted"));
        QCOMPARE(f.http.value("subtitle"), QString("recovered"));
        QCOMPARE(f.agent->status().completed, 2);
    }
    void retriesStopAfterTwoFailuresWithoutReplayingSuccessfulPrefix() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        f.http.writeStatuses = {{2, 422}, {3, 422}, {4, 422}};
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "accepted"),
                                                             agent_test::operation("subtitle", "rejected")})));
        f.agent->generate("两个修改");
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        f.agent->approve();
        QTRY_COMPARE_WITH_TIMEOUT(f.http.writes, 2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("failed"), 5000);
        for (int retry = 0; retry < 2; ++retry) {
            QVERIFY(f.agent->status().canRetry);
            f.agent->retry();
            QTRY_COMPARE_WITH_TIMEOUT(f.http.writes, retry + 3, 5000);
            QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("failed"), 5000);
            QCOMPARE(f.agent->status().completed, 1);
        }
        QVERIFY(!f.agent->status().canRetry);
        QCOMPARE(f.savedState()["retries"].toInt(), 2);
        f.agent->retry(); QTest::qWait(100);
        QCOMPARE(f.http.writtenFields, QStringList({"title", "subtitle", "subtitle", "subtitle"}));
        QCOMPARE(f.http.value("title"), QString("accepted"));
        QCOMPARE(f.http.value("subtitle"), QString("old subtitle"));
    }
    void stopWhileWriteIsAcceptedConfirmsPrefixAndPreventsNextStep() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "accepted"),
                                                             agent_test::operation("subtitle", "never written")})));
        f.http.onWrite = [&] { if (f.http.writes == 1) f.agent->stop(); };
        f.agent->generate("两个修改");
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        f.agent->approve();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().completed, 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("paused"), 5000);
        QTest::qWait(450);
        QCOMPARE(f.http.writes, 1);
        QCOMPARE(f.http.value("title"), QString("accepted"));
        QCOMPARE(f.http.value("subtitle"), QString("old subtitle"));
        QCOMPARE(f.savedState()["next"].toInt(), 1);
    }
    void stopStillSettlesAcceptedWriteBeforeRetryingOnlyRemainingStep() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        f.http.lateRepublish = true;
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "accepted"),
                                                             agent_test::operation("subtitle", "remaining")})));
        f.http.onWrite = [&] { if (f.http.writes == 1) f.agent->stop(); };
        f.agent->generate("两个修改");
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        f.agent->approve();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().completed, 1, 5000);
        QVERIFY(!f.agent->status().canRetry);
        QSignalSpy errors(f.agent.get(), &qvw::agent::AgentController::failed);
        f.agent->retry();
        QCOMPARE(errors.size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("paused"), 5000);
        QCOMPARE(f.editor.snapshot().revision, f.http.revision);
        QCOMPARE(f.http.revision, 3);
        QCOMPARE(f.http.writes, 1);
        QVERIFY(f.agent->status().canRetry);
        f.agent->retry();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("complete"), 5000);
        QCOMPARE(f.http.writtenFields, QStringList({"title", "subtitle"}));
        QCOMPARE(f.http.value("title"), QString("accepted"));
        QCOMPARE(f.http.value("subtitle"), QString("remaining"));
    }
    void manualChangeWhileModelIsThinkingInvalidatesItsFutureAnswer() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "stale answer")}), true));
        f.agent->generate("改变标题");
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(f.temp.filePath("model-started")), 5000);
        QCOMPARE(f.agent->status().phase, QString("thinking"));
        f.http.sourceSalt = "manual change while thinking"; ++f.http.revision;
        f.editor.acceptSnapshot(f.http.snapshot());
        QCOMPARE(f.agent->status().phase, QString("stale"));
        QVERIFY(agent_test::writeFile(f.temp.filePath("model-release"), "release"));
        QTest::qWait(200);
        QVERIFY(!f.agent->status().canApprove);
        QVERIFY(!f.agent->plan());
        QCOMPARE(f.http.writes, 0);
    }
    void manualChangeAfterReviewPreventsApproval() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "stale review")})));
        f.agent->generate("改变标题");
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        f.http.sourceSalt = "manual change after review"; ++f.http.revision;
        f.editor.acceptSnapshot(f.http.snapshot());
        QCOMPARE(f.agent->status().phase, QString("stale"));
        QVERIFY(!f.agent->status().canApprove);
        f.agent->approve(); QTest::qWait(100);
        QCOMPARE(f.http.writes, 0);
    }
    void lateRevisionOnlyRepublishContinuesAndPreservesSharedUndo() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        f.http.lateRepublish = true;
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "accepted title"),
                                                             agent_test::operation("subtitle", "accepted subtitle")})));
        f.agent->generate("两个修改");
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        f.agent->approve();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("complete"), 5000);
        QCOMPARE(f.http.writes, 2);
        QCOMPARE(f.editor.snapshot().revision, f.http.revision);
        QCOMPARE(f.http.revision, 5); // Two writes, two late same-source republishes.
        QVERIFY(f.agent->status().canUndo);
        f.http.lateRepublish = false;
        f.agent->undoLast();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("paused"), 5000);
        QCOMPARE(f.http.value("title"), QString("accepted title"));
        QCOMPARE(f.http.value("subtitle"), QString("old subtitle"));
        QSignalSpy confirmed(&f.editor, &qvw::controllers::EditorController::operationSucceeded);
        f.editor.undo();
        QTRY_COMPARE_WITH_TIMEOUT(confirmed.size(), 1, 5000);
        QCOMPARE(f.http.value("title"), QString("old title"));
    }
    void externalSourceDuringCompileSettleStopsRemainingWrites() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        f.http.lateRepublish = true; f.http.externalOnRepublish = true;
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "accepted"),
                                                             agent_test::operation("subtitle", "forbidden after external change")})));
        f.agent->generate("两个修改");
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        f.agent->approve();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("stale"), 5000);
        QCOMPARE(f.agent->status().completed, 1);
        QVERIFY(!f.agent->status().canApprove);
        QVERIFY(!f.agent->status().canRetry);
        QTest::qWait(200);
        QCOMPARE(f.http.writes, 1);
        QCOMPARE(f.http.value("subtitle"), QString("old subtitle"));
        f.agent->retry(); QCOMPARE(f.http.writes, 1);
    }
    void undoSettleRejectsAnotherAgentActionUntilSourceVersionIsConfirmed() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        f.http.lateRepublish = true;
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "accepted"),
                                                             agent_test::operation("subtitle", "accepted subtitle")})));
        f.agent->generate("两个修改");
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        f.agent->approve();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("complete"), 5000);
        QSignalSpy confirmed(&f.editor, &qvw::controllers::EditorController::operationSucceeded);
        QSignalSpy errors(f.agent.get(), &qvw::agent::AgentController::failed);
        const auto task = f.agent->status().taskId;
        f.agent->undoLast();
        QTRY_COMPARE_WITH_TIMEOUT(confirmed.size(), 1, 5000);
        QCOMPARE(f.agent->status().phase, QString("undoing"));
        f.agent->generate("不应开始的新目标");
        QCOMPARE(errors.size(), 1);
        QCOMPARE(f.agent->status().taskId, task);
        f.agent->undoLast();
        QCOMPARE(errors.size(), 2); // No second write while undo is settling.
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("paused"), 5000);
        QCOMPARE(f.http.writes, 3);
        QCOMPARE(f.http.value("title"), QString("accepted"));
        QCOMPARE(f.http.value("subtitle"), QString("old subtitle"));
    }
    void undoConfirmsLateRevisionBeforeSharedRedoSucceeds() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        f.http.lateRepublish = true;
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "accepted"),
                                                             agent_test::operation("subtitle", "accepted subtitle")})));
        f.agent->generate("两个修改");
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        f.agent->approve();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("complete"), 5000);
        f.agent->undoLast();
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("paused"), 5000);
        QCOMPARE(f.editor.snapshot().revision, f.http.revision);
        QCOMPARE(f.http.revision, 7); // Undo's own late revision was also read.
        QSignalSpy confirmed(&f.editor, &qvw::controllers::EditorController::operationSucceeded);
        f.editor.redo();
        QTRY_COMPARE_WITH_TIMEOUT(confirmed.size(), 1, 5000);
        QCOMPARE(f.http.value("title"), QString("accepted"));
        QCOMPARE(f.http.value("subtitle"), QString("accepted subtitle"));
    }
    void newTaskUsesCurrentBaselineRatherThanPreviousReviewBaseline() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "first proposal")})));
        f.agent->generate("第一目标");
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        const auto previous = f.agent->status().taskId;
        f.http.sourceSalt = "new manual version"; ++f.http.revision;
        f.editor.acceptSnapshot(f.http.snapshot());
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "new proposal")})));
        f.agent->generate("第二目标");
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        QVERIFY(f.agent->status().taskId != previous);
        QCOMPARE(f.savedState()["executionBase"].toObject(), f.baseline());
        QCOMPARE(f.agent->plan()->base.fingerprint, f.editor.snapshot().sourceFingerprint);
    }
    void switchingProjectsDoesNotPersistPreviousTaskInNewDocument() {
        agent_test::AgentProtocolFixture f; QVERIFY(f.initialize());
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "old project")})));
        f.agent->generate("旧工程任务");
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        const auto previousPath = f.statePath();
        const auto previousBytes = agent_test::readFile(previousPath);
        QVERIFY(f.document.create(QStringLiteral(QVW_SOURCE_DIR) + "/templates/title-card", f.temp.filePath("new-project"), "新工程"));
        f.attachDocument();
        QTest::qWait(100);
        QVERIFY(!QFileInfo::exists(f.statePath()));
        QCOMPARE(agent_test::readFile(previousPath), previousBytes);
        QVERIFY(f.agent->status().taskId.isEmpty());
        QVERIFY(f.useFakeModel(agent_test::editPlan(QJsonArray{agent_test::operation("title", "new project")})));
        f.agent->generate("新工程任务");
        QTRY_COMPARE_WITH_TIMEOUT(f.agent->status().phase, QString("review"), 5000);
        QCOMPARE(f.savedState()["executionBase"].toObject(), f.baseline());
        QCOMPARE(f.savedState()["projectId"].toString(), f.document.project().rootPath);
        QCOMPARE(agent_test::readFile(previousPath), previousBytes);
    }
    void modelProposalLeavesDocumentUntouchedUntilApproval() {
        QTemporaryDir temp;qvw::infra::LogWriter log(temp.filePath("log.jsonl"));
        qvw::controllers::DocumentController doc;qvw::controllers::EditorController editor;qvw::controllers::ExportController exporter({},log);
        qvw::agent::ModelClient model;qvw::agent::AgentController agent(doc,editor,exporter,model,QStringLiteral(QVW_SOURCE_DIR)+"/templates");
        QStringList paths;for(int i=0;i<3;i++){QImage image(64,64,QImage::Format_RGB32);image.fill(QColor(i==0?"red":i==1?"green":"blue"));auto path=temp.filePath(QString("图片%1.png").arg(i));QVERIFY(image.save(path));paths.append(path);}
        QVERIFY(agent.setImages(paths));QCOMPARE(agent.assets().size(),3);
        QJsonArray scenes;for(int i=0;i<3;i++)scenes.append(QJsonObject{{"sceneId",QString("scene-%1").arg(i+1)},{"assetId",agent.assets()[i].asset.hash},{"title","校园创作社"},{"subtitle","欢迎加入"},{"color","#35bca8"},{"durationFrames",i==0?90:i==1?240:120},{"fontSize",54}});
        QJsonObject json{{"format","qvw.agent-plan@1"},{"kind","create"},{"summary","拟创建三段故事"},{"question",""},{"projectName","校园故事"},{"scenes",scenes},{"operations",QJsonArray{}}};
        QFile response(temp.filePath("response.json"));QVERIFY(response.open(QIODevice::WriteOnly));response.write(QJsonDocument(json).toJson());response.close();
        const auto quoted=QJsonDocument(QJsonArray{response.fileName()}).toJson(QJsonDocument::Compact);const auto literal=quoted.mid(1,quoted.size()-2);
        QFile cli(temp.filePath("fake-codex"));QVERIFY(cli.open(QIODevice::WriteOnly));cli.write("#!/usr/bin/python3\nimport sys,pathlib\na=sys.argv\npathlib.Path(a[a.index('--output-last-message')+1]).write_bytes(pathlib.Path("+literal+").read_bytes())\n");cli.close();QVERIFY(cli.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));model.setProgram(cli.fileName());
        QSignalSpy ready(&agent,&qvw::agent::AgentController::planReady);agent.generate("三张照片做15秒招新短片");
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(),1,5000);QVERIFY(!doc.hasProject());QCOMPARE(agent.status().phase,QString("review"));QVERIFY(agent.status().canApprove);
        json["approved"]=true;agent.updatePlan(json);QCOMPARE(agent.status().phase,QString("review"));QVERIFY(!doc.hasProject());
        agent.stop();QVERIFY(!agent.status().canApprove);
    }
};
QTEST_GUILESS_MAIN(AgentControllerTest)
#include "AgentControllerTest.moc"
