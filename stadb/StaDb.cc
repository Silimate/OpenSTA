// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

// Serialize / deserialize a full STA session to one .stadb file.
//
// File layout:
//   [u32 version][u32 section_count]
//   repeated: [u32 name_length][name][u64 size][bytes]
//
// Each section is a db block, the same string table plus body encoding the
// liberty database uses:
//   liberty.N  one NLDM liberty library, LibDb encoding
//   network    the linked netlist: cells, instances, nets, pins, NetDb encoding
//   sdc        constraints
//   parasitics the RC networks read from spef, ParaDb encoding
//   anno       every delay and slew in the timing graph, AnnoDb encoding
//
// Sections are stored and loaded in this order because each one refers back to
// the ones before it: the netlist names liberty cells, the constraints name
// netlist objects, and the annotations name pins.

#include "StaDb.hh"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <tcl.h>

#include "AnnoDb.hh"
#include "Error.hh"
#include "Format.hh"
#include "Liberty.hh"
#include "MinMax.hh"
#include "NetDb.hh"
#include "Network.hh"
#include "ParaDb.hh"
#include "Report.hh"
#include "Sta.hh"
#include "Stats.hh"
#include "Units.hh"
#include "liberty/LibDb.hh"

namespace sta {

namespace fs = std::filesystem;

// One named chunk of bytes in the file.
struct StaDbSection
{
  std::string name;
  std::vector<uint8_t> bytes;
};

////////////////////////////////////////////////////////////////
// Low-level file I/O.

static void
writeSections(std::string_view filename,
              const std::vector<StaDbSection> &sections,
              Report *report)
{
  FILE *f = fopen(std::string(filename).c_str(), "wb");
  if (f == nullptr)
    report->error(1384, "cannot open {} for writing.", filename);

  auto write = [&](const void *data, size_t size, bool &ok) {
    ok = ok && (size == 0 || fwrite(data, size, 1, f) == 1);
  };
  bool ok = true;
  uint32_t version = sta_db_version;
  uint32_t count = static_cast<uint32_t>(sections.size());
  write(&version, sizeof version, ok);
  write(&count, sizeof count, ok);
  for (const StaDbSection &section : sections) {
    uint32_t name_length = static_cast<uint32_t>(section.name.size());
    uint64_t size = section.bytes.size();
    write(&name_length, sizeof name_length, ok);
    write(section.name.data(), section.name.size(), ok);
    write(&size, sizeof size, ok);
    write(section.bytes.data(), section.bytes.size(), ok);
  }
  fclose(f);

  if (!ok)
    report->error(1385, "error writing {}.", filename);
}

static std::vector<StaDbSection>
readSections(std::string_view filename,
             Report *report)
{
  FILE *f = fopen(std::string(filename).c_str(), "rb");
  if (f == nullptr)
    report->error(1387, "cannot open {}.", filename);

  bool ok = true;
  auto read = [&](void *data, size_t size) {
    ok = ok && (size == 0 || fread(data, size, 1, f) == 1);
    return ok;
  };

  uint32_t version = 0;
  uint32_t count = 0;
  if (!read(&version, sizeof version) || !read(&count, sizeof count)) {
    fclose(f);
    report->error(1388, "{} is truncated.", filename);
  }
  if (version != sta_db_version) {
    fclose(f);
    report->error(1389, "{} is sta database version {}, expected {}.",
                  filename, version, sta_db_version);
  }

  std::vector<StaDbSection> sections;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t name_length = 0;
    uint64_t size = 0;
    StaDbSection section;
    if (read(&name_length, sizeof name_length)) {
      section.name.resize(name_length);
      if (read(section.name.data(), name_length)
          && read(&size, sizeof size)) {
        section.bytes.resize(static_cast<size_t>(size));
        read(section.bytes.data(), section.bytes.size());
      }
    }
    if (!ok) {
      fclose(f);
      report->error(1388, "{} is truncated.", filename);
    }
    sections.push_back(std::move(section));
  }
  fclose(f);
  return sections;
}

static const StaDbSection *
findSection(const std::vector<StaDbSection> &sections,
            std::string_view name)
{
  for (const StaDbSection &section : sections) {
    if (section.name == name)
      return &section;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////

static void
readLibertySection(Sta *sta,
                   const StaDbSection &section)
{
  // Same bookkeeping read_liberty does once the library object exists: hand it
  // to the scene, and let the first library define the defaults.
  Network *network = sta->network();
  LibertyLibrary *library = readLibDbBytes(section.bytes.data(),
                                           section.bytes.size(),
                                           section.name, network);
  if (library) {
    sta->readLibertyAfter(library, sta->cmdScene(), MinMaxAll::all());
    network->readLibertyAfter(library);
    if (network->defaultLibertyLibrary() == nullptr) {
      network->setDefaultLibertyLibrary(library);
      *sta->units() = *library->units();
    }
  }
}

////////////////////////////////////////////////////////////////
// Constraints still round trip as sdc text, which the tcl commands read and
// write through files, so it is staged in a temp file on the way past.

static fs::path
tempSdcPath(Report *report)
{
  std::error_code ec;
  fs::path dir = fs::temp_directory_path(ec);
  if (ec)
    report->error(1390, "cannot find a temp directory.");
  auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return dir / sta::format("sta_db_{}.sdc", now);
}

static std::vector<uint8_t>
sdcBytes(Sta *sta,
         Report *report)
{
  fs::path path = tempSdcPath(report);
  sta->writeSdc(sta->cmdSdc(), path.string(), false, true, 4, false, true);
  std::ifstream in(path, std::ios::binary);
  if (!in)
    report->error(1380, "cannot open {}.", path.string());
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
  in.close();
  std::error_code ec;
  fs::remove(path, ec);
  return bytes;
}

static void
readSdcBytes(Sta *sta,
             const StaDbSection &section,
             Report *report)
{
  fs::path path = tempSdcPath(report);
  {
    std::ofstream out(path, std::ios::binary);
    if (!out)
      report->error(1392, "cannot write {}.", path.string());
    out.write(reinterpret_cast<const char *>(section.bytes.data()),
              static_cast<std::streamsize>(section.bytes.size()));
  }
  std::string command = sta::format("sta::include_file {{{}}} 0 0",
                                    path.string());
  int status = Tcl_Eval(sta->tclInterp(), command.c_str());
  std::error_code ec;
  fs::remove(path, ec);
  if (status != TCL_OK)
    report->error(1404, "failed to read the constraints section.");
}

////////////////////////////////////////////////////////////////

void
writeStaDb(Sta *sta,
           std::string_view filename)
{
  Report *report = sta->report();
  Network *network = sta->network();
  Stats stats(sta->debug(), report);

  if (!filename.ends_with(".stadb"))
    report->error(1381, "{} must end with .stadb.", filename);
  if (network->topInstance() == nullptr)
    report->error(1395, "design is not linked; cannot write sta database.");

  std::vector<StaDbSection> sections;
  // Liberty first, in load order, so the default library stays the default.
  int liberty_count = 0;
  LibertyLibraryIterator *lib_iter = network->libertyLibraryIterator();
  while (lib_iter->hasNext()) {
    LibertyLibrary *library = lib_iter->next();
    sections.push_back({sta::format("liberty.{}", liberty_count++),
                        writeLibDbBytes(library, report)});
  }
  delete lib_iter;
  if (liberty_count == 0)
    report->error(1396, "no liberty libraries loaded; cannot write sta database.");

  sections.push_back({"network", writeNetDbBytes(network, report)});
  sections.push_back({"sdc", sdcBytes(sta, report)});
  sections.push_back({"parasitics", writeParaDbBytes(sta)});
  // The delay section is a snapshot of the graph, so the delays have to be
  // there to snapshot. Parasitics and derating are baked in at this point.
  sta->ensureGraph();
  sta->findDelays();
  sections.push_back({"anno", writeAnnoDbBytes(sta)});

  writeSections(filename, sections, report);
  stats.report("Wrote sta database");
}

void
readStaDb(Sta *sta,
          std::string_view filename)
{
  Report *report = sta->report();
  Stats stats(sta->debug(), report);

  if (!filename.ends_with(".stadb"))
    report->error(1386, "{} must end with .stadb.", filename);
  std::vector<StaDbSection> sections = readSections(filename, report);

  // Drop the constraints and graph of whatever session is loaded, the same
  // way linking a new design does. Liberty libraries survive this.
  sta->clear();

  for (const StaDbSection &section : sections) {
    if (section.name.starts_with("liberty."))
      readLibertySection(sta, section);
  }

  const StaDbSection *network_section = findSection(sections, "network");
  if (network_section == nullptr)
    report->error(1398, "{} is missing the network section.", filename);
  NetworkReader *network = dynamic_cast<NetworkReader *>(sta->network());
  if (network == nullptr)
    report->error(1402, "{} cannot be loaded into this network.", filename);
  readNetDbBytes(network_section->bytes.data(), network_section->bytes.size(),
                 network_section->name, network);

  const StaDbSection *sdc_section = findSection(sections, "sdc");
  if (sdc_section == nullptr)
    report->error(1399, "{} is missing the sdc section.", filename);
  readSdcBytes(sta, *sdc_section, report);

  // Parasitics name nets and pins, so they follow the netlist. Older files
  // without this section still load; their delays come back either way.
  const StaDbSection *para_section = findSection(sections, "parasitics");
  if (para_section)
    readParaDbBytes(para_section->bytes.data(), para_section->bytes.size(),
                    para_section->name, sta);

  // Annotations name graph edges, so the graph has to exist to hang them on.
  const StaDbSection *anno_section = findSection(sections, "anno");
  if (anno_section == nullptr)
    report->error(1400, "{} is missing the annotation section.", filename);
  sta->ensureGraph();
  readAnnoDbBytes(anno_section->bytes.data(), anno_section->bytes.size(),
                  anno_section->name, sta);
  sta->arrivalsInvalid();

  stats.report("Read sta database");
}

} // namespace sta
