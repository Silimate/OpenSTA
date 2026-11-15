// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Silimate, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#include "DbFile.hh"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "Error.hh"
#include "Format.hh"
#include "Zlib.hh"

namespace sta {

// Size of a single table of contents entry: id, offset, stored size, raw size
// and checksum.
static constexpr size_t stadb_toc_entry_size = 4 + 8 + 8 + 8 + 4;

uint32_t
dbChecksum(uint32_t hash, const uint8_t *data, size_t size)
{
  for (size_t i = 0; i < size; i++) {
    hash ^= data[i];
    hash *= 16777619u;
  }
  return hash;
}

uint32_t
dbChecksum(const uint8_t *data, size_t size)
{
  return dbChecksum(2166136261u, data, size);
}

uint64_t
dbDigest(uint64_t seed, const void *data, size_t size)
{
  const uint8_t *bytes = static_cast<const uint8_t*>(data);
  uint64_t hash = seed;
  for (size_t i = 0; i < size; i++) {
    hash ^= bytes[i];
    hash *= 1099511628211ull;
  }
  return hash;
}

////////////////////////////////////////////////////////////////

// Section bytes travel to the file in blocks of this size, and deflate
// writes its output in blocks of the same size.
static constexpr size_t stadb_stream_block = 1 << 20;

static constexpr size_t
headerSize(size_t section_count)
{
  // magic, version, ABI guard, flags, input digest, scene count, section count
  return 4 + 4 + 8 + 4 + 8 + 4 + 4 + section_count * stadb_toc_entry_size;
}

// Writes one section's bytes into the file as they arrive, deflated when asked,
// and tallies what its table of contents entry records.
class DbSectionSink : public DbByteSink
{
public:
  DbSectionSink(std::ofstream &stream,
                bool deflate,
                std::vector<uint8_t> &out_block);
  ~DbSectionSink() override;
  void write(const uint8_t *data, size_t size) override;
  // Flushes what deflate still holds.
  void finish();
  // True when deflate reported an error, in which case the section is stored
  // as is, as it was when compress2 failed.
  bool failed() const { return failed_; }
  uint64_t rawSize() const { return raw_size_; }
  uint64_t storedSize() const { return stored_size_; }
  uint32_t checksum() const { return checksum_; }

private:
  void emit(const uint8_t *data, size_t size);
  void pump(int flush);

  std::ofstream &stream_;
  bool deflate_;
  bool failed_;
  std::vector<uint8_t> &out_block_;
  uint64_t raw_size_;
  uint64_t stored_size_;
  uint32_t checksum_;
#ifdef ZLIB_FOUND
  z_stream zstream_;
#endif
};

DbSectionSink::DbSectionSink(std::ofstream &stream,
                             bool deflate,
                             std::vector<uint8_t> &out_block) :
  stream_(stream),
  deflate_(deflate),
  failed_(false),
  out_block_(out_block),
  raw_size_(0),
  stored_size_(0),
  checksum_(dbChecksum(nullptr, 0))
{
#ifdef ZLIB_FOUND
  if (deflate_) {
    zstream_ = z_stream{};
    // The level and the default window and memory settings are what compress2
    // used, so the deflated bytes match it.
    if (deflateInit(&zstream_, Z_BEST_SPEED) != Z_OK) {
      deflate_ = false;
      failed_ = true;
    }
  }
#else
  if (deflate_) {
    deflate_ = false;
    failed_ = true;
  }
#endif
}

DbSectionSink::~DbSectionSink()
{
#ifdef ZLIB_FOUND
  if (deflate_)
    deflateEnd(&zstream_);
#endif
}

void
DbSectionSink::emit(const uint8_t *data, size_t size)
{
  stream_.write(reinterpret_cast<const char*>(data), size);
  stored_size_ += size;
  checksum_ = dbChecksum(checksum_, data, size);
}

void
DbSectionSink::pump(int flush)
{
#ifdef ZLIB_FOUND
  while (true) {
    zstream_.next_out = out_block_.data();
    zstream_.avail_out = static_cast<uInt>(out_block_.size());
    int status = ::deflate(&zstream_, flush);
    if (status == Z_STREAM_ERROR) {
      failed_ = true;
      return;
    }
    emit(out_block_.data(), out_block_.size() - zstream_.avail_out);
    // Done once the input is consumed with room to spare, or, when finishing,
    // once the stream has ended.
    if (flush == Z_FINISH ? status == Z_STREAM_END : zstream_.avail_out != 0)
      return;
  }
#else
  (void) flush;
#endif
}

void
DbSectionSink::write(const uint8_t *data, size_t size)
{
  raw_size_ += size;
  if (failed_)
    // The section is about to be rewritten as is, so the rest is not needed.
    return;
  if (!deflate_) {
    emit(data, size);
    return;
  }
#ifdef ZLIB_FOUND
  while (size > 0 && !failed_) {
    // avail_in is an unsigned int.
    size_t chunk = std::min<size_t>(size, size_t(1) << 30);
    zstream_.next_in = const_cast<Bytef*>(data);
    zstream_.avail_in = static_cast<uInt>(chunk);
    pump(Z_NO_FLUSH);
    data += chunk;
    size -= chunk;
  }
#endif
}

void
DbSectionSink::finish()
{
#ifdef ZLIB_FOUND
  if (deflate_ && !failed_) {
    zstream_.next_in = nullptr;
    zstream_.avail_in = 0;
    pump(Z_FINISH);
  }
#endif
}

////////////////////////////////////////////////////////////////

DbFileWriter::DbFileWriter(std::string_view filename,
                           bool compress,
                           size_t section_count) :
  filename_(filename),
  compress_(compress),
  section_count_(section_count),
  finished_(false),
  offset_(headerSize(section_count)),
  end_(offset_),
  flags_(0),
  input_digest_(0)
{
  stream_.open(filename_, std::ios::binary | std::ios::trunc | std::ios::out);
  if (!stream_.is_open())
    throw FileNotWritable(filename);
  // Placeholder header, filled in once the section offsets are known.
  std::vector<char> zeros(offset_, 0);
  stream_.write(zeros.data(), zeros.size());
  checkStream();
#ifndef ZLIB_FOUND
  // Every section is stored as is, without first trying to deflate it.
  compress_ = false;
#endif
  if (compress_)
    out_block_.resize(stadb_stream_block);
}

DbFileWriter::~DbFileWriter()
{
  if (!finished_ && stream_.is_open()) {
    stream_.close();
    std::remove(filename_.c_str());
  }
}

void
DbFileWriter::checkStream()
{
  if (stream_.fail())
    throw FileNotWritable(filename_);
}

void
DbFileWriter::seek(uint64_t offset)
{
  stream_.seekp(static_cast<std::streamoff>(offset));
  checkStream();
}

DbFileWriter::TocEntry
DbFileWriter::encodeSection(DbSectionId id,
                            const std::function<void(DbWriter &)> &encode,
                            bool deflate)
{
  DbSectionSink sink(stream_, deflate, out_block_);
  DbWriter writer(&strings_);
  writer.streamTo(&sink, stadb_stream_block);
  encode(writer);
  writer.flush();
  sink.finish();
  checkStream();
  end_ = std::max(end_, offset_ + sink.storedSize());
  if (sink.failed())
    // Larger than any raw size, so the caller stores the section as is.
    return TocEntry{id, offset_, UINT64_MAX, sink.rawSize(), 0};
  return TocEntry{id, offset_, sink.storedSize(), sink.rawSize(),
                  sink.checksum()};
}

void
DbFileWriter::addSection(DbSectionId id,
                         const std::function<void(DbWriter &)> &encode)
{
  if (compress_) {
    TocEntry entry = encodeSection(id, encode, true);
    if (entry.raw_size > 0 && entry.stored_size < entry.raw_size) {
      flags_ |= stadb_flag_compressed;
      toc_.push_back(entry);
      offset_ += entry.stored_size;
      return;
    }
    // A section is stored verbatim when deflating it did not pay off, so the
    // per section raw and stored sizes are what actually select the decode
    // path.
    seek(offset_);
  }
  TocEntry entry = encodeSection(id, encode, false);
  toc_.push_back(entry);
  offset_ += entry.stored_size;
}

void
DbFileWriter::finish()
{
  // The string table is emitted last because encoding the other sections is
  // what populates it.
  addSection(DbSectionId::strings, [this](DbWriter &writer) {
    writer.putU64(strings_.size());
    for (size_t id = 0; id < strings_.size(); id++) {
      std::string_view str = strings_.string(DbStrId(static_cast<uint32_t>(id)));
      writer.putBlob(str.data(), str.size());
    }
  });
  if (toc_.size() != section_count_)
    throw DbCorrupt(sta::format("stadb writer planned {} sections but wrote {}",
                                section_count_, toc_.size()));

  DbWriter header;
  header.putRawU32(stadb_magic);
  header.putRawU32(stadb_version);
  header.putRawU64(stadbAbiGuard());
  header.putRawU32(flags_);
  header.putRawU64(input_digest_);
  header.putRawU32(stadb_scene_count);
  header.putRawU32(static_cast<uint32_t>(toc_.size()));
  for (const TocEntry &entry : toc_) {
    header.putRawU32(static_cast<uint32_t>(entry.id));
    header.putRawU64(entry.offset);
    header.putRawU64(entry.stored_size);
    header.putRawU64(entry.raw_size);
    header.putRawU32(entry.checksum);
  }
  seek(0);
  stream_.write(reinterpret_cast<const char*>(header.bytes().data()),
                header.size());
  stream_.close();
  if (stream_.fail())
    throw FileNotWritable(filename_);
  finished_ = true;
  // A section rewritten undeflated is shorter than the deflated try it
  // overwrote, which leaves a tail when it was the last one.
  if (end_ > offset_) {
    std::error_code error;
    std::filesystem::resize_file(filename_, offset_, error);
    if (error)
      throw FileNotWritable(filename_);
  }
}

static void
dbUncompress(const uint8_t *stored,
             size_t stored_size,
             size_t raw_size,
             std::vector<uint8_t> &raw)
{
#ifdef ZLIB_FOUND
  // Deflate cannot expand by more than 1032:1, so a larger claim is a corrupt
  // or crafted header rather than a real payload. Without this the declared
  // size alone decides the allocation, and a few hundred bytes on disk can ask
  // for every byte of address space.
  if (raw_size > stored_size * stadb_max_inflate_ratio)
    throw DbCorrupt("stadb section claims more decompressed bytes than deflate "
                    "can produce");
  raw.resize(raw_size);
  uLongf out_size = static_cast<uLongf>(raw_size);
  int status = uncompress(raw.data(), &out_size, stored,
                          static_cast<uLong>(stored_size));
  if (status != Z_OK || out_size != raw_size)
    throw DbCorrupt("stadb section failed to decompress");
#else
  (void) stored;
  (void) stored_size;
  (void) raw_size;
  (void) raw;
  throw DbCorrupt("stadb file is compressed but zlib is not available");
#endif
}

DbFileReader::DbFileReader() :
  input_digest_(0)
{
}

void
DbFileReader::read(std::string_view filename)
{
  std::string name(filename);
  std::ifstream stream(name, std::ios::binary | std::ios::ate);
  if (!stream.is_open())
    throw FileNotReadable(filename);
  std::streamsize size = stream.tellg();
  if (size < 0)
    throw FileNotReadable(filename);
  // Reject before the file vector is allocated. The section-size cap only
  // runs after the whole file is already in memory.
  if (static_cast<uint64_t>(size) > stadb_max_file_bytes)
    throw DbCorrupt("stadb file is larger than this build will restore");
  stream.seekg(0);
  std::vector<uint8_t> file(static_cast<size_t>(size));
  stream.read(reinterpret_cast<char*>(file.data()), size);
  if (stream.fail())
    throw FileNotReadable(filename);

  DbReader header(file.data(), file.size());
  if (header.getRawU32() != stadb_magic)
    throw DbCorrupt(sta::format("{} is not a stadb file", filename));
  uint32_t version = header.getRawU32();
  if (version != stadb_version)
    throw DbCorrupt(sta::format("stadb version {} but this build writes {}",
                                version, stadb_version));
  uint64_t abi_guard = header.getRawU64();
  if (abi_guard != stadbAbiGuard())
    throw DbCorrupt("stadb was written by a build with different data "
                    "structure layouts");
  header.getRawU32();
  input_digest_ = header.getRawU64();
  uint32_t scene_count = header.getRawU32();
  if (scene_count != stadb_scene_count)
    throw DbCorrupt(sta::format("stadb has {} scenes but this build supports "
                                "only {}", scene_count, stadb_scene_count));
  uint32_t section_count = header.getRawU32();

  for (uint32_t i = 0; i < section_count; i++) {
    DbSectionId id = static_cast<DbSectionId>(header.getRawU32());
    uint64_t offset = header.getRawU64();
    uint64_t stored_size = header.getRawU64();
    uint64_t raw_size = header.getRawU64();
    uint32_t checksum = header.getRawU32();
    // Subtraction, because offset + stored_size can wrap and describe a range
    // that looks like it fits while pointing outside the file.
    if (offset > file.size() || stored_size > file.size() - offset)
      throw DbCorrupt("stadb section extends past end of file");
    const uint8_t *stored = file.data() + offset;
    if (dbChecksum(stored, stored_size) != checksum)
      throw DbCorrupt("stadb section checksum mismatch");
    // Ratio-valid headers can still name a size that will not fit in memory.
    // Reject those before the section vector is allocated.
    if (raw_size > stadb_max_section_bytes)
      throw DbCorrupt("stadb section claims more bytes than this build will "
                      "restore");
    std::vector<uint8_t> raw;
    if (stored_size == raw_size)
      raw.assign(stored, stored + stored_size);
    else
      dbUncompress(stored, stored_size, raw_size, raw);
    sections_[id] = std::move(raw);
  }

  if (!hasSection(DbSectionId::strings))
    throw DbCorrupt("stadb has no string table");
  DbReader string_reader = sectionReader(DbSectionId::strings);
  size_t string_count = string_reader.getCount("string table");
  strings_.reset();
  for (size_t i = 0; i < string_count; i++)
    strings_.append(string_reader.getBlob());
  string_reader.checkFullyConsumed("string table");
}

bool
DbFileReader::hasSection(DbSectionId id) const
{
  return sections_.find(id) != sections_.end();
}

DbReader
DbFileReader::sectionReader(DbSectionId id)
{
  auto itr = sections_.find(id);
  if (itr == sections_.end())
    throw DbCorrupt("stadb is missing a required section");
  return DbReader(itr->second.data(), itr->second.size(), &strings_);
}

} // namespace sta
