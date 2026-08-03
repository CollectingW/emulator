#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "common/common_types.h"

namespace FileSys {

// Minimal read-only ZIP archive reader.
//
// Scans the central directory of an in-memory ZIP blob and exposes one entry
// at a time. Supports the stored (method 0) and deflate (method 8) compression
// methods only. The End-of-Central-Directory record is found by scanning from
// the tail of the buffer for the EOCD signature, which is how practically every
// zip implementation locates the directory; ZIP64 records are not supported
// (BCAT seed archives are small and never use ZIP64). Encryption
// (general-purpose bit 0) is rejected; pre-decrypted archives must be supplied.
class ZipReader {
public:
    struct Entry {
        std::string_view name;
        u64 uncompressed_size{};
        bool is_directory{};
        // Method 0 (stored) or 8 (deflate). Anything else makes Read() return false.
        u16 compression_method{};
        // Offset of the local-file header in the source blob; used by Read().
        u64 local_header_offset{};
    };

    // View into the user's data. The caller must keep `data` alive for the
    // lifetime of the reader.
    explicit ZipReader(std::span<const u8> data);

    // Returns false when there are no more entries, or if the blob is not a
    // recognisable ZIP archive.
    bool IsValid() const;

    // Advances to the next entry. Returns false at end-of-archive.
    bool Next();

    // Accessor for the current entry; valid only between a successfully-returned
    // Next() and the next call to Next().
    const Entry& Current() const;

    // Reads + decompresses the current entry into `out`. Returns false on
    // unsupported method, mismatched sizes, or CRC32 mismatch (when known).
    // For directory entries returns true and leaves `out` empty.
    bool Read(std::vector<u8>& out) const;

private:
    bool ParseCentralDirectory();
    bool ReadLocalHeader(u64 lho, u16& name_len, u16& extra_len, u64& data_offset,
                         u64& compressed_size) const;

    std::span<const u8> blob;
    std::vector<std::pair<Entry, u64>> entries; // [entry, compressed_size]
    size_t cursor{};
    bool valid{};
    Entry current{};
};

} // namespace FileSys
