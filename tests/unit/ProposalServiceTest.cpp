#include "services/ProposalService.h"
#include "services/ProposalProvider.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
using namespace qvw;
namespace {
domain::Snapshot snapshot() {
    domain::Snapshot s;s.revision=7;s.sourceFiles.insert("scene.svml","source");
    s.sourceFingerprint=QCryptographicHash::hash("source",QCryptographicHash::Sha256);
    domain::Clip c;c.id="cover";
    for(const QString &binding:{QString("title"),QString("color"),QString("image")}) {
        domain::InspectorField f;f.id="parameter:"+binding;f.binding=binding;f.label=binding;
        f.control=binding=="color"?domain::ControlKind::Color:domain::ControlKind::Text;
        f.rawValue=binding=="color"?"#35bca8":"old";f.value=f.rawValue.toString();f.writable=true;c.inspector.append(f);
    }
    domain::Track t;t.clips.append(c);s.tracks.append(t);return s;
}
domain::EditProposal proposal(const domain::Snapshot &s) {
    domain::EditProposal p;p.revision=s.revision;p.sourceFingerprint=s.sourceFingerprint;
    p.entityId="cover";p.fieldId="parameter:title";p.value="new";p.origin=domain::ProposalOrigin::Demo;return p;
}
QJsonObject json(const domain::Snapshot &s) {
    return {{"format","qvw.edit-proposal@1"},{"revision",s.revision},{"sourceFingerprint",QString::fromLatin1(s.sourceFingerprint.toHex())},
            {"entityId","cover"},{"fieldId","parameter:title"},{"value","new"},{"origin","real-model"}};
}
domain::Project project(const QString &root) {domain::Project p;p.rootPath=root;p.templateId="title-card";return p;}
void addImage(domain::Project &p,const QString &relative) {
    QDir(p.rootPath).mkpath(QFileInfo(relative).path());QFile f(QDir(p.rootPath).filePath(relative));f.open(QIODevice::WriteOnly);f.write("image");
    domain::Asset a;a.path=relative;a.mime="image/png";a.originalName="image.png";p.assets.append(a);
}
}
class ProposalServiceTest : public QObject {
    Q_OBJECT
private slots:
    void strictRoundTripOverridesSelfClaimedOrigin() {
        const auto s=snapshot();domain::EditProposal p;QString error;
        QVERIFY2(services::ProposalService::parseJson(QJsonDocument(json(s)).toJson(),domain::ProposalOrigin::ExternalUnverified,&p,&error),qPrintable(error));
        QCOMPARE(p.sourceFingerprint,s.sourceFingerprint);QCOMPARE(p.origin,domain::ProposalOrigin::ExternalUnverified);
        domain::EditProposal round;QVERIFY(services::ProposalService::parseJson(p.toJson(),domain::ProposalOrigin::Demo,&round,&error));
        QCOMPARE(round.value,p.value);QCOMPARE(round.origin,domain::ProposalOrigin::Demo);
        QVERIFY(domain::proposalOriginLabel(p.origin).contains("未核验"));QVERIFY(domain::proposalOriginLabel(round.origin).contains("模拟"));
    }
    void schemaFailures_data() {
        QTest::addColumn<QByteArray>("bytes");const auto base=json(snapshot());
        for(const QString &key:base.keys()){auto j=base;j.remove(key);QTest::newRow(qPrintable("missing-"+key))<<QJsonDocument(j).toJson();}
        for(const QString &key:{QString("operations"),QString("Source"),QString("Shell"),QString("path"),QString("extra")}) {
            auto j=base;j.insert(key,QJsonArray{});QTest::newRow(qPrintable("unknown-"+key))<<QJsonDocument(j).toJson();
        }
        for(const QString &key:{QString("format"),QString("sourceFingerprint"),QString("entityId"),QString("fieldId"),QString("origin")}) {
            auto j=base;j[key]=true;QTest::newRow(qPrintable("type-"+key))<<QJsonDocument(j).toJson();
        }
        for(const QJsonValue &value:{QJsonValue(-1),QJsonValue(1.2),QJsonValue("7"),QJsonValue(2147483648.0)}) {
            auto j=base;j["revision"]=value;QTest::newRow(qPrintable("revision-"+QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson())))<<QJsonDocument(j).toJson();
        }
        auto wrong=base;wrong["format"]="qvw.edit-proposal@2";QTest::newRow("format")<<QJsonDocument(wrong).toJson();
        wrong=base;wrong["sourceFingerprint"]=QString(64,'z');QTest::newRow("fingerprint")<<QJsonDocument(wrong).toJson();
        wrong=base;wrong["entityId"]="";QTest::newRow("empty-entity")<<QJsonDocument(wrong).toJson();
        wrong=base;wrong["fieldId"]=QString(257,'a');QTest::newRow("long-field")<<QJsonDocument(wrong).toJson();
        wrong=base;wrong["origin"]=QString(129,'a');QTest::newRow("long-origin")<<QJsonDocument(wrong).toJson();
        for(const QJsonValue &value:{QJsonValue(QJsonObject{}),QJsonValue(QJsonArray{}),QJsonValue(QJsonValue::Null),QJsonValue(QString(8193,'x'))}) {
            wrong=base;wrong["value"]=value;QTest::newRow(qPrintable("invalid-value-"+QString::number(value.type())))<<QJsonDocument(wrong).toJson();
        }
        QTest::newRow("array")<<QJsonDocument(QJsonArray{base,base}).toJson();QTest::newRow("malformed")<<QByteArray("{");
        QTest::newRow("over-64k")<<QByteArray(65537,' ');
    }
    void schemaFailures() {
        QFETCH(QByteArray,bytes);domain::EditProposal p;QString error;
        QVERIFY(!services::ProposalService::parseJson(bytes,domain::ProposalOrigin::ExternalUnverified,&p,&error));QVERIFY(!error.isEmpty());
    }
    void validatesCurrentWritableFieldAndVersion() {
        QTemporaryDir dir;auto s=snapshot();auto p=proposal(s);const auto projectValue=project(dir.path());QString error;
        QVERIFY2(services::ProposalService::validate(p,s,projectValue,&error),qPrintable(error));
        const auto summary=services::ProposalService::summary(p,s);QVERIFY(summary.contains("old"));QVERIFY(summary.contains("new"));QVERIFY(summary.contains("模拟"));
        p.revision++;QVERIFY(!services::ProposalService::validate(p,s,projectValue,&error));p=proposal(s);p.sourceFingerprint[0]^=1;
        QVERIFY(!services::ProposalService::validate(p,s,projectValue,&error));p=proposal(s);
        s.tracks[0].clips[0].inspector[0].writable=false;QVERIFY(!services::ProposalService::validate(p,s,projectValue,&error));
        s=snapshot();p.fieldId="Source";QVERIFY(!services::ProposalService::validate(p,s,projectValue,&error));
        p=proposal(s);p.value=3;QVERIFY(!services::ProposalService::validate(p,s,projectValue,&error));
        p=proposal(s);s.revision=-1;QVERIFY(!services::ProposalService::validate(p,s,projectValue,&error));
    }
    void numberBooleanSelectAndColorAreTyped() {
        QTemporaryDir dir;auto s=snapshot();auto p=proposal(s);auto projectValue=project(dir.path());QString error;
        auto &f=s.tracks[0].clips[0].inspector[0];f.control=domain::ControlKind::Number;f.rawValue="18";p.value=24.0;
        QVERIFY(services::ProposalService::validate(p,s,projectValue,&error));p.value="24";QVERIFY(!services::ProposalService::validate(p,s,projectValue,&error));
        f.control=domain::ControlKind::Boolean;f.rawValue="true";p.value=false;QVERIFY(services::ProposalService::validate(p,s,projectValue,&error));p.value="false";QVERIFY(!services::ProposalService::validate(p,s,projectValue,&error));
        f.control=domain::ControlKind::Select;f.rawValue="first";f.options={{"First","first"},{"Second","second"}};
        p.value="second";QVERIFY(services::ProposalService::validate(p,s,projectValue,&error));p.value="other";QVERIFY(!services::ProposalService::validate(p,s,projectValue,&error));
        p.fieldId="parameter:color";p.value="#e47735";QVERIFY(services::ProposalService::validate(p,s,projectValue,&error));p.value="orange";QVERIFY(!services::ProposalService::validate(p,s,projectValue,&error));
    }
    void numberControlCannotCoerceBooleanCurrentValue() {
        QTemporaryDir dir;auto s=snapshot();auto p=proposal(s);QString error;
        auto &field=s.tracks[0].clips[0].inspector[0];field.control=domain::ControlKind::Number;field.rawValue=true;p.value=1.0;
        QVERIFY(!services::ProposalService::validate(p,s,project(dir.path()),&error));QVERIFY(!error.isEmpty());
    }
    void imageMustBeRegisteredExistingAndSafe() {
        QTemporaryDir dir,outside;auto s=snapshot();auto p=proposal(s);auto projectValue=project(dir.path());QString error;
        addImage(projectValue,"assets/one.png");p.fieldId="parameter:image";
        for(const QString &value:{QString("assets/one.png"),QString("./assets/one.png")}) {p.value=value;QVERIFY2(services::ProposalService::validate(p,s,projectValue,&error),qPrintable(error));}
        for(const QString &value:{QString("assets/unknown.png"),QString("../outside.png"),QString("/tmp/one.png"),QString("https://example.com/x.png"),QString("././assets/one.png")}) {
            p.value=value;QVERIFY(!services::ProposalService::validate(p,s,projectValue,&error));
        }
        p.value="./assets/one.png";QFile::remove(dir.filePath("assets/one.png"));QVERIFY(!services::ProposalService::validate(p,s,projectValue,&error));
        QFile target(outside.filePath("outside.png"));QVERIFY(target.open(QIODevice::WriteOnly));target.write("image");target.close();
        QVERIFY(QFile::link(target.fileName(),dir.filePath("assets/one.png")));QVERIFY(!services::ProposalService::validate(p,s,projectValue,&error));
    }
    void demoUsesDeclaredBindingsAndLabelsSimulation() {
        QTemporaryDir dir;auto s=snapshot();auto projectValue=project(dir.path());addImage(projectValue,"assets/one.png");
        services::DemoProposalProvider provider;domain::EditProposal p;QString error;
        QVERIFY2(provider.generate("标题改为校园",s,projectValue,&p,&error),qPrintable(error));QCOMPARE(p.fieldId,QString("parameter:title"));QCOMPARE(p.value.toString(),QString("校园"));QCOMPARE(p.origin,domain::ProposalOrigin::Demo);
        QVERIFY(provider.generate("主题色改为#e47735",s,projectValue,&p,&error));QCOMPARE(p.fieldId,QString("parameter:color"));
        QVERIFY(provider.generate("主题色改为橙色",s,projectValue,&p,&error));QCOMPARE(p.value.toString(),QString("#e47735"));
        QVERIFY(provider.generate("图片使用第1张",s,projectValue,&p,&error));QCOMPARE(p.value.toString(),QString("./assets/one.png"));
        for(const QString &input:{QString("运行Shell"),QString("修改Source"),QString("图片使用第0张"),QString("图片使用第2张"),QString("主题色改为red")})QVERIFY(!provider.generate(input,s,projectValue,&p,&error));
        s.tracks[0].clips[0].inspector[0].binding="";QVERIFY(!provider.generate("标题改为校园",s,projectValue,&p,&error));
        s=snapshot();s.tracks[0].clips[0].inspector.append(s.tracks[0].clips[0].inspector[0]);QVERIFY(!provider.generate("标题改为校园",s,projectValue,&p,&error));
        s=snapshot();projectValue.templateId="unknown";QVERIFY(!provider.generate("标题改为校园",s,projectValue,&p,&error));
    }
};
QTEST_GUILESS_MAIN(ProposalServiceTest)
#include "ProposalServiceTest.moc"
