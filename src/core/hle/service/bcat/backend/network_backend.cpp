#include "core/hle/service/bcat/backend/network_backend.h"

#ifdef ENABLE_WEB_SERVICE

#include <algorithm>
#include <fmt/format.h>

#include "common/hex_util.h"
#include "common/logging.h"
#include "core/core.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/file_sys/zip_reader.h"
#include "core/hle/service/bcat/bcat_types.h"

#include "web_service/nextendo_api.h"

namespace Service::BCAT {

using FileSys::VirtualDir;

namespace {

VirtualDir GetOrCreateRoot(const DirectoryGetter& getter, u64 title_id) {
    auto root = getter(title_id);
    return root;
}

bool CreateParentDirs(VirtualDir root, std::string_view rel_path, VirtualDir& out_parent,
                      std::string& out_leaf) {
    if (rel_path.empty()) {
        return false;
    }

    std::string path{rel_path};
    while (!path.empty() && path.back() == '/') {
        path.pop_back();
    }
    if (path.empty()) {
        return false;
    }

    auto slash = path.find_last_of('/');
    out_leaf = slash == std::string::npos ? path : path.substr(slash + 1);

    VirtualDir cur = root;
    if (slash != std::string::npos) {
        std::string dir = path.substr(0, slash);
        for (;;) {
            auto next = dir.find('/');
            std::string segment = next == std::string::npos ? dir : dir.substr(0, next);
            if (segment.empty() || segment == ".") {
                if (next == std::string::npos) {
                    break;
                }
                dir = dir.substr(next + 1);
                continue;
            }
            auto sub = cur->GetSubdirectory(segment);
            if (!sub) {
                cur = cur->CreateSubdirectory(segment);
            } else {
                cur = sub;
            }
            if (!cur) {
                return false;
            }
            if (next == std::string::npos) {
                break;
            }
            dir = dir.substr(next + 1);
        }
    }
    out_parent = cur;
    return !out_leaf.empty();
}

void SplitForProgress(std::string_view rel_path, std::string& dirname, std::string& filename) {
    auto slash = rel_path.find_last_of('/');
    if (slash == std::string::npos) {
        dirname.clear();
        filename = std::string{rel_path};
    } else {
        dirname = std::string{rel_path.substr(0, slash)};
        if (!dirname.empty() && dirname.back() == '/') {
            dirname.pop_back();
        }
        filename = std::string{rel_path.substr(slash + 1)};
    }
}

} // namespace

NetworkBcatBackend::NetworkBcatBackend(DirectoryGetter getter)
    : BcatBackend(std::move(getter)) {}

NetworkBcatBackend::~NetworkBcatBackend() = default;

bool NetworkBcatBackend::Synchronize(TitleIDVersion title, ProgressServiceBackend& progress) {
    LOG_DEBUG(Service_BCAT, "NetworkBcatBackend::Synchronize title_id={:016X}",
              title.title_id);

    auto root = GetOrCreateRoot(dir_getter, title.title_id);
    if (!root) {
        progress.FinishDownload(ResultSuccess);
        return true;
    }

    progress.StartConnecting();

    const std::string title_id_hex = fmt::format("{:016X}", title.title_id);
    auto zip_bytes = WebService::NextendoApi::DownloadBcatSeed(title_id_hex);
    if (zip_bytes.empty()) {
        progress.FinishDownload(ResultSuccess);
        return true;
    }

    progress.StartProcessingDataList();

    FileSys::ZipReader zip{zip_bytes};
    if (!zip.IsValid()) {
        LOG_WARNING(Service_BCAT, "BCAT seed for {:016X} was returned but is not a valid zip",
                    title.title_id);
        progress.FinishDownload(ResultSuccess);
        return true;
    }

    std::string last_dirname{};
    u64 file_index = 0;
    u64 total_bytes = 0;

    while (zip.Next()) {
        const auto& entry = zip.Current();
        if (entry.is_directory) {
            VirtualDir parent;
            std::string leaf;
            CreateParentDirs(root, entry.name, parent, leaf);
            continue;
        }

        std::string dirname, filename;
        SplitForProgress(entry.name, dirname, filename);
        if (!last_dirname.empty() && dirname != last_dirname) {
            progress.CommitDirectory(last_dirname);
        }
        last_dirname = dirname;

        VirtualDir parent;
        std::string leaf;
        if (!CreateParentDirs(root, entry.name, parent, leaf)) {
            LOG_WARNING(Service_BCAT, "Skipping BCAT entry with bad path: {}", entry.name);
            continue;
        }

        std::vector<u8> body;
        if (!zip.Read(body)) {
            LOG_WARNING(Service_BCAT, "Skipping BCAT entry that won't decompress: {}", entry.name);
            continue;
        }

        progress.StartDownloadingFile(dirname, filename, entry.uncompressed_size);

        auto file = parent->GetFile(leaf);
        if (!file) {
            file = parent->CreateFile(leaf);
        }
        if (file) {
            if (file->Resize(body.size())) {
                file->Write(body.data(), body.size(), 0);
                total_bytes += static_cast<u64>(body.size());
                progress.UpdateFileProgress(body.size());
            } else {
                LOG_WARNING(Service_BCAT, "Failed to resize BCAT entry: {}", entry.name);
            }
        } else {
            LOG_WARNING(Service_BCAT, "Failed to create BCAT entry: {}", entry.name);
        }

        progress.FinishDownloadingFile();
        ++file_index;
    }

    if (!last_dirname.empty()) {
        progress.CommitDirectory(last_dirname);
    }

    progress.FinishDownload(ResultSuccess);

    LOG_INFO(Service_BCAT,
             "NetworkBcatBackend::Synchronize complete for {:016X}: {} files, {} bytes",
             title.title_id, file_index, total_bytes);
    return true;
}

bool NetworkBcatBackend::SynchronizeDirectory(TitleIDVersion title, std::string name,
                                               ProgressServiceBackend& progress) {
    LOG_DEBUG(Service_BCAT, "NetworkBcatBackend::SynchronizeDirectory title_id={:016X} dir={}",
              title.title_id, name);

    auto root = GetOrCreateRoot(dir_getter, title.title_id);
    if (!root) {
        progress.FinishDownload(ResultSuccess);
        return true;
    }

    progress.StartConnecting();

    const std::string title_id_hex = fmt::format("{:016X}", title.title_id);
    auto zip_bytes = WebService::NextendoApi::DownloadBcatSeed(title_id_hex);
    if (zip_bytes.empty()) {
        progress.FinishDownload(ResultSuccess);
        return true;
    }

    progress.StartProcessingDataList();

    FileSys::ZipReader zip{zip_bytes};
    if (!zip.IsValid()) {
        progress.FinishDownload(ResultSuccess);
        return true;
    }

    u64 file_index = 0;
    while (zip.Next()) {
        const auto& entry = zip.Current();
        if (entry.is_directory) {
            continue;
        }
        auto first_slash = entry.name.find('/');
        if (first_slash == std::string::npos) {
            continue;
        }
        if (std::string_view{entry.name.data(), first_slash} != name) {
            continue;
        }

        std::string dirname, filename;
        SplitForProgress(entry.name, dirname, filename);

        VirtualDir parent;
        std::string leaf;
        if (!CreateParentDirs(root, entry.name, parent, leaf)) {
            continue;
        }

        std::vector<u8> body;
        if (!zip.Read(body)) {
            continue;
        }

        progress.StartDownloadingFile(dirname, filename, entry.uncompressed_size);
        auto file = parent->GetFile(leaf);
        if (!file) {
            file = parent->CreateFile(leaf);
        }
        if (file && file->Resize(body.size())) {
            file->Write(body.data(), body.size(), 0);
            progress.UpdateFileProgress(body.size());
        }
        progress.FinishDownloadingFile();
        ++file_index;
    }

    progress.CommitDirectory(name);

    progress.FinishDownload(ResultSuccess);
    LOG_INFO(Service_BCAT,
             "NetworkBcatBackend::SynchronizeDirectory complete for {:016X}/{}: {} files",
             title.title_id, name, file_index);
    return true;
}

bool NetworkBcatBackend::Clear(u64 title_id) {
    LOG_DEBUG(Service_BCAT, "NetworkBcatBackend::Clear title_id={:016X}", title_id);
    auto root = dir_getter(title_id);
    if (!root) {
        return true;
    }
    bool any_failed = false;
    for (const auto& sub : root->GetSubdirectories()) {
        if (!root->DeleteSubdirectoryRecursive(sub->GetName())) {
            any_failed = true;
        }
    }
    for (const auto& f : root->GetFiles()) {
        if (!root->DeleteFile(f->GetName())) {
            any_failed = true;
        }
    }
    return !any_failed;
}

void NetworkBcatBackend::SetPassphrase(u64 title_id, const Passphrase& passphrase) {
    LOG_DEBUG(Service_BCAT, "NetworkBcatBackend::SetPassphrase title_id={:016X} passphrase={}",
              title_id, Common::HexToString(passphrase));
}

std::optional<std::vector<u8>> NetworkBcatBackend::GetLaunchParameter(TitleIDVersion title) {
    return std::nullopt;
}

} // namespace Service::BCAT

#endif // ENABLE_WEB_SERVICE
