# Contributing to NetGuard by Mashatir

Thank you for helping improve NetGuard’s privacy, reliability, and accessibility.

## Before contributing

- Use ESP-IDF v5.2.5 and target `esp32s3`.
- Never commit real Wi-Fi credentials, passwords, tokens, private keys, or personal DNS logs.
- Do not add cloud telemetry or browsing-history upload.
- Preserve global-policy precedence: profile rules cannot weaken a global block or schedule.

## Workflow

1. Discuss substantial work in an issue first.
2. Keep each branch and pull request focused.
3. Run `idf.py build`; test relevant dashboard and DNS behaviour on hardware when possible.
4. State the privacy/security impact, tests performed, and router assumptions in the pull request.
5. Update docs and `CHANGELOG.md` for user-visible changes.

By contributing, you license your contribution under the repository [LICENSE](LICENSE).
