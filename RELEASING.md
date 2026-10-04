# Releasing MsPacmanRipper

This checklist defines the release process for maintainers.

## 1. Prepare main

Before tagging a release:

- merge all intended changes to `main`;
- ensure `git diff --check` is clean;
- ensure no ROM/PROM or generated ROM-derived material is tracked;
- update `CHANGELOG.md`;
- update `RELEASE_NOTES.md`;
- update `README.md` if installation/support changed;
- update `TEST_REPORT.md` for new verification claims.

## 2. Keep the version consistent

The release version must match in:

- `VERSION`;
- `src/Version.h`;
- `README.md`;
- `RELEASE_NOTES.md`;
- `CHANGELOG.md`.

The release-readiness workflow checks these automatically.

## 3. Require green CI

All current release gates must pass on the intended commit:

- Release readiness;
- Windows build/package;
- macOS Apple Silicon;
- macOS Intel;
- macOS universal package;
- Docker linux/amd64;
- Docker linux/arm64;
- Docker multi-architecture publish.

For a release that changes analysis semantics, also run the canonical private/local regression documented in `TEST_REPORT.md`.

## 4. Tag

Tags use:

```text
vMAJOR.MINOR.PATCH
```

Example:

```bash
git tag -a v1.3.0 -m "MsPacmanRipper 1.3.0"
git push origin v1.3.0
```

The tag version must equal `VERSION`. Platform workflows are configured to run for `v*` tag pushes.

## 5. Collect artifacts

From the tag's successful workflow runs, collect:

- Windows runnable package;
- macOS universal package;
- Linux multi-architecture OCI archive.

The Docker workflow also publishes:

```text
ghcr.io/hodgkinsstudios/mspacmanripper:<version>
ghcr.io/hodgkinsstudios/mspacmanripper:latest
```

Verify checksums before attaching release assets.

## 6. Create the GitHub release

Create the GitHub release from the annotated tag, use `RELEASE_NOTES.md` as the basis for the release description, and attach the platform artifacts/checksums.

Do not attach:

- ROM/PROM files;
- generated disassembly output;
- reconstructed game binaries;
- private signing credentials.

## 7. Final verification

After publishing:

- confirm the release page points to the intended tag;
- confirm Windows/macOS artifacts download successfully;
- confirm the GHCR version tag resolves to both linux/amd64 and linux/arm64;
- confirm README quick-start commands reference the released version;
- confirm `latest` points to the intended current container release.
