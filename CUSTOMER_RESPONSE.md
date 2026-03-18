Subject: RESOLVED - Memory Corruption Fix for Preqorsor Generated Clock Issue

Hi Nico,

Thank you for your patience in working through this issue. I've identified and fixed the root cause of the memory corruption crashes you were experiencing.

## Problem Summary

The crashes you saw with messages like:
- `free(): invalid pointer`
- `double free or corruption (out)`
- `malloc(): invalid next size (unsorted)`

Were caused by TWO critical bugs in OpenSTA's Clock.cc file:

### Bug 1: Double-Free in initGeneratedClk()
The code was incorrectly managing memory when empty edge lists were passed:
```cpp
delete edges_;
if (edges && edges->empty()) {
  delete edges;
  edges = nullptr;      // BUG: Setting local parameter, not member!
}
edges_ = edges;         // BUG: Assigning potentially deleted pointer
```

This caused the pointer to be deleted but then still used, leading to heap corruption.

### Bug 2: Missing Bounds Check in masterClkEdgeTr()
The function was accessing edges_[0] and edges_[1] without checking if the edges_ pointer was null or had enough elements, leading to out-of-bounds memory access.

## The Fix

I've applied three fixes:

1. **Corrected memory management** - Properly handle empty edge cases without double-free
2. **Added bounds checking** - Prevent null pointer and out-of-bounds access in masterClkEdgeTr()
3. **Relaxed edge restrictions** - Allow generated clocks with ≥3 edges (your original issue)

All fixes are now committed and available in the codebase.

## Files for Your Review

1. `MEMORY_CORRUPTION_FIX.md` - Detailed technical explanation of all bugs and fixes
2. `memory_corruption_fix.patch` - Patch file you can apply to verify/test

## Next Steps

1. Please test with your w_dig_top design using the updated code
2. Run with your full constraint files including PROJ_BUFLOW
3. If you have memory debugging tools (valgrind, AddressSanitizer), I recommend running a test to verify no remaining issues

The fixes should resolve all the crashes you were experiencing. Please let me know if you encounter any issues or have questions about the changes.

Best regards,
Akash

---

Technical Details:
- Files Modified: sdc/Clock.cc (lines 348-364, 492-502)
- Related Commits: Includes previous fix for "edges size is not three" error
- OpenSTA Version: Current master branch
