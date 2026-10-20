# Catalog submission

`manifest.yml` is the entry for the
[Flipper Application Catalog](https://github.com/flipperdevices/flipper-application-catalog)
(Flipper Lab and the Flipper mobile apps). Name, id, version, category, author and icon come
from `application.fam`.

1. Push all changes and copy the full commit hash: `git rev-parse HEAD`.
2. Fork the catalog, create the branch `TheProductAgency/sub_counter_1.0`.
3. Copy `manifest.yml` to `applications/Tools/sub_counter/manifest.yml` and set `commit_sha`.
4. Validate: `python3 tools/bundle.py --nolint applications/Tools/sub_counter/manifest.yml bundle.zip`
5. Open a pull request with the template (fill in the checklist and the AI usage disclosure).

For updates, raise `fap_version` in `application.fam`, add an entry to `docs/changelog.md`
and repeat with the new commit hash.
