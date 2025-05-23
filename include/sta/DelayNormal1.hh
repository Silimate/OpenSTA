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

#include "MinMax.hh"

namespace sta {

class Delay;
class DelayDbl;
class StaState;

// Normal distribution with std deviation.
class Delay
{
public:
  Delay();
  Delay(const Delay &delay);
  Delay(const DelayDbl &delay);
  Delay(float mean);
  Delay(float mean,
	float sigma2);
  float mean() const { return mean_; }
  float sigma() const;
  // sigma^2
  float sigma2() const;
  void operator=(const Delay &delay);
  void operator=(float delay);
  void operator+=(const Delay &delay);
  void operator+=(float delay);
  Delay operator+(const Delay &delay) const;
  Delay operator+(float delay) const;
  Delay operator-(const Delay &delay) const;
  Delay operator-(float delay) const;
  Delay operator-() const;
  void operator-=(float delay);
  void operator-=(const Delay &delay);
  bool operator==(const Delay &delay) const;

private:
  float mean_;
  // Sigma^2
  float sigma2_;

  friend class DelayDbl;
};

// Dwlay with doubles for accumulating delays.
class DelayDbl
{
public:
  DelayDbl();
  float mean() const { return mean_; }
  float sigma() const;
  // sigma^2
  float sigma2() const;
  void operator=(float delay);
  void operator+=(const Delay &delay);
  void operator-=(const Delay &delay);

private:
  double mean_;
  // Sigma^2
  double sigma2_;

  friend class Delay;
};

const Delay delay_zero(0.0);

void
initDelayConstants();

const char *
delayAsString(const Delay &delay,
	      const StaState *sta);
const char *
delayAsString(const Delay &delay,
	      const StaState *sta,
	      int digits);
const char *
delayAsString(const Delay &delay,
	      const EarlyLate *early_late,
	      const StaState *sta,
	      int digits);

Delay
makeDelay(float delay,
	  float sigma_early,
	  float sigma_late);

Delay
makeDelay2(float delay,
	   // sigma^2
	   float sigma_early,
	   float sigma_late);

inline float
delayAsFloat(const Delay &delay)
{
  return delay.mean();
}

// mean late+/early- sigma
float
delayAsFloat(const Delay &delay,
	     const EarlyLate *early_late,
	     const StaState *sta);
float
delaySigma2(const Delay &delay,
	    const EarlyLate *early_late);
const Delay &
delayInitValue(const MinMax *min_max);
bool
delayIsInitValue(const Delay &delay,
		 const MinMax *min_max);
bool
delayZero(const Delay &delay);
bool
delayInf(const Delay &delay);
bool
delayEqual(const Delay &delay1,
	   const Delay &delay2);
bool
delayLess(const Delay &delay1,
	  const Delay &delay2,
	  const StaState *sta);
bool
delayLess(const Delay &delay1,
	  const Delay &delay2,
	  const MinMax *min_max,
	  const StaState *sta);
bool
delayLessEqual(const Delay &delay1,
	       const Delay &delay2,
	       const StaState *sta);
bool
delayLessEqual(const Delay &delay1,
	       const Delay &delay2,
	       const MinMax *min_max,
	       const StaState *sta);
bool
delayGreater(const Delay &delay1,
	     const Delay &delay2,
	     const StaState *sta);
bool
delayGreaterEqual(const Delay &delay1,
		  const Delay &delay2,
		  const StaState *sta);
bool
delayGreaterEqual(const Delay &delay1,
		  const Delay &delay2,
		  const MinMax *min_max,
		  const StaState *sta);
bool
delayGreater(const Delay &delay1,
	     const Delay &delay2,
	     const MinMax *min_max,
	     const StaState *sta);

// delay1-delay2 subtracting sigma instead of addiing.
Delay delayRemove(const Delay &delay1,
		  const Delay &delay2);
float
delayRatio(const Delay &delay1,
	   const Delay &delay2);

// Most non-operator functions on Delay are not defined as member
// functions so they can be defined on floats, where there is no class
// to define them.

Delay operator+(float delay1,
		const Delay &delay2);
// Used for parallel gate delay calc.
Delay operator/(float delay1,
		const Delay &delay2);
// Used for parallel gate delay calc.
Delay operator*(const Delay &delay1,
		float delay2);

} // namespace
