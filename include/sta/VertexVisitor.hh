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

#include "NetworkClass.hh"
#include "GraphClass.hh"

namespace sta {

// Abstract base class for visiting a vertex.
class VertexVisitor
{
public:
  VertexVisitor() {}
  virtual ~VertexVisitor() {}
  virtual VertexVisitor *copy() const = 0;
  virtual void visit(Vertex *vertex) = 0;
  void operator()(Vertex *vertex) { visit(vertex); }
  virtual void levelFinished() {}
};

// Collect visited pins into a PinSet.
class VertexPinCollector : public VertexVisitor
{
public:
  VertexPinCollector(PinSet &pins);
  const PinSet &pins() const { return pins_; }
  void visit(Vertex *vertex);
  virtual VertexVisitor *copy() const;

protected:
  PinSet &pins_;
};

} // namespace
