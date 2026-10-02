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
    void editableTextCommitsOnlyChanges() {
        qvw::domain::InspectorField field; field.control=qvw::domain::ControlKind::Text;
        field.writable=true;field.value="before";field.rawValue="before";
        QVariant written; int calls=0;
        std::unique_ptr<QWidget> control(qvw::ui::createInspectorControl(field,nullptr,[&](const QVariant &value){written=value;++calls;}));
        auto *line=qobject_cast<QLineEdit*>(control.get());QVERIFY(line);QVERIFY(line->isEnabled());QVERIFY(!line->isReadOnly());
        QMetaObject::invokeMethod(line,"editingFinished");QCOMPARE(calls,0);
        line->setText("after");QMetaObject::invokeMethod(line,"editingFinished");QCOMPARE(calls,1);QCOMPARE(written.toString(),QString("after"));
    }
    void numberLiteralUsesNumericWireValue() {
        qvw::domain::InspectorField field;field.control=qvw::domain::ControlKind::Number;
        field.writable=true;field.value="18";field.rawValue="18";
        QVariant written;
        std::unique_ptr<QWidget> control(qvw::ui::createInspectorControl(field,nullptr,[&](const QVariant &value){written=value;}));
        auto *spin=qobject_cast<QDoubleSpinBox*>(control.get());QVERIFY(spin);QVERIFY(spin->isEnabled());
        spin->setValue(22);QMetaObject::invokeMethod(spin,"editingFinished");
        QCOMPARE(written.metaType(),QMetaType::fromType<double>());QCOMPARE(written.toDouble(),22.0);
    }
    void unchangedRoundedNumberDoesNotWrite() {
        qvw::domain::InspectorField field;field.control=qvw::domain::ControlKind::Number;
        field.writable=true;field.value="0.1234567891234567";field.rawValue=field.value;int writes=0;
        std::unique_ptr<QWidget> control(qvw::ui::createInspectorControl(field,nullptr,[&](const QVariant &){++writes;}));
        auto *spin=qobject_cast<QDoubleSpinBox*>(control.get());QVERIFY(spin);
        QMetaObject::invokeMethod(spin,"editingFinished");QCOMPARE(writes,0);
    }
    void booleanAndSelectPreserveValues() {
        qvw::domain::InspectorField field;field.control=qvw::domain::ControlKind::Boolean;
        field.writable=true;field.value="true";field.rawValue="true";QVariant written;
        std::unique_ptr<QWidget> control(qvw::ui::createInspectorControl(field,nullptr,[&](const QVariant &value){written=value;}));
        qobject_cast<QCheckBox*>(control.get())->setChecked(false);QCOMPARE(written.metaType(),QMetaType::fromType<bool>());QVERIFY(!written.toBool());
        field.control=qvw::domain::ControlKind::Select;field.value="a";field.rawValue="a";field.options={{"Alpha","a"},{"Beta","b"}};
        control.reset(qvw::ui::createInspectorControl(field,nullptr,[&](const QVariant &value){written=value;}));
        auto *combo=qobject_cast<QComboBox*>(control.get());combo->setCurrentIndex(1);
        QMetaObject::invokeMethod(combo,"activated",Q_ARG(int,1));QCOMPARE(written.toString(),QString("b"));
    }
    void undeclaredAndUnsupportedStayDisabledWithHandler() {
        qvw::domain::InspectorField field;field.control=qvw::domain::ControlKind::Text;
        std::unique_ptr<QWidget> one(qvw::ui::createInspectorControl(field,nullptr,[](const QVariant &){}));QVERIFY(!one->isEnabled());
        field.writable=true;field.control=qvw::domain::ControlKind::Unsupported;
        std::unique_ptr<QWidget> two(qvw::ui::createInspectorControl(field,nullptr,[](const QVariant &){}));QVERIFY(!two->isEnabled());
    }
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
