# Contract Gate Report

- Result: **FAIL**
- Fail-fast: `True`
- Draft policy: `warn`

| Gate | Stage | Result | Contracts |
|------|-------|--------|-----------|
| `GATE-META-CONTRACT-FILES` | `preflight` | `PASS` | C-IX-6-prime, C-IX-5, C-IX-8 |
| `GATE-META-REGISTRY` | `preflight` | `PASS` | C-IX-6-prime, C-IX-5 |
| `GATE-TEST-HERM` | `preflight` | `PASS` | C-TST-HERM-1 |
| `GATE-TEST-L1-GATE-SCOPE` | `preflight` | `PASS` | C-TST-GATE-SCOPE-1, C-TST-L1-1 |
| `GATE-TEST-TIER-ASSIGNMENT` | `preflight` | `PASS` | C-TST-POL-1 |
| `GATE-ARCH-INCLUDE-BOUNDARY` | `bootstrap` | `PASS` | C-IX-1, C-IX-2, C-IX-3, C-IX-7, C-FN-2 |
| `GATE-VENDOR-PARSE-BOUNDARY` | `bootstrap` | `PASS` | C-IX-7 |
| `GATE-STYLE-LAYERED` | `read` | `PASS` | C-ST-14 |
| `GATE-STYLE-PREFIX` | `read` | `PASS` | C-ST-1 |
| `GATE-APP-VENDOR-NORMALIZE` | `parse` | `PASS` | C-FN-4 |
| `GATE-DAY14` | `persist` | `PASS` | C-IX-4, C-CF-2 |
| `GATE-ATTACH-WATCH-CB` | `watch` | `PASS` | C-ATTACH-WATCH-CB-1 |
| `GATE-DAY15` | `watch` | `PASS` | C-IX-4, C-ST-13 |
| `GATE-ATTACH-SYNC-PROD` | `ci` | `PASS` | C-ATTACH-SYNC-1, C-ATTACH-SYNC-2, C-ATTACH-SYNC-3, C-ATTACH-SYNC-4, C-ATTACH-SYNC-5, C-ATTACH-SYNC-6 |
| `GATE-INTEGRATION` | `ci` | `FAIL` | C-IX-2, C-ST-13, C-FN-1, C-FN-3, C-CF-1 |
| `(stage:dev_ci)` | `dev_ci` | `SKIP` |  |
| `(stage:staging)` | `staging` | `SKIP` |  |
| `(stage:prod_ops)` | `prod_ops` | `SKIP` |  |
