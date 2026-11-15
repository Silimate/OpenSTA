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

#pragma once

#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "DbCodec.hh"
#include "DbFormat.hh"

namespace sta {

// Container writer. Sections are indexed by a table of contents, so the
// string table can be emitted last even though the reader needs it first.
//
// Each section streams into the file as it is encoded, deflated block by block
// when compressing, so the write holds no whole section in memory. The header
// and table of contents are filled in by finish(). The bytes are the same as
// encoding every section in memory first.
class DbFileWriter
{
public:
  // Creates filename, leaving room for a table of contents of section_count
  // entries, the string table included. Throws FileNotWritable.
  DbFileWriter(std::string_view filename,
               bool compress,
               size_t section_count);
  // Removes the file unless finish() completed, so a failed write leaves no
  // partial file behind.
  ~DbFileWriter();
  DbStringTable *strings() { return &strings_; }
  // Encodes a section into the file. encode writes the payload through the
  // DbWriter it is handed. When deflate does not make the section smaller it
  // is stored as is, which runs encode a second time, so encode must write the
  // same bytes each time it runs.
  void addSection(DbSectionId id,
                  const std::function<void(DbWriter &)> &encode);
  // Opaque digest of the source files this snapshot was built from. Callers
  // compare it to decide whether a cached file is still valid.
  void setInputDigest(uint64_t digest) { input_digest_ = digest; }
  // Writes the string table section, then the header.
  void finish();

private:
  struct TocEntry
  {
    DbSectionId id;
    uint64_t offset;
    uint64_t stored_size;
    uint64_t raw_size;
    uint32_t checksum;
  };

  TocEntry encodeSection(DbSectionId id,
                         const std::function<void(DbWriter &)> &encode,
                         bool deflate);
  void seek(uint64_t offset);
  void checkStream();

  std::string filename_;
  bool compress_;
  size_t section_count_;
  std::ofstream stream_;
  bool finished_;
  DbStringTable strings_;
  std::vector<TocEntry> toc_;
  // Where the next section starts, and the furthest byte written so far, which
  // passes it when a section is rewritten undeflated.
  uint64_t offset_;
  uint64_t end_;
  uint32_t flags_;
  uint64_t input_digest_;
  // Deflate output block, reused across sections.
  std::vector<uint8_t> out_block_;
};

// Container reader. Throws DbCorrupt for a malformed file, a format version
// mismatch, or an ABI guard mismatch; all three mean regenerate from source.
class DbFileReader
{
public:
  DbFileReader();
  void read(std::string_view filename);
  DbStringTable *strings() { return &strings_; }
  bool hasSection(DbSectionId id) const;
  // Reader over the decompressed section payload, bound to the string table.
  DbReader sectionReader(DbSectionId id);
  uint64_t inputDigest() const { return input_digest_; }

private:
  DbStringTable strings_;
  std::map<DbSectionId, std::vector<uint8_t>> sections_;
  uint64_t input_digest_;
};

// FNV-1a over a byte range. Used for section checksums and for digesting the
// input file list.
uint32_t dbChecksum(const uint8_t *data, size_t size);
// Continues a dbChecksum over the next bytes of the same range.
uint32_t dbChecksum(uint32_t hash, const uint8_t *data, size_t size);
uint64_t dbDigest(uint64_t seed, const void *data, size_t size);

} // namespace sta
