# Document Review Report — database_system

**Generated**: 2026-04-14
**Mode**: Report-only
**Files analyzed**: 80

Scope: All `*.md` files under `/Volumes/T5 EVO/Sources/database_system` excluding `build/`, `.git/`, `vcpkg-*/`, and `docs/doxygen-awesome-css/`. Primary tree is `docs/`; root `README*`, `CHANGELOG*`, `CODE_OF_CONDUCT*`, `CONTRIBUTING*`, `SECURITY*`, `.github/**`, `benchmarks/README.md`, `integration_tests/README.md`, `samples/README.md`, `examples/README.md` also covered. Korean `*.kr.md` files included.

Ground-truth anchors: 2,492. Links validated (excluding external URLs): 1,115. Broken links: 73 (71 missing files, 2 missing anchors).

## Findings Summary

| Severity       | Phase 1 | Phase 2 | Phase 3 | Total |
|----------------|---------|---------|---------|-------|
| Must-Fix       | 73      | 14      | 10      | 97    |
| Should-Fix     | 3       | 12      | 12      | 27    |
| Nice-to-Have   | 2       | 4       | 6       | 12    |

## Must-Fix Items

### Phase 1 — Broken Links & Anchors (73)

Intra-anchor mismatches:
1. `README.md:263` — `docs/FEATURES.md#query-builders`. Anchor `query-builders` does not exist in `FEATURES.md` (which is now a split index). Actual anchor is in `docs/FEATURES_ORM_QUERY.md#query-builders`. (Phase 1)
2. `README.md:293` — `docs/FEATURES.md#orm-framework`. Anchor is in `docs/FEATURES_ORM_QUERY.md#orm-framework`. (Phase 1)

Missing files referenced by links (71 total; grouped below):

3. `README.md:40` — `docs/migration/proxy-mode.md` not present. (Phase 1)
4. `README.md:153` — `docs/guides/QUICK_START_KO.md` not present (actual file is `QUICK_START.kr.md`). (Phase 1)
5. `README.md:508,581` — `docs/01-ARCHITECTURE.md` not present (should be `docs/ARCHITECTURE.md`). (Phase 1)
6. `README.md:563` — `../ECOSYSTEM.md` resolves outside repo (`/Volumes/T5 EVO/Sources/ECOSYSTEM.md`). Actual file is `docs/ECOSYSTEM.md`. (Phase 1)
7. `README.md:582` — `docs/02-API_REFERENCE.md` not present (should be `docs/API_REFERENCE.md`). (Phase 1)
8. `README.md:583` — `docs/advanced/SECURITY.md` not present. (Phase 1)
9. `README.md:584` — `docs/guides/MIGRATION_GUIDE.md` not present (should be `docs/advanced/MIGRATION.md`). (Phase 1)
10. `docs/ADAPTER_PATTERNS.md:15` — `ADAPTER_PATTERNS.kr.md` not present (Language switcher link broken). (Phase 1)
11. `docs/API_REFERENCE.md:1107` — `guides/PROXY_LAYER.md` not present. (Phase 1)
12. `docs/README.kr.md:66..258` — 21 broken links to `BUILD_GUIDE.kr.md`, `SAMPLES_GUIDE.kr.md`, `PERFORMANCE_BENCHMARKS.kr.md` at wrong paths (actual files live under `guides/` or `performance/`). (Phase 1)
13. `docs/advanced/MIGRATION.md:394` — `../migration/proxy-mode.md` not present. (Phase 1)
14. `docs/advanced/MIGRATION.md:869` — `docs/API_REFERENCE.md` resolves to `docs/advanced/docs/API_REFERENCE.md` (path prefix error; should be `../API_REFERENCE.md`). (Phase 1)
15. `docs/advanced/THREAD_SYSTEM_MIGRATION.md:398..399` — `./INTEGRATION.md`, `./API_REFERENCE.md` at wrong relative path. (Phase 1)
16. `docs/advanced/TYPE_SYSTEM.md:434..437` — `API_REFERENCE.md`, `ORM.md`, `BACKEND_INTEGRATION.md` at wrong relative path (files live in `../`). (Phase 1)
17. `docs/contributing/CONTRIBUTING.md:80,908` — `../BUILD_GUIDE.md` not present (should be `../guides/BUILD_GUIDE.md`). (Phase 1)
18. `docs/contributing/CONTRIBUTING.md:81,911` — `../../IMPROVEMENT_PLAN.md` not present in repo. (Phase 1)
19. `docs/guides/ASYNC_OPERATIONS.md:15` — `ASYNC_OPERATIONS.kr.md` not present. (Phase 1)
20. `docs/guides/FAQ.md:171,795,928,1126,1377,1490,1493,1494,1495` — 9 broken links (`../BUILD_GUIDE.md`, `../PERFORMANCE_BENCHMARKS.md`, `../INTEGRATION.md`, `../../IMPROVEMENT_PLAN.md`). (Phase 1)
21. `docs/guides/INTEGRATION.md:15` — `INTEGRATION.kr.md` not present. (Phase 1)
22. `docs/guides/INTEGRATION.md:1076..1077` — `docs/API_REFERENCE.md`, `docs/BUILD_GUIDE.md` resolved incorrectly. (Phase 1)
23. `docs/guides/QUICK_START.md:159` — `../../INTEGRATION.md` not present in repo root. (Phase 1)
24. `docs/guides/SAMPLES_GUIDE.md:797`, `docs/guides/SAMPLES_GUIDE.kr.md:797` — `API_REFERENCE.md` at wrong relative path (should be `../API_REFERENCE.md`). (Phase 1)
25. `docs/guides/TROUBLESHOOTING.md:915` — `../BUILD_GUIDE.md` not present. (Phase 1)
26. `docs/guides/UNIFIED_SYSTEM.md:15` — `UNIFIED_SYSTEM.kr.md` not present. (Phase 1)
27. `docs/integration/README.md:21..25` — 5 broken links to `with-common-system.md`, `with-logger.md`, `with-monitoring.md`, `with-thread-system.md`, `with-network-system.md` (none exist). (Phase 1)
28. `docs/integration/README.md:222` — `../../../ECOSYSTEM.md` resolves outside repo. (Phase 1)
29. `docs/migration/database_base.md:316` — `../guides/backend_registry.md` not present. (Phase 1)
30. `docs/performance/BENCHMARKS.kr.md:15` — `PERFORMANCE_BENCHMARKS.md` (language-switcher) not present; `docs/performance/BENCHMARKS.md:15` — same mirror issue for `.kr.md`. (Phase 1)
31. `docs/performance/TUNING_GUIDE.md:133..134` — `POSTGRESQL_TUNING.md`, `SQLITE_TUNING.md` not present. (Phase 1)
32. `integration_tests/README.md:285` — `../docs/performance.md` not present. (Phase 1)

### Phase 2 — Factual Accuracy (14 items)

33. `docs/advanced/CURRENT_STATE.md:30,36,54,74,84` — Documents claim backends are PostgreSQL, MySQL, SQLite. Actual supported backends per `CLAUDE.md` and `vcpkg.json` are PostgreSQL, SQLite, MongoDB, Redis. MySQL does not exist; MongoDB/Redis not mentioned. Korean mirror `CURRENT_STATE.kr.md` has identical errors. (Phase 2)
34. `docs/advanced/ARCHITECTURE_ISSUES.md:32..33` + Korean mirror — "ARC-001: MySQL/SQLite Backend Testing (P0, Phase 1)". MySQL is not a real backend. (Phase 2)
35. `docs/PROJECT_STRUCTURE.md:684`, `docs/PROJECT_STRUCTURE.kr.md:508` — libpqxx listed as "7.7+", but `vcpkg.json` pins 7.9.2. (Phase 2)
36. `docs/SOUP.md:43,86` — libpqxx listed as 7.9.0 rather than 7.9.2 (vcpkg override). (Phase 2)
37. `docs/GETTING_STARTED.md:24` — Compiler baseline given as GCC 12+/Clang 15+/MSVC 17.4+; `README.md:74` says GCC 13+/Clang 17+/MSVC 2022+. (Phase 2)
38. `docs/guides/QUICK_START.kr.md:30` — Compiler baseline says GCC 11+/Clang 14+; actual (per README) is GCC 13+/Clang 17+. (Phase 2)
39. `samples/README.md:212` — Compiler baseline GCC 10+/Clang 12+/MSVC 2019+; severely out of date. (Phase 2)
40. `CONTRIBUTING.md:17` — "GCC 13+, Clang 16+, MSVC 2022+" — Clang 16 inconsistent with README's Clang 17+. (Phase 2)
41. `docs/FEATURES_POOLING_SECURITY.md:35..160` — Documents active `connection_pool`, `connection_pool_v3`, `acquire_connection()` etc. as current features. Per `CLAUDE.md` Phase 4.3 and `docs/CHANGELOG.md:206,272`, all local pooling classes were **removed**. Entire "Connection Pooling" and "Resilient Connections" sections are factually obsolete. (Phase 2)
42. `docs/BACKENDS.md:41` — Lists "Connection pooling support" as a PostgreSQL feature (contradicts Phase 4.3 removal). (Phase 2)
43. `docs/FEATURES.kr.md` (lines 29, 412–) — Korean features doc still describes connection pooling as a current feature and was not updated when English `FEATURES.md` was split and pool removal was documented. (Phase 2)
44. `docs/ORM_GUIDE.md:15,80,250,413` and `docs/FEATURES_ORM_QUERY.md:39` — Claim "Full Support (C++17 SFINAE-based)". Project root `CLAUDE.md` states ORM uses "C++20 concepts". Inconsistency between implementation language claim and project description. (Phase 2)
45. `docs/guides/SAMPLES_GUIDE.md:750`, `docs/guides/SAMPLES_GUIDE.kr.md:750` — Hard-coded date literal `"2023-01-01"` in sample; acceptable as example data but mismatched with other 2026 dates (Nice-to-Have; surfaced here because paired with the 2025-12/2026-02 inconsistencies).
46. `docs/advanced/CURRENT_STATE.md:46..48` — Lists Ubuntu 22.04 (GCC 12, Clang 15) as the validated platform baseline. Given README's GCC 13+/Clang 17+ requirement, the baseline is out of date. (Phase 2)

### Phase 3 — SSOT Contradictions (10)

47. `docs/ARCHITECTURE.md:13` AND `docs/advanced/ARCHITECTURE.md:13` — Both claim "single source of truth for Database System Architecture". Duplicate SSOT with different doc_ids (DBS-ARCH-002 and DBS-ARCH-003). Content not reconciled. (Phase 3)
48. `docs/PROJECT_STRUCTURE.md:13` AND `docs/advanced/STRUCTURE.md:13` — Both SSOT for "Database System Project Structure" (DBS-PROJ-004 vs DBS-ARCH-006). Content overlap unresolved. (Phase 3)
49. `docs/BENCHMARKS.md:13` AND `docs/performance/BENCHMARKS.md:13` — Both SSOT for "Database System Performance Benchmarks" (DBS-PERF-002 vs DBS-PERF-006). Two parallel benchmark documents. (Phase 3)
50. `docs/BENCHMARKS.kr.md` AND `docs/performance/BENCHMARKS.kr.md` — Same duplication in Korean. (Phase 3)
51. `docs/MIGRATION_database_base.md:13` AND `docs/migration/database_base.md:13` — Two migration guides for the same `database_base → database_backend` topic (DBS-MIGR-001 vs DBS-GUID-022), both claiming SSOT. (Phase 3)
52. `docs/FEATURES.md` was refactored into split sub-documents (`FEATURES_BACKENDS.md`, `FEATURES_ORM_QUERY.md`, `FEATURES_POOLING_SECURITY.md`), but `docs/FEATURES.kr.md` was NOT split. Korean docs are out of sync with English SSOT structure. (Phase 3)
53. `docs/FEATURES_POOLING_SECURITY.md` documents connection pooling as current functionality while `README.md:40,52` and `docs/CHANGELOG.md:206,272` state it was removed in Phase 4.3. Cross-document SSOT contradiction on a load-bearing technical fact. (Phase 3)
54. `docs/BACKENDS.md:41` contradicts the same Phase 4.3 status (states pool support for PostgreSQL). (Phase 3)
55. `docs/README.md` lists 59 documents. Actual total is 80. Missing from the registry: `FEATURES_BACKENDS.md`, `FEATURES_ORM_QUERY.md`, `FEATURES_POOLING_SECURITY.md`, `API_QUICK_REFERENCE.md`, `ECOSYSTEM.md`, `GETTING_STARTED.md` plus all root/sub-README and .github templates. Registry is stale. (Phase 3)
56. `docs/README.kr.md:17-53` table of contents uses emoji-prefixed headings that produce anchors the ToC does not match (fixed by emoji-stripping in this audit), but the ToC was clearly authored without running it through a GitHub-style renderer — high risk of drift if headings change. (Phase 3)

## Should-Fix Items

### Phase 1
- `docs/README.kr.md` — many `.kr.md` links use bare filename root (`BUILD_GUIDE.kr.md`) but the target files live in `guides/` or `performance/`. Convert to relative paths.
- `docs/guides/INTEGRATION.md:1076..1077` — recurring pattern of `docs/API_REFERENCE.md`-style absolute references from within `docs/` subdirs. Standardize on relative paths.
- `docs/advanced/MIGRATION.md:869` — same absolute-from-docs pattern; replace with `../API_REFERENCE.md`.

### Phase 2 (Terminology)
- "backend" vs "driver" used interchangeably in `docs/advanced/CURRENT_STATE*.md:54` ("Database drivers"). Project convention is "backend" (per `CLAUDE.md` and `BACKENDS.md`).
- "connection pool" vs "connection pooling" vs "pool" — inconsistent noun usage across `FEATURES_POOLING_SECURITY.md` and `BACKENDS.md`.
- "ORM framework" vs "ORM" vs "entity system" — `ORM_GUIDE.md` uses "ORM framework"; other docs drop "framework".
- "query builder" vs "query_builder" — `API_REFERENCE.md` uses underscore namespace form; narrative docs use space form. Acceptable, but not called out.
- "transaction" vs "transaction management" — mixed.
- README.md:74 vs README.kr.md:77 — English gives GCC 13+/Clang 17+, Korean table gives "MSVC 2022+ | C++20 기능 필수" with no GCC/Clang row. Korean table is incomplete.
- `benchmarks/README.md:299` — "Last Updated: 2025-10-07" vs newer docs dated 2026-02-08. Stale timestamp.
- `docs/API_REFERENCE.md:1245` — "Last Updated: 2025-12-09" vs most docs 2026-02-08.
- `docs/ORM_GUIDE.md` — no frontmatter `doc_date` update; has 2026-04-04 but references say C++17 still.
- `docs/advanced/ARCHITECTURE.md` and `docs/advanced/STRUCTURE.md` — do not declare language-switcher links (present in most other English docs).
- `docs/SOUP.md:43` — libpqxx 7.9.0 should match override (7.9.2).
- `CHANGELOG.md:316` — "OpenSSL 1.1.1 reached End-of-Life in September 2023" is a correct historical fact; flagged only because it sits next to version rows that are otherwise current — verify vs current OpenSSL override (3.4.1).

### Phase 3 (Cross-refs / Redundancy)
- `docs/advanced/TYPE_SYSTEM.md` is referenced by only 1 other file. Consider adding bidirectional xref from `API_REFERENCE.md` or `ORM_GUIDE.md`.
- `docs/ADAPTER_PATTERNS.md` — only 4 inbound refs; `CLAUDE.md` cites "Adapter pattern" as a key pattern but `ARCHITECTURE.md` is the only SSOT linker. Strengthen xrefs from `FEATURES_BACKENDS.md`.
- `docs/ECOSYSTEM.md` is not in the registry index (`docs/README.md`) yet is cross-linked from README.md. Decide orphan-vs-registered status.
- `docs/API_QUICK_REFERENCE.md` and `docs/GETTING_STARTED.md` — orphan from registry; decide whether to add rows or remove files.
- `docs/FEATURES.kr.md` — not linked from `FEATURES.md` via "Korean" switcher despite same topic. Add language switch.
- `docs/FEATURES_BACKENDS.md`, `docs/FEATURES_ORM_QUERY.md`, `docs/FEATURES_POOLING_SECURITY.md` — no Korean counterparts; inconsistent with rest of docs which maintain `*.kr.md`.
- `docs/PROJECT_STRUCTURE.md` (root) vs `docs/advanced/STRUCTURE.md` (advanced) — same title, same SSOT claim, different structure. Pick one as canonical, redirect the other.
- `docs/ARCHITECTURE.md` (root) vs `docs/advanced/ARCHITECTURE.md` — redundant pair; reconcile and delete or retitle.
- `docs/BENCHMARKS.md` vs `docs/performance/BENCHMARKS.md` — pick SSOT and collapse the other into a stub linking to it.
- `docs/MIGRATION_database_base.md` vs `docs/migration/database_base.md` — collapse duplication.
- `docs/advanced/CURRENT_STATE.md` — "Phase 0 Baseline" document appears to be a frozen historical baseline. If so, flag status `Frozen` or `Baseline` rather than `Released` to prevent confusion; otherwise update to reflect real backends.
- `docs/integration/README.md` lists 5 sub-documents (`with-*.md`) none of which exist; either the sub-docs should be created or the ToC should be trimmed.

## Nice-to-Have Items

### Phase 1
- Normalize all emoji in heading anchors (GitHub strips emoji in slugs; manual ToCs can drift silently). Present in `README.md`, `docs/README.kr.md`, `docs/advanced/TYPE_SYSTEM.md`.
- Language switchers at the top of each English doc that references a `.kr.md` which doesn't exist are cosmetically broken — consider a lint-style CI check.

### Phase 2
- `docs/performance/STATIC_ANALYSIS_BASELINE.md:73,77` — "Phase 1 Goals (By 2025-11-01)" and "Phase 2 Goals (By 2025-12-01)" are past-dated; consider moving to a Completed table or archiving.
- `docs/performance/BASELINE.md:368..385` — version history ends 2025-10-09 though date is 2026-04-04; update.
- `docs/CHANGELOG.md:334` uses `## [Previous] - 2025-12-09`; keep-a-changelog style would prefer a concrete version header.
- Badge block at top of `README.md` uses relative branch URLs — fine; but "Code Coverage" badge's workflow file name (`coverage.yml`) should be verified to exist in `.github/workflows/`.

### Phase 3
- Consider adding a `docs/INDEX.md` that enumerates all 80 files (orphans included) and flags their role (registered/index/archive). Current `docs/README.md` only covers 59.
- Consider collapsing `docs/advanced/` content into `docs/` and demoting per-topic "advanced" duplicates — the split currently creates SSOT conflicts.
- All "SSOT" notice blocks should include either an authoritative doc_id or a line stating "if content here conflicts with X, X wins", so reviewers can break ties.
- Add a bidirectional-xref linter to CI (the 22 `.kr.md` TOC entries in `docs/README.kr.md` are the canonical repeat-error case).
- FrontMatter `doc_date` dates (2026-04-04) differ from in-body `Last Updated` fields (2025-xx-xx or 2026-02-08) — decide a single date-of-record field.
- `docs/adr/` only has 2 ADRs (multi-backend abstraction, connection pool) yet connection pooling was removed in Phase 4.3. Consider adding ADR-003 superseding ADR-002.

## Score

- **Overall**: 6.3 / 10
- **Anchors (Phase 1)**: 6 / 10 — 73 broken links out of 1,115; most are file-path errors (missing `.kr.md` mirrors, incorrect relative paths after folder restructuring, stale `01-`/`02-` numbered filenames).
- **Accuracy (Phase 2)**: 5 / 10 — Load-bearing technical claims are wrong (MySQL listed as a backend, C++17 SFINAE vs C++20 concepts, connection pool documented as active after Phase 4.3 removal, inconsistent compiler baselines across 5+ docs, libpqxx version drift).
- **SSOT (Phase 3)**: 5 / 10 — Multiple pairs of documents claim SSOT for identical topics (Architecture, Project Structure, Benchmarks, Migration, database_base). FEATURES split diverged between English (split) and Korean (monolithic). Registry (`docs/README.md`) is stale, missing 3 newly-split docs and 3 orphans.

## Notes

- Audit generated via a local Python slug-builder implementing GitHub's anchor rules (lowercase, strip emoji, keep Korean/CJK, replace single whitespace with hyphen, preserve consecutive hyphens from "&"/"/" removals, duplicate-suffix via `-N`). Fenced code blocks (``` / ~~~) are skipped during heading extraction and link scraping.
- External URLs (schemes with `:`) are excluded; only intra-repo links validated.
- `docs/doxygen-awesome-css/`, `docs/custom.css`, `*.dox`, `*.html` excluded from corpus.
- Phase 4.3 pool-removal is the single largest SSOT hazard: it affects `BACKENDS.md`, `FEATURES_POOLING_SECURITY.md`, `FEATURES.kr.md`, ADR-002, and multiple guide docs. Consolidating a single authoritative page (e.g., `docs/migration/proxy-mode.md` which is linked but absent) would close ~20% of the findings.
- The pattern `docs/xxx.md` used as a link inside a file located in `docs/advanced/` or `docs/guides/` (producing `docs/advanced/docs/xxx.md`) recurs at least 4 times. One sweep of relative-path repair would clear a cluster.
- Eight `.kr.md` language-switcher links target files that do not exist (`ADAPTER_PATTERNS.kr.md`, `ASYNC_OPERATIONS.kr.md`, `INTEGRATION.kr.md`, `UNIFIED_SYSTEM.kr.md`, `PERFORMANCE_BENCHMARKS.kr.md` x2, etc.). Either the Korean translations should be created or the switcher blocks removed.

## Post-Fix Re-Validation (2026-04-15)

**Fix commit**: `3b6f5be3` — `docs: fix 30 broken links, 11 factual errors, 5 ssot redundancies`
**Re-run scope**: Phase 1 only (anchors + intra/inter-file .md link validation)
**Files scanned**: 78 Markdown files (same corpus as 2026-04-14 audit; two files dropped in churn)
**Methodology**: Python re-validator using the same GitHub-slug rules (lowercase, strip emoji, strip markdown link syntax in headings, keep Korean/CJK, non-collapsing whitespace → hyphen so `&`/`/` removals preserve consecutive hyphens, duplicate-suffix `-N`). Fenced code blocks skipped; HTML comments (including multi-line) stripped before link extraction, so TODO markers left by the fix (`<!-- TODO: ... -->`) are not re-flagged.

### Before / After Summary

Link counts are occurrences (one broken link rendered twice counts twice).

| Metric                                         | Pre-Fix (2026-04-14) | Post-Fix (2026-04-15) | Delta |
|------------------------------------------------|----------------------|-----------------------|-------|
| Files scanned                                  | 80                   | 78                    | −2    |
| Total intra-repo links validated               | 989                  | 1,030                 | +41   |
| Broken links — `.md` scope (aligned w/ prior)  | **73**               | **23**                | −50   |
| Broken links — all scopes (incl. `.h`/dirs)    | 80                   | 26                    | −54   |
| Broken intra-file anchors                      | 2                    | 1                     | −1    |
| Broken file references                         | 78                   | 25                    | −53   |

Classification against the prior 73-item Phase-1 list:

| Category       | Count | Notes                                                                          |
|----------------|-------|--------------------------------------------------------------------------------|
| **Fixed**      | **51**| All fixes the commit message claims (30 "broken links") plus a larger set once each listed group is expanded to individual occurrences (e.g., item #12 was 21 links, item #20 was 9). |
| **Residual**   | **22**| 21 inside `docs/README.kr.md` + 1 inside `docs/guides/SAMPLES_GUIDE.kr.md`. All fall under the commit's explicit policy deferral: *"Skipped by policy: Korean (.kr.md) sync, ... docs/README.kr.md path corrections (Korean file)."* |
| **Regression** | **1** | New broken anchor introduced by the deprecation banner in `docs/FEATURES_POOLING_SECURITY.md`. |

### Regression Detail (1)

1. **`docs/FEATURES_POOLING_SECURITY.md:20` → `CHANGELOG.md#043---2025-12-09`** — *cross-file anchor not found*. The fix commit inserted a new "DEPRECATION NOTICE (Phase 4.3)" banner linking to a CHANGELOG section anchor. `docs/CHANGELOG.md` does not contain a heading that slugs to `043---2025-12-09`; the only matching date-bearing heading is `## [Previous] - 2025-12-09` (line 334), which slugs to `previous---2025-12-09`. Recommended fix: change link to `CHANGELOG.md#previous---2025-12-09` or refactor the CHANGELOG heading to `## [0.4.3] - 2025-12-09`.

### Residual Detail (22)

**A. `docs/README.kr.md` — 21 occurrences** (deferred by commit policy):
- 8 x `BUILD_GUIDE.kr.md` (lines 66, 73, 110, 157, 165, 168, 256, and one more on 168)
- 7 x `SAMPLES_GUIDE.kr.md` (lines 67, 74, 119, 158, 164, 175)
- 6 x `PERFORMANCE_BENCHMARKS.kr.md` (lines 68, 75, 128, 163, 169, 176, 258)
  — All three files live under `guides/` or `performance/`, not at `docs/` root. A single mass-rewrite to `guides/BUILD_GUIDE.kr.md`, `guides/SAMPLES_GUIDE.kr.md`, `performance/BENCHMARKS.kr.md` (filename also needs rename if the Korean match is desired) would clear this cluster.

**B. `docs/guides/SAMPLES_GUIDE.kr.md:797` → `API_REFERENCE.md`** — mirror of the fixed English item. The English `SAMPLES_GUIDE.md:797` was corrected to `../API_REFERENCE.md`; the Korean mirror was skipped.

**C. `docs/performance/BENCHMARKS.kr.md:15` → `PERFORMANCE_BENCHMARKS.md`** — language-switcher at the top of the Korean benchmarks page points at a non-existent English counterpart (actual file is `BENCHMARKS.md` in the same directory or `docs/BENCHMARKS.md`).

### Additional Observations (not counted in the 73-item Phase-1 total)

- `docs/MIGRATION_database_base.md:303..304` → `../database/database_base.h` and `../database/database_base_adapter.h`: these source-code references remain broken after the fix (the files were removed when `database_base` was deleted in 2026-01-20 per CHANGELOG). These were NOT in the original Phase-1 list (which was `.md`-only scope). The "See Also" block should either link to the historical commit or be trimmed.
- `docs/guides/INTEGRATION.md:1054` → `samples/integration_example/`: directory does not exist. Also pre-existing and outside the original Phase-1 `.md` scope.

### Verdict

**PASS with 1 regression**. 51 of 73 prior Phase-1 items were fixed — every Phase-1 item the commit message explicitly claimed (plus more once grouped items are expanded) is verified as resolved. All 22 residuals fall under the commit's documented policy deferral for Korean mirrors. The single regression (`FEATURES_POOLING_SECURITY.md → CHANGELOG.md#043---2025-12-09`) was introduced by the fix itself and should be corrected in a follow-up. Overall Phase-1 health is now **97% clean** (1 − 23/1030 ≈ 97.8% of validated links OK, vs 92.6% before).
