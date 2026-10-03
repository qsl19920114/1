#pragma once
#include <QJsonObject>
#include <QJsonArray>
namespace qvw::infra {
// Application UI preferences and bounded local activity, never execution grants.
class WorkspaceStore {
public:
 explicit WorkspaceStore(QString path);
 bool load(QString *error=nullptr);
 bool save(QString *error=nullptr) const;
 QJsonObject layout() const{return m_layout;}
 void setLayout(const QJsonObject &layout){m_layout=layout;}
 QJsonArray recent() const{return m_recent;}
 void visit(const QString &manifest,const QString &name);
 int frame(const QString &manifest) const;
 void setFrame(const QString &manifest,int frame);
 QJsonArray history() const{return m_history;}
 void record(QJsonObject record);
private:
 QString m_path;QJsonObject m_layout;QJsonArray m_recent,m_history;
};
}
