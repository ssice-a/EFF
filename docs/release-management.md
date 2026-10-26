# Release Management

This document defines the release path for the EFF source repository and its
Blender and AnimeStudio submodules. The repositories have independent histories
and tags; a parent gitlink records an exact submodule commit, not a branch head.

## Release Tracker

Open one **Release readiness** issue in EFF for each coordinated release. Put
the exact commit SHA for each repository in that issue and link the CI runs and
release pages there. Treat a release as one tracked work item with these states:

1. **Preparing**: source changes and version numbers are being finalized in each
   repository.
2. **Pinned**: child repository commits are pushed; EFF gitlinks and
   `native-base.json` point at those exact commits and artifacts.
3. **Validated**: tests for all exact tagged commits pass, and the prepared
   native base matches both its committed receipt and the production DLL
   protocol.
4. **EFF published**: the EFF release contains the DLL and the verified native
   base.
5. **Blender published**: only after the EFF native-base URL is available, the
   Blender stable release is manually dispatched and its full package passes.
6. **Closed**: public asset URLs, file sizes, and SHA256 values have been checked
   and recorded in the issue.

GitHub Issues is the source-of-truth tracker. GitHub Projects V2 is optional;
release correctness must not depend on a personal project board or an
unauthenticated local `gh` session.

VS Code's **Source Control** view tracks the local Git worktree and index: it
shows changed files and supports staging, committing, and syncing. It does not
track cross-repository release dependencies, CI run IDs, or publication gates;
those belong in the EFF release issue and the checks below. VS Code's GitHub
sign-in and the `gh` CLI authentication are separate credentials, so an
authenticated VS Code session does not imply that `gh auth status` will pass.

## Normal Release

1. Finish and commit Blender/AnimeStudio changes in their own repositories.
   Push their commits, then update the EFF gitlinks and commit the parent
   repository. Do not create tags from dirty trees.
2. Set version numbers and release notes in the exact commits. Create/push the
   Blender tag first so its CI can run. The stable-release workflow does not run
   on tag push; it is dispatched later after EFF publishes its paired native
   base.
3. If Blender's `native-base.json` points to an already published EFF asset and
   the native compiler protocol is unchanged, keep that immutable receipt. The
   EFF release workflow downloads it, checks size/SHA256 and checks its
   `package-protocol.json` against the newly built production DLL. It does not
   rebuild the native archive for every DLL release.
4. Run EFF's `Build and Release` workflow for the exact EFF tag. The workflow
   runs the production and application tests before creating a draft. Verify
   both `EFF_<tag>_dll.zip` and `EFF_Blender_Native_*.zip` exist, then publish
   the EFF release.
5. Dispatch `Release` in EFF-blender for the exact Blender tag. Its build uses
   the now-published immutable native-base URL, runs author/update contracts,
   and creates the complete add-on release. Verify the asset, then publish.
6. Complete the EFF release readiness issue with final URLs and hashes.

## Native Protocol Changes

When `package-protocol.json` or native Blender tools change, prepare a new base
once before tagging:

1. Push the intended EFF source commit and exact Blender source commit.
2. Dispatch **Prepare Blender Native Base** with those two commit refs and the
   planned EFF tag. The workflow builds one candidate, verifies it against the
   production DLL protocol, and publishes a 30-day Actions artifact plus a JSON
   receipt in the run summary.
3. Copy that receipt to Blender `native-base.json`, commit and push the Blender
   change, then update and push the EFF gitlink. The zip bytes are never rebuilt
   during Release; the release workflow consumes the artifact from its recorded
   run ID and verifies the exact SHA256 again.
4. Create tags only after the commits and gitlinks are final. Dispatch the EFF
   workflow with the preparation run ID, release key, and receipt SHA256. After
   EFF is published, dispatch the Blender stable release.

The preparation artifact expires after 30 days. Finish the paired release
within that window; otherwise regenerate the candidate and update the receipt
before creating release tags. Never work around a hash/protocol failure by
manually replacing an already uploaded asset.

## Failure Handling

- A version, gitlink, checksum, or protocol failure is a release blocker. Fix
  source/receipt pairing and create a new reviewed commit/tag as appropriate;
  do not repeatedly dispatch the same failing run.
- A failed EFF build does not publish a release because draft creation happens
  after verification. Use the failed step and run log to correct the cause,
  then rerun the exact workflow once.
- Blender release is intentionally manual. A tag push runs CI, but cannot start
  the stable package workflow before the paired EFF asset is ready.
- Record every run ID, release URL, package size, and SHA256 in the readiness
  issue. Keep locally generated `bin/` output out of source commits unless it is
  the explicit, verified release artifact.

## Local Preflight

Before creating tags, verify each repository and parent gitlink:

```powershell
git status --short --branch
git -C tools/Blender status --short --branch
git -C tools/AnimeStudio status --short --branch
git submodule status
```

For an already published native base, run the same receipt/protocol verifier
used by CI after building the production DLL:

```powershell
python tools/release/verify_native_base.py `
  --archive bin/release-artifacts/EFF_Blender_Native_v0.49.2.zip `
  --dll-protocol bin/resource-directory-entry-v35/package-protocol.json `
  --manifest tools/Blender/native-base.json
```

The verifier checks ZIP integrity, required compiler files, exact size/SHA256,
and protocol compatibility. `python -m unittest discover -s tools/release`
exercises its failure paths without requiring Blender or the game.
