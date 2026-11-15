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

#include "DbCodec.hh"

#include <algorithm>
#include <functional>

#include "Format.hh"

namespace sta {

DbCorrupt::DbCorrupt(const std::string &msg) :
  msg_(sta::format("{} {}", stadb_error_corrupt, msg))
{
}

const char *
DbCorrupt::what() const noexcept
{
  return msg_.c_str();
}

////////////////////////////////////////////////////////////////

// Arena block size. Strings longer than a block get a block of their own.
static constexpr size_t string_block_size = 1 << 20;
// Length prefix ahead of each string's bytes in the arena.
static constexpr size_t string_length_size = sizeof(uint32_t);

DbStringTable::DbStringTable()
{
  clear();
}

void
DbStringTable::reset()
{
  blocks_.clear();
  block_next_ = nullptr;
  block_left_ = 0;
  entries_.clear();
  slots_.clear();
  indexed_ = 0;
}

void
DbStringTable::clear()
{
  reset();
  // Id 0 is the null/empty string.
  intern("");
}

uint32_t
DbStringTable::hash(std::string_view str)
{
  uint64_t full = std::hash<std::string_view>()(str);
  return static_cast<uint32_t>(full ^ (full >> 32));
}

const char *
DbStringTable::store(std::string_view str)
{
  if (str.size() >= slot_empty)
    throw DbCorrupt("stadb string is too long to store");
  size_t record_size = string_length_size + str.size() + 1;
  if (record_size > block_left_) {
    size_t block_size = std::max(record_size, string_block_size);
    blocks_.emplace_back(new char[block_size]);
    block_next_ = blocks_.back().get();
    block_left_ = block_size;
  }
  char *record = block_next_;
  uint32_t length = static_cast<uint32_t>(str.size());
  std::memcpy(record, &length, string_length_size);
  if (!str.empty())
    std::memcpy(record + string_length_size, str.data(), str.size());
  record[string_length_size + str.size()] = '\0';
  block_next_ += record_size;
  block_left_ -= record_size;
  return record;
}

void
DbStringTable::rehash(size_t slot_count)
{
  std::vector<Slot> slots(slot_count, Slot{slot_empty, 0});
  size_t mask = slot_count - 1;
  for (const Slot &slot : slots_) {
    if (slot.id != slot_empty) {
      size_t i = slot.hash & mask;
      while (slots[i].id != slot_empty)
        i = (i + 1) & mask;
      slots[i] = slot;
    }
  }
  slots_ = std::move(slots);
}

uint32_t
DbStringTable::findOrInsert(std::string_view str,
                            uint32_t hash,
                            uint32_t id)
{
  // Grow before the table passes three quarters full; probe lengths climb
  // steeply past that.
  if ((indexed_ + 1) * 4 > slots_.size() * 3)
    rehash(std::max<size_t>(slots_.size() * 2, 64));
  size_t mask = slots_.size() - 1;
  size_t i = hash & mask;
  while (slots_[i].id != slot_empty) {
    if (slots_[i].hash == hash
        && string(DbStrId(slots_[i].id)) == str)
      return slots_[i].id;
    i = (i + 1) & mask;
  }
  slots_[i] = Slot{id, hash};
  indexed_++;
  return id;
}

// Indexes ids added by append() since the last intern. A repeat keeps its
// first id, as the map this table replaced did.
void
DbStringTable::indexPending()
{
  while (indexed_ < entries_.size()) {
    uint32_t id = static_cast<uint32_t>(indexed_);
    std::string_view str = string(DbStrId(id));
    if (findOrInsert(str, hash(str), id) != id)
      // Counted as covered even though only its first copy is in the index.
      indexed_++;
  }
}

DbStrId
DbStringTable::intern(std::string_view str)
{
  indexPending();
  if (entries_.size() >= slot_empty)
    throw DbCorrupt("stadb string table is full");
  uint32_t next_id = static_cast<uint32_t>(entries_.size());
  uint32_t id = findOrInsert(str, hash(str), next_id);
  if (id == next_id)
    entries_.push_back(store(str));
  return DbStrId(id);
}

void
DbStringTable::append(std::string_view str)
{
  if (entries_.size() >= slot_empty)
    throw DbCorrupt("stadb string table is full");
  entries_.push_back(store(str));
}

std::string_view
DbStringTable::string(DbStrId id) const
{
  if (id.id >= entries_.size())
    return std::string_view();
  const char *record = entries_[id.id];
  uint32_t length;
  std::memcpy(&length, record, string_length_size);
  return std::string_view(record + string_length_size, length);
}

const char *
DbStringTable::cstring(DbStrId id) const
{
  if (id.isNull() || id.id >= entries_.size())
    return nullptr;
  return entries_[id.id] + string_length_size;
}

////////////////////////////////////////////////////////////////

void
DbIndexMap::set(uint32_t id,
                uint32_t index)
{
  if (id >= indexes_.size())
    indexes_.resize(static_cast<size_t>(id) + 1, missing);
  indexes_[id] = index;
}

////////////////////////////////////////////////////////////////

DbWriter::DbWriter(DbStringTable *strings) :
  strings_(strings)
{
}

void
DbWriter::streamTo(DbByteSink *sink,
                   size_t block_size)
{
  flush();
  sink_ = sink;
  block_size_ = block_size;
  if (sink_)
    bytes_.reserve(block_size_ + 16);
}

void
DbWriter::flush()
{
  if (sink_ && !bytes_.empty()) {
    sink_->write(bytes_.data(), bytes_.size());
    bytes_.clear();
  }
}

void
DbWriter::putU8(uint8_t v)
{
  bytes_.push_back(v);
  spill();
}

void
DbWriter::putU64(uint64_t v)
{
  while (v >= 0x80) {
    bytes_.push_back(static_cast<uint8_t>(v) | 0x80);
    v >>= 7;
  }
  bytes_.push_back(static_cast<uint8_t>(v));
  spill();
}

void
DbWriter::putI64(int64_t v)
{
  // Zigzag so that small negatives stay short.
  putU64((static_cast<uint64_t>(v) << 1) ^ static_cast<uint64_t>(v >> 63));
}

void
DbWriter::putF32(float v)
{
  putBytes(&v, sizeof(v));
}

void
DbWriter::putF64(double v)
{
  putBytes(&v, sizeof(v));
}

void
DbWriter::putStr(std::string_view str)
{
  if (strings_ == nullptr)
    throw DbCorrupt("stadb writer has no string table");
  putU32(strings_->intern(str).id);
}

void
DbWriter::putBytes(const void *data, size_t size)
{
  const uint8_t *bytes = static_cast<const uint8_t*>(data);
  if (sink_ && size >= block_size_) {
    // Too big to be worth buffering, such as a spliced sub-buffer.
    flush();
    sink_->write(bytes, size);
    return;
  }
  bytes_.insert(bytes_.end(), bytes, bytes + size);
  spill();
}

void
DbWriter::putBlob(const void *data, size_t size)
{
  putU64(size);
  putBytes(data, size);
}

////////////////////////////////////////////////////////////////

DbReader::DbReader(const uint8_t *data,
                   size_t size,
                   DbStringTable *strings) :
  data_(data),
  size_(size),
  pos_(0),
  strings_(strings)
{
}

void
DbReader::require(size_t count) const
{
  // Subtraction, because a corrupt length can be large enough that pos_ + count
  // wraps and lands back inside the section.
  if (count > size_ - pos_)
    throw DbCorrupt(sta::format("stadb section truncated at offset {}", pos_));
}

size_t
DbReader::getCount(const char *what)
{
  uint64_t count = getU64();
  // Every element costs at least one byte, so a count larger than the bytes
  // left cannot be honest. Checking here means a crafted file is rejected
  // before the count reaches a reserve, instead of after the allocator has
  // already tried to find room for it.
  if (count > remaining())
    throw DbCorrupt(sta::format("stadb {} count {} exceeds the {} bytes left "
                                "in the section", what, count, remaining()));
  return static_cast<size_t>(count);
}

uint8_t
DbReader::getU8()
{
  require(1);
  return data_[pos_++];
}

uint64_t
DbReader::getU64()
{
  uint64_t result = 0;
  int shift = 0;
  while (true) {
    uint8_t byte = getU8();
    result |= static_cast<uint64_t>(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0)
      break;
    shift += 7;
    if (shift >= 64)
      throw DbCorrupt("stadb varint overflow");
  }
  return result;
}

uint16_t
DbReader::getU16()
{
  return static_cast<uint16_t>(getU64());
}

uint32_t
DbReader::getU32()
{
  return static_cast<uint32_t>(getU64());
}

int64_t
DbReader::getI64()
{
  uint64_t encoded = getU64();
  return static_cast<int64_t>(encoded >> 1) ^ -static_cast<int64_t>(encoded & 1);
}

int32_t
DbReader::getI32()
{
  return static_cast<int32_t>(getI64());
}

uint32_t
DbReader::getRawU32()
{
  uint32_t v;
  getBytes(&v, sizeof(v));
  return v;
}

uint64_t
DbReader::getRawU64()
{
  uint64_t v;
  getBytes(&v, sizeof(v));
  return v;
}

float
DbReader::getF32()
{
  float v;
  getBytes(&v, sizeof(v));
  return v;
}

double
DbReader::getF64()
{
  double v;
  getBytes(&v, sizeof(v));
  return v;
}

std::string_view
DbReader::getStr()
{
  if (strings_ == nullptr)
    throw DbCorrupt("stadb reader has no string table");
  return strings_->string(DbStrId(getU32()));
}

const char *
DbReader::getCstring()
{
  if (strings_ == nullptr)
    throw DbCorrupt("stadb reader has no string table");
  return strings_->cstring(DbStrId(getU32()));
}

void
DbReader::getBytes(void *data, size_t size)
{
  require(size);
  std::memcpy(data, data_ + pos_, size);
  pos_ += size;
}

std::string_view
DbReader::getBlob()
{
  size_t size = getU64();
  require(size);
  std::string_view blob(reinterpret_cast<const char*>(data_ + pos_), size);
  pos_ += size;
  return blob;
}

void
DbReader::checkFullyConsumed(const char *what) const
{
  if (pos_ != size_)
    throw DbCorrupt(sta::format("stadb {} section has {} trailing bytes",
                                what, size_ - pos_));
}

} // namespace sta
