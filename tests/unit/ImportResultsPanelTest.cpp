#include "ui/ImportResultsPanel.h"
#include <QtTest>
#include <QPushButton>
#include <QTreeWidget>
#include <QJsonArray>
class ImportResultsPanelTest : public QObject {
    Q_OBJECT
private slots:
    void recoveryRequiresSameIdleAvailableProject(){
        qvw::ui::ImportResultsPanel panel;panel.setProjectRoot("/project");panel.setAvailable(true);
        QJsonObject report{{"projectRoot","/project"},{"active",true},{"completed",1},{"total",2},{"entries",QJsonArray{QJsonObject{{"path","/a.png"},{"state","imported"}},QJsonObject{{"path","/b.png"},{"state","failed"},{"error","无法读取"}}}}};
        panel.showReport(report);auto *retry=panel.findChild<QPushButton*>("retryImports");QVERIFY(retry);QVERIFY(!retry->isEnabled());report["active"]=false;panel.showReport(report);QVERIFY(retry->isEnabled());QSignalSpy requests(&panel,&qvw::ui::ImportResultsPanel::retryRequested);retry->click();QCOMPARE(requests.size(),1);
        panel.setAvailable(false);QVERIFY(!retry->isEnabled());panel.setProjectRoot("/other");panel.setAvailable(true);panel.showReport(report);QVERIFY(!retry->isEnabled());QCOMPARE(panel.findChild<QTreeWidget*>("importFiles")->topLevelItemCount(),0);
    }
};
QTEST_MAIN(ImportResultsPanelTest)
#include "ImportResultsPanelTest.moc"
