# Desktop storage cleanup — 2026-11-04

Vincent requested removal of unnecessary old project files. The cleanup freed **6.44 GiB**,
measured by the increase in C: free space. Free space afterward was approximately **74 GiB**.

Removed only these verified untracked generated/download files inside Shieldbreak:

| Item | Approximate GiB |
|---|---:|
| `Intermediate/` build cache/PCH | 2.74 |
| Project `DerivedDataCache/` | 0.63 |
| 1,976 generated screenshots dated before 2026-11-04 in `Saved/Screenshots/` | 2.69 |
| Redundant `Saved/Tools/Blender/blender-4.5.15-windows-x64.zip` | 0.37 |

Rounded item sizes do not sum exactly to the measured total. Paths were resolved within the project,
checked for tracked files and unexpected links, and no editor/compiler/Blender session was active.
An itemized manifest is retained locally at `Saved/StorageCleanup-20261104.json`.

Retained source, artwork, all **2,760 Content files** (including local ignored scan dependencies),
Git history/branches/stashes, local config and integration backups, binaries, installed Unreal and
the extracted Blender executable. Committed screenshots/reports and the current character verification
folders remain. Personal photos/documents, games and unrelated applications were not deleted.
Older documentation that refers to raw pre-November-4 `Saved/Screenshots` files is historical;
those generated files were removed as requested, while committed review evidence remains.

Verification: Git reports no tracked deletions; `git lfs fsck --objects HEAD` passed. Content file
count remained 2,760. Unreal/Blender executables, integration config backup and current test reports
were confirmed present. No game/build test was run, preserving the user's overnight development pause.
The next build/launch will regenerate caches and may be slower.

Larger external items were **audited only**, not removed:

- `C:\Users\addve\Documents\Unreal Projects\ElectricDreamsEnv`: about **56 GiB logical**, an art-source sample.
- `C:\Windows.old`: about **47 GiB logical**, including a previous Windows installation and old Users folder.
- Installed UE 5.8: about **84 GiB logical**, required by the project and retained.
- Shared Unreal Common data: about **8.2 GiB logical**, retained.

Logical sizes are not guaranteed reclaimable space. The sample may be useful for future art imports;
Windows.old can contain rollback data and old personal files. Further removal needs a specific review.
