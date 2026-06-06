#ifndef BS_ADAPTER_ATTACH_ERRORS_H
#define BS_ADAPTER_ATTACH_ERRORS_H

/*
 * C-ST-7 contract block:
 * Thread safety: Error constants only; no mutable state.
 * Error semantics: BS_ATTACH_CONC_ERR_* attach-domain codes (XX-CONC-7); persist conflict uses
 * RES-IX-12. Platform notes: Distinct from BS_ATTACH_ERR_CONFLICT on persist boundary.
 */

#define BS_ATTACH_CONC_ERR_REENTRANT -301
#define BS_ATTACH_CONC_ERR_REVISION_CHANGED -302
#define BS_ATTACH_CONC_ERR_TOO_LARGE -303
#define BS_ATTACH_CONC_ERR_NOTIFY_TIMEOUT -304
#define BS_ATTACH_CONC_ERR_CLOSED -305
#define BS_ATTACH_CONC_ERR_READ_BLOCKED -306
#define BS_ATTACH_CONC_ERR_INVALID_HANDLE -307

#endif /* BS_ADAPTER_ATTACH_ERRORS_H */
