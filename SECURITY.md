# Security Policy

## Supported versions

Security fixes are provided for the current major release line.

| Version | Supported |
|---|---|
| `3.x` | Yes |
| `2.x` and earlier | No |

Users of an unsupported release should reproduce the issue on the latest v3
release before reporting it when practical.

## Reporting a vulnerability

Do not open a public issue for a suspected vulnerability. Email
`info@thymos.cz` with:

- affected version, target, framework, and transport backend;
- a concise description and potential impact;
- reproduction steps or a minimal test case;
- whether physical access or unusual hardware conditions are required; and
- any suggested mitigation, if known.

Do not include credentials, private keys, or unrelated device data. Use a
minimal fixture and avoid destructive testing on production hardware.

The maintainer will coordinate validation and disclosure privately. No fixed
response-time or embargo SLA is promised by this community project.

## Scope notes

This driver does not implement network protocols, authentication, encryption,
or firmware update. Relevant reports can still include memory-safety issues,
unbounded blocking, incorrect validation that permits unsafe register writes,
or transport behavior that violates the documented timeout/ownership contract.
Application bus adapters, board wiring, and higher-level recovery policy remain
application responsibilities, but cross-boundary issues are welcome when the
library contributes to the impact.
