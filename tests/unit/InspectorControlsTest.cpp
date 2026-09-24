#include "ui/InspectorControls.h"
#include <QTest>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <memory>
class InspectorControlsTest : public QObject {
    Q_OBJECT
private slots:
    void mapping_data() {
        QTest::addColumn<QString>("kind"); QTest::addColumn<QString>("value"); QTest::addColumn<QString>("widgetClass");
        QTest::newRow("text") << "text" << "中文标题" << "QLineEdit";
        QTest::newRow("number") << "number" << "12.5" << "QDoubleSpinBox";
        QTest::newRow("boolean") << "boolean" << "true" << "QCheckBox";
        QTest::newRow("select") << "select" << "a" << "QComboBox";
        QTest::newRow("color") << "color" << "#ff0000" << "QPushButton";
        QTest::newRow("list-readonly") << "list" << "[1,2]" << "QLineEdit";
    }
    void mapping() {
        QFETCH(QString, kind); QFETCH(QString, value); QFETCH(QString, widgetClass);
        qvw::domain::InspectorField field;
        field.control = qvw::domain::controlKindFromKey(kind); field.value = value;
        field.rawValue = value; field.disabledReason = "服务端只读原因";
        field.options.append(qvw::domain::InspectorOption{"Alpha", "a"});
        std::unique_ptr<QWidget> control(qvw::ui::createInspectorControl(field));
        QCOMPARE(QString(control->metaObject()->className()), widgetClass);
        QVERIFY(!control->isEnabled()); QCOMPARE(control->toolTip(), field.disabledReason);
        if (auto *box = qobject_cast<QCheckBox *>(control.get())) QVERIFY(box->isChecked());
        if (auto *combo = qobject_cast<QComboBox *>(control.get())) QCOMPARE(combo->currentText(), QString("Alpha"));
    }
};
QTEST_MAIN(InspectorControlsTest)
#include "InspectorControlsTest.moc"
