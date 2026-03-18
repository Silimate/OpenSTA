# Memory Corruption Fix for OpenSTA Generated Clocks

This directory contains fixes for critical memory corruption bugs in OpenSTA's generated clock handling that caused crashes with `free(): invalid pointer` and `double free or corruption` errors.

## Quick Start

### Apply the Fix
```bash
git apply memory_corruption_fix.patch
```

### Verify the Fix
```bash
# Rebuild OpenSTA
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run test script
./sta -f ../test_memory_corruption_fix.tcl
```

## Bug Summary

Three critical issues were identified and fixed:

1. **Double-Free Bug** (`initGeneratedClk`)
   - Incorrectly managed memory for empty edge lists
   - Caused heap corruption and invalid pointer errors

2. **Out-of-Bounds Access** (`masterClkEdgeTr`)
   - Missing null/bounds checks before accessing edges_[0] and edges_[1]
   - Caused memory access violations

3. **Too-Restrictive Edge Count** (`generateEdgesClk`)
   - Required exactly 3 edges, rejected valid generated clocks with more
   - Changed to allow ≥3 edges, using first 3

## Documentation Files

| File | Purpose |
|------|---------|
| `memory_corruption_fix.patch` | Git patch with all fixes |
| `MEMORY_CORRUPTION_FIX.md` | Detailed technical analysis |
| `COMPLETE_FIX_SUMMARY.md` | Comprehensive timeline and fix summary |
| `CUSTOMER_RESPONSE.md` | Customer-facing explanation |
| `test_memory_corruption_fix.tcl` | Test script to verify fix |
| `README_MEMORY_FIX.md` | This file |

## Modified Files

- `sdc/Clock.cc`
  - Lines 348-364: Fixed double-free in `initGeneratedClk()`
  - Lines 454-483: Relaxed edge count check in `generateEdgesClk()`
  - Lines 492-502: Added bounds checking in `masterClkEdgeTr()`

## Testing

### Basic Testing
```bash
# Run the included test script
./sta -f test_memory_corruption_fix.tcl
```

### Memory Safety Testing
```bash
# Build with AddressSanitizer
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..
make
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 ./sta -f test.tcl

# Or use Valgrind
valgrind --leak-check=full --show-leak-kinds=all \
  ./sta -f test.tcl
```

### Regression Testing
Test with real designs that previously crashed:
1. Marvell's w_dig_top design
2. Any design with complex generated clock constraints
3. Liberty files with non-standard generated clock definitions

## Impact

**Before Fix:**
- Crashes during constraint file reading
- `free(): invalid pointer` errors
- `double free or corruption` errors
- `malloc(): invalid next size` errors

**After Fix:**
- Safely handles all generated clock configurations
- No memory corruption
- Proper error handling for edge cases

## Customer Context

- **Reporter:** Nico from Marvell
- **Tool:** Preqorsor v2.1.0-2.2.3
- **Design:** w_dig_top
- **Date:** March 10-13, 2026

## Additional Notes

### TCL Validation
The TCL command `create_generated_clock` still enforces exactly 3 edges (see `sdc/Sdc.tcl:1108`). This is stricter than necessary now that the C++ code handles any number ≥3. Consider relaxing the TCL check in a future update.

### Liberty Files
Liberty files can define generated clocks with arbitrary edge counts, which bypass TCL validation. The C++ fixes ensure these are handled safely.

## Questions?

For technical questions about the fix:
- See `MEMORY_CORRUPTION_FIX.md` for detailed analysis
- See `COMPLETE_FIX_SUMMARY.md` for complete timeline

For customer communication:
- See `CUSTOMER_RESPONSE.md`

---

**Version:** 1.0  
**Date:** March 17, 2026  
**Author:** Akash Levy (akash@silimate.com)
