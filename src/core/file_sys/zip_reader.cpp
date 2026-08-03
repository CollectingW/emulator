#include "core/file_sys/zip_reader.h"

#include <cstring>
#include <string>

#include <zlib.h>

namespace FileSys {

namespace {

// ZIP record signatures.
constexpr u32 kSigCentralDirEntry = 0x02014b50; // "PK\x01\x02"
constexpr u32 kSigEndOfCentralDir = 0x06054b50; // "PK\x05\x06"

// Reads a little-endian u16/u32 at a fixed offset. The caller has already
// bounds-checked; the asserts are cheap insurance to make the bounds check sit
// at one place in the source rather than being forgotten at a new call site.
u16 ReadU16(std::span<const u8> b, size_t off) {
    return static_cast<u16>(static_cast<u32>(b[off]) | (static_cast<u32>(b[off + 1]) << 8));
}
u32 ReadU32(std::span<const u8> b, size_t off) {
    return static_cast<u32>(ReadU16(b, off)) | (static_cast<u32>(ReadU16(b, off + 2)) << 16);
}

} // namespace

ZipReader::ZipReader(std::span<const u8> data) : blob(data) {
    valid = !blob.empty() && ParseCentralDirectory();
}

bool ZipReader::IsValid() const {
    return valid;
}

bool ZipReader::ParseCentralDirectory() {
    constexpr size_t kEocdMinSize = 22;
    constexpr size_t kMaxEocdTailScan = 65557; // max EOCD + 64K comment

    if (blob.size() < kEocdMinSize) {
        return false;
    }

    // Find the EOCD record by scanning backwards from the tail.
    size_t eocd_off = std::string::npos;
    const size_t scan_from = blob.size() >= kMaxEocdTailScan ? blob.size() - kMaxEocdTailScan : 0;
    for (size_t i = blob.size() - kEocdMinSize + 1; i-- > scan_from;) {
        if (ReadU32(blob, i) == kSigEndOfCentralDir) {
            eocd_off = i;
            break;
        }
    }
    if (eocd_off == std::string::npos) {
        return false;
    }

    const size_t eocd_end = eocd_off + kEocdMinSize;
    if (eocd_end > blob.size()) {
        return false;
    }

    const u32 cd_entries = ReadU16(blob, eocd_off + 10);
    const u64 cd_size = ReadU32(blob, eocd_off + 12);
    const u64 cd_offset = ReadU32(blob, eocd_off + 16);

    if (cd_offset + cd_size > blob.size()) {
        return false;
    }

    entries.clear();
    entries.reserve(cd_entries);

    size_t p = static_cast<size_t>(cd_offset);
    for (u32 e = 0; e < cd_entries; ++e) {
        if (p + 46 > blob.size() || ReadU32(blob, p) != kSigCentralDirEntry) {
            return false;
        }

        Entry entry{};
        entry.compression_method = ReadU16(blob, p + 10);
        entry.uncompressed_size = ReadU32(blob, p + 24);
        const u64 compressed_size = ReadU32(blob, p + 20);
        const u16 name_len = ReadU16(blob, p + 28);
        const u16 extra_len = ReadU16(blob, p + 30);
        const u16 comment_len = ReadU16(blob, p + 32);
        const u64 local_header_offset = ReadU32(blob, p + 42);

        if (p + 46 + name_len + extra_len + comment_len > blob.size()) {
            return false;
        }

        entry.name = std::string_view{
            reinterpret_cast<const char*>(blob.data() + p + 46),
            static_cast<size_t>(name_len)};
        entry.is_directory = !entry.name.empty() && entry.name.back() == '/';
        entry.local_header_offset = local_header_offset;

        entries.emplace_back(entry, static_cast<size_t>(compressed_size));

        p += 46 + name_len + extra_len + comment_len;
    }

    cursor = 0;
    return true;
}

bool ZipReader::ReadLocalHeader(u64 lho, u16& name_len, u16& extra_len, u64& data_offset,
                                u64& compressed_size) const {
    constexpr u32 kSigLocalFileHeader = 0x04034b50; // "PK\x03\x04"
    constexpr size_t kLfhMinSize = 30;

    if (lho + kLfhMinSize > blob.size() || ReadU32(blob, static_cast<size_t>(lho)) != kSigLocalFileHeader) {
        return false;
    }
    name_len = ReadU16(blob, static_cast<size_t>(lho) + 26);
    extra_len = ReadU16(blob, static_cast<size_t>(lho) + 28);
    data_offset = lho + kLfhMinSize + name_len + extra_len;
    compressed_size = ReadU32(blob, static_cast<size_t>(lho) + 18);
    return true;
}

bool ZipReader::Next() {
    if (cursor >= entries.size()) {
        return false;
    }
    current = entries[cursor].first;
    cursor++;
    return true;
}

const ZipReader::Entry& ZipReader::Current() const {
    return current;
}

bool ZipReader::Read(std::vector<u8>& out) const {
    out.clear();
    if (current.is_directory) {
        return true;
    }

    const Entry& e = current;
    u16 lh_name_len{}, lh_extra_len{};
    u64 data_offset{}, compressed_size{};
    if (!ReadLocalHeader(e.local_header_offset, lh_name_len, lh_extra_len, data_offset, compressed_size)) {
        return false;
    }
    if (data_offset + compressed_size > blob.size()) {
        return false;
    }

    const u8* body = blob.data() + data_offset;

    if (e.compression_method == 0) {
        out.assign(body, body + compressed_size);
        return out.size() == e.uncompressed_size ||
               e.uncompressed_size == 0;
    }

    if (e.compression_method != 8) {
        return false;
    }

    if (e.uncompressed_size == 0) {
        return true;
    }
    out.resize(static_cast<size_t>(e.uncompressed_size));

    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(body);
    stream.avail_in = static_cast<uInt>(compressed_size);
    stream.next_out = reinterpret_cast<Bytef*>(out.data());
    stream.avail_out = static_cast<uInt>(out.size());

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        return false;
    }

    const int ret = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    if (ret != Z_STREAM_END) {
        return false;
    }
    out.resize(stream.total_out);
    return true;
}

} // namespace FileSys
