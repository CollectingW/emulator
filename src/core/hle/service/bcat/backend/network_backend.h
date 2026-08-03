#pragma once

#include "core/hle/service/bcat/backend/backend.h"

namespace Core {
class System;
}

namespace Service::BCAT {

class NetworkBcatBackend : public BcatBackend {
public:
    explicit NetworkBcatBackend(DirectoryGetter getter);
    ~NetworkBcatBackend() override;

    bool Synchronize(TitleIDVersion title, ProgressServiceBackend& progress) override;
    bool SynchronizeDirectory(TitleIDVersion title, std::string name,
                              ProgressServiceBackend& progress) override;

    bool Clear(u64 title_id) override;

    void SetPassphrase(u64 title_id, const Passphrase& passphrase) override;

    std::optional<std::vector<u8>> GetLaunchParameter(TitleIDVersion title) override;
};

} // namespace Service::BCAT
