// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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

#include "PatternMatch.hh"
#include "FilterExpr.hh"
#include <functional>


template <class SEQ_TYPE, class OBJECT_TYPE, int ERROR_CODE, const char * OBJECT_TYPE_NAME>
SEQ_TYPE *
find_objects_complete(SEQ_TYPE *collection,
                      StringSeq *patterns,
                      bool regexp,
                      bool nocase,
                      bool quiet,
                      const char *filter_expression,
                      std::function<SEQ_TYPE(PatternMatch *)> get_pattern)
{
  collection = collection ? new SEQ_TYPE(*collection) : new SEQ_TYPE();
  auto sta = Sta::sta();
  for (const char *pattern: *patterns) {
    PatternMatch matcher(pattern, regexp, nocase, sta->tclInterp());
    auto result = get_pattern(&matcher);
    if (result.size() == 0) {
        if (!quiet)
          sta->report()->warn(ERROR_CODE, "%s '%s' not found.", OBJECT_TYPE_NAME, pattern);
    } else {
      auto entries = collection->size();
      collection->resize(entries + result.size());
      std::move(
        result.begin(),
        result.end(),
        collection->begin() + entries
      );
    }
  }
  if (filter_expression != nullptr) {
    auto filtered = filter_objects<OBJECT_TYPE>(filter_expression, collection, sta->booleanPropsAsInt());
    delete collection;
    collection = new SEQ_TYPE(filtered);
  }
  return collection;
}
