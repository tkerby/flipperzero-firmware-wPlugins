# Updating the CFS material catalog

`scripts/update_material_catalog.py` synchronizes the built-in catalog from an
external Creality `material_database.json` download. It requires Python 3.10+
from the standard library, with no other dependencies.

From the repository root, preview and apply an update:

```sh
python3 scripts/update_material_catalog.py /path/to/material_database.json --dry-run
python3 scripts/update_material_catalog.py /path/to/material_database.json
python3 scripts/update_material_catalog.py /path/to/material_database.json --check
ufbt
```

- `--dry-run` prints a unified diff without writing any files, and returns 0
  if the source is valid.
- **Default mode** regenerates only the marked block in `src/data/cfs_data.c`.
  It writes only when the result changes.
- `--check` never writes; exits **0 when synchronized**, **1 if the generated
  catalog is stale**, and **2 for invalid JSON or missing generation markers**.
  This is suitable for CI when a licensed/local copy of the JSON is available.

## Field mapping

The generator maps the fields represented in `CfsFilamentInfo`:

| C catalog field | JSON field |
| --- | --- |
| `base_id` | `base.id` |
| `brand` | `base.brand`, mapped to the existing `CfsBrand` enum |
| `material_type` | `base.meterialType` (source spelling is intentional) |
| `name` | `base.name` |
| `min_temp` | `base.minTemp` |
| `max_temp` | `base.maxTemp` |

The generator sorts entries by five-character ID, validates identifier lengths,
unique IDs, expected count, supported brands, nonempty text, and unsigned 16-bit
temperature bounds. Names and types are escaped safely as C strings. Unknown
brands **fail closed** until the C enum and vendor helpers are extended.

The `result.list[*].base` object is the sole source for these six C catalog
fields. The script does not read or validate `kvParam`, because those settings
are not used by the C catalog. Differences within unused fields are not reported.
The script never modifies the JSON or validates physical printing suitability.

Only the catalog is generated. Hand-maintained code,
including NFC encoding/decoding, fallback filament types, vendor-code mappings,
write wizard logic, color choices, and spool length tables, is left unchanged.
Do not manually edit generated regions. The source JSON is intentionally not
bundled or redistributed in this repository.
