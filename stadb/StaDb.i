// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
//
%{
#include "StaDb.hh"
#include "Sta.hh"
%}

%inline %{

void
write_sta_db_cmd(const char *filename)
{
  sta::writeStaDb(sta::Sta::sta(), filename);
}

void
read_sta_db_cmd(const char *filename)
{
  sta::readStaDb(sta::Sta::sta(), filename);
}

%}
