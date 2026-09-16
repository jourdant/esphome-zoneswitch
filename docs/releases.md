# Releases

Use project tags such as `v0.2.0-rc.1` or `v0.2.0`. They identify a complete
component revision, not a wire protocol. V1.0-T/V2.1 are hardware revisions;
`protocol: v1`/`v2` are configuration choices.

## Prepare an immutable release

1. Review the diff and update the Unreleased changelog with migration details.
2. Run the host replay/transaction tests, schema checks and ESP32 builds for both
   protocols using the exact ESPHome versions that will be named in the release.
3. Record results in `docs/research/validation/YYYY-MM-DD.md`. Include commit,
   framework/version, board, and whether a result is a software check or a bench
   observation. Preserve explicit V1 limitations even when software tests pass.
4. Perform the relevant bench checks for changed transport/control behavior.
   A successful build alone does not establish hardware compatibility.
5. Choose an unused project version. Use a prerelease tag while release-level
   hardware validation is pending. Move the changelog entry to that version.
6. Create and push an annotated tag on the reviewed commit:

   ```sh
   git tag -a v0.2.0-rc.1 -m 'ZoneSwitch v0.2.0-rc.1'
   git push origin v0.2.0-rc.1
   ```

The version above is an example, not a published release. Do not move published
tags. Publish a new version for corrections. Release notes should link validation,
name tested ESPHome versions, and distinguish V2 support from V1 experimental status.

## Install or roll back

Set `external_components.source.ref` to the published tag or full tested commit
SHA, then compile and install. Preserve the previous device YAML and ref before
updating so a rollback can rebuild the prior firmware. Examples on a moving
branch are development examples, not immutable release artifacts.

No release tag is created solely by a documentation or refactoring pass; the
checks and review above must substantiate the named release.
