#pragma once
#include "services/ProposalService.h"

namespace qvw::services {
class IProposalProvider {
public:
    virtual ~IProposalProvider() = default;
    virtual bool generate(const QString &input, const domain::Snapshot &, const domain::Project &,
                          domain::EditProposal *, QString *error) const = 0;
};
class DemoProposalProvider final : public IProposalProvider {
public:
    bool generate(const QString &, const domain::Snapshot &, const domain::Project &,
                  domain::EditProposal *, QString *error) const override;
};
}
