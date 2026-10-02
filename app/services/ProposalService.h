#pragma once
#include "domain/EditProposal.h"
#include "domain/Project.h"
#include "domain/Snapshot.h"

namespace qvw::services {
class ProposalService {
public:
    static constexpr qsizetype MaxJsonBytes = 64 * 1024;
    static bool parseJson(const QByteArray &, domain::ProposalOrigin, domain::EditProposal *, QString *error);
    static bool validate(const domain::EditProposal &, const domain::Snapshot &, const domain::Project &, QString *error);
    static QString summary(const domain::EditProposal &, const domain::Snapshot &);
};
}
