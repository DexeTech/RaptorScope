# RaptorScope — TODO

Tracking unimplemented features, known shortcomings, and future work.
Items marked `[GREYED]` have menu entries that are currently disabled/greyed out.

---

## Unimplemented Menu Commands

### Entry Operations

- [ ] `[GREYED]` **IDM_ENTRY_EXPORT_ALL** — Export All entries to a folder
  - Needs `SHBrowseForFolder` (XP-safe) or `IFileOpenDialog` with fallback
  - Auto-name files: `NNN_TYPE.ext` using `dat_type_ext()`
  - Option to auto-decompress LZSS entries on export

- [ ] `[GREYED]` **IDM_ENTRY_IMPORT** — Import / Replace selected entry
  - Load raw file from disk, replace `DatEntry::data` and `size`
  - **Requires ownership model change**: entries currently point into `DatArchive::raw` (non-owning). Imported data needs `malloc`'d buffer with ownership flag
  - Add `DAT_FLAG_OWNS_DATA (0x02)` to `DatEntry::flags`, free in `DatArchive::free_data()`
  - For LZSS types (7/8): offer to LZSS-recompress imported data via `lzss_compress()`
  - Update header fields (size, w/h) — prompt user or auto-detect
  - Mark archive as dirty, enable Save As

- [ ] `[GREYED]` **IDM_ENTRY_ADD** — Add new entry to archive
  - Dialog: select type (dropdown of `DAT_DATA`..`DAT_LZSS1`), set x/y/w/h, pick file
  - Grow `DatArchive::entries` array (realloc or new+copy)
  - New entry owns its data (`DAT_FLAG_OWNS_DATA`)
  - Enforce max entries = `DAT_SECTOR / 16 - 1` = 127 (header is one 0x800 sector)
  - Rebuild tree view after add

- [ ] `[GREYED]` **IDM_ENTRY_DELETE** — Delete selected entry
  - Remove from entries array, shift remaining
  - If entry owns data, free it
  - Rebuild tree view
  - Mark archive dirty

### Export Formats

- [ ] `[GREYED]` **IDM_EXPORT_PNG** — Export current image panel as PNG
  - Need minimal PNG writer (stb_image_write.h single-header, or hand-rolled deflate+PNG)
  - stb_image_write is public domain, ~1.5KB compiled, pre-C++11 safe
  - Source RGBA from `ImagePanel::hBmp` or re-render from entry
  - Include palette row / BPP in default filename

- [ ] `[GREYED]` **IDM_EXPORT_TGA** — Export as TGA + palette cue file
  - Uncompressed 32-bit TGA (trivial: 18-byte header + BGRA pixels)
  - Companion `.cue` or `.pal` file with raw BGR555 CLUT data
  - Useful for round-trip: TGA import back with palette

---

## Architecture Issues

### Entry Data Ownership
- **Current**: `DatEntry::data` is a raw pointer into `DatArchive::raw` file buffer. Zero-copy, read-only.
- **Needed**: Mixed ownership model for import/replace/add operations.
- **Plan**: Use `DatEntry::flags` bit 1 (`DAT_FLAG_OWNS_DATA = 0x02`). When set, `data` was `malloc`'d separately and must be `free`'d on cleanup. Update `DatArchive::free_data()` to check flag per-entry before freeing `raw`.

### Archive Dirty State
- No dirty tracking. After import/add/delete, user should be warned on close if unsaved.
- Add `bool dirty` to `DatArchive` or `App`.
- Enable/disable Save As based on dirty state.
- Prompt on `WM_CLOSE` / `IDM_FILE_CLOSE` if dirty.

---

## View Menu Enhancements

- [ ] **IDM_VIEW_HEX / IDM_VIEW_3D / IDM_VIEW_IMAGE / IDM_VIEW_AUDIO / IDM_VIEW_PALETTE** — Force panel switch
  - IDs defined in `app.h` (3001-3005) but not added to View menu
  - Would allow user to manually switch panel regardless of auto-detect
  - Accelerators partially mapped (F5-F9 in .rc) but no handler

- [ ] **Zoom controls in View menu** — Image zoom presets (25%, 50%, 100%, 200%, 400%)

---

## Export Enhancements

- [ ] **Batch OBJ export** — Export all meshes in archive as separate .obj files
- [ ] **Batch WAV export** — Export all SNDB samples as .wav files
- [ ] **PNG atlas export** — Stitch all textures into a single atlas PNG
- [ ] **glTF export** — Modern 3D format with embedded textures and skeleton

---

## Import / Modding Pipeline

- [ ] **PNG import** — Convert PNG to 8bpp indexed + BGR555 CLUT, re-swizzle, replace texture entry
  - Needs palette quantization (median cut or similar) if source is truecolor
  - Or assume indexed PNG with matching palette
- [ ] **TGA import** — Read uncompressed TGA, convert to entry format
- [ ] **OBJ import** — Convert OBJ mesh back to room/EMD format (complex, low priority)
- [ ] **LZSS recompress on import** — When replacing type 7/8 entries, auto-compress with `lzss_compress()`
- [ ] **Drag-and-drop import** — `WM_DROPFILES` handler for quick file replacement

---

## XP Compatibility Notes

- `dpiAware` in manifest works on Vista+, ignored on XP (acceptable)
- Missing `dpiAwareness` for per-monitor DPI on Win8.1+ / Win10 1703+
  - Consider adding `<dpiAwareness>PerMonitorV2</dpiAwareness>` with runtime `SetProcessDpiAwarenessContext` fallback
- `_snprintf` behavior differs between MSVC versions (no null-term on overflow in old MSVC) — currently safe but worth auditing
- `SHBrowseForFolder` for folder picker is XP-safe; `IFileOpenDialog` (Vista+) is not
- Dynamic UxTheme ordinal loading (132, 135) correctly handles absence on XP/Vista

---

## Polish / QA

- [ ] Status bar feedback for all operations (import, delete, add, export all)
- [ ] Undo/redo stack for entry modifications (stretch goal)
- [ ] Keyboard navigation for tree view entries
- [ ] Right-click context menu on tree entries (export, replace, delete)
- [ ] Drag reorder entries in tree (low priority)
- [ ] Memory leak audit — ensure all owned buffers freed on archive close
- [ ] Test with actual Dino Crisis .dat files: COMMON, DOOR, PL00, ROOM*.dat, item.dat
