# Action Items Checklist

## Completed ✓

- [x] Identified double-free bug in `initGeneratedClk()`
- [x] Identified out-of-bounds access in `masterClkEdgeTr()`
- [x] Fixed double-free bug for both `edges_` and `edge_shifts_`
- [x] Added bounds checking in `masterClkEdgeTr()`
- [x] Verified previous fix in `generateEdgesClk()` is correct
- [x] Checked code for linter errors
- [x] Created patch file (`memory_corruption_fix.patch`)
- [x] Wrote technical documentation (`MEMORY_CORRUPTION_FIX.md`)
- [x] Wrote complete fix summary (`COMPLETE_FIX_SUMMARY.md`)
- [x] Wrote customer response (`CUSTOMER_RESPONSE.md`)
- [x] Created test script (`test_memory_corruption_fix.tcl`)
- [x] Created README (`README_MEMORY_FIX.md`)

## Pending - Before Commit

- [ ] Run the test script to ensure basic functionality works
- [ ] Build and test with AddressSanitizer if available
- [ ] Review changes one more time
- [ ] Commit changes with detailed commit message
- [ ] Push to repository

## Pending - Customer Communication

- [ ] Send `CUSTOMER_RESPONSE.md` to Nico
- [ ] Wait for customer testing feedback with w_dig_top design
- [ ] Provide support if any issues arise

## Pending - Follow-up Work

- [ ] Consider relaxing TCL validation in `sdc/Sdc.tcl:1108-1110` to allow 3+ edges
- [ ] Add comprehensive unit tests for edge cases:
  - [ ] Generated clocks with 0, 1, 2 edges (should warn/error)
  - [ ] Generated clocks with 3 edges (should work)
  - [ ] Generated clocks with 4+ edges (should work, use first 3)
  - [ ] Empty IntSeq/FloatSeq parameters
  - [ ] Null edges_ pointer cases
  - [ ] Liberty files with various edge configurations
- [ ] Run full regression test suite
- [ ] Memory audit with Valgrind on full test suite
- [ ] Update OpenSTA documentation if needed

## Optional - Future Enhancements

- [ ] Add debug logging for edge handling
- [ ] Add assertions to catch similar bugs early
- [ ] Consider adding edge count validation in other locations
- [ ] Review other uses of IntSeq/FloatSeq for similar patterns

## Notes

### Commit Message Template
```
Fix memory corruption bugs in generated clock handling

Fixes three critical bugs causing crashes with "free(): invalid pointer"
and "double free or corruption" errors:

1. Double-free bug in Clock::initGeneratedClk()
   - Fixed incorrect handling of empty edges/edge_shifts parameters
   - Previously set local parameter instead of member variable
   
2. Out-of-bounds access in Clock::masterClkEdgeTr()
   - Added null and bounds checking before accessing edges_[0] and edges_[1]
   - Returns safe fallback value when edges unavailable

3. Relaxed edge count restriction in Clock::generateEdgesClk()
   - Changed from requiring exactly 3 edges to allowing ≥3 edges
   - Uses first 3 edges when more are provided
   - Changed from critical error to warning

These bugs were exposed when generated clocks with unusual edge
configurations were used, particularly in Liberty file definitions
and complex constraint files.

Fixes issue reported by Nico from Marvell with w_dig_top design.

Files modified:
- sdc/Clock.cc (initGeneratedClk, masterClkEdgeTr, generateEdgesClk)

Testing:
- Verified no linter errors
- Created test script for validation
- Pending: Customer testing with w_dig_top design
- Recommended: Memory sanitizer testing
```

### Files to Include in Commit
- sdc/Clock.cc (modified)
- memory_corruption_fix.patch (new)
- MEMORY_CORRUPTION_FIX.md (new)
- COMPLETE_FIX_SUMMARY.md (new)
- CUSTOMER_RESPONSE.md (new)
- test_memory_corruption_fix.tcl (new)
- README_MEMORY_FIX.md (new)
- ACTION_ITEMS.md (this file, new)
