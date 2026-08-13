# ChartDisplay
### <span style="color:red"> For flight simulation use only. Not for real world use. </span>

ChartDisplay is a chart viewer for US VATSIM controllers. Each AIRAC cycle it downloads the FAA digital Terminal Procedures Publication (dTPP) charts and organizes them by ARTCC, airport, and chart type, and it lets you add your own custom charts.

If you run the build output directly (rather than installing), keep `ChartDisplay.exe` and its DLLs together in one directory. The installer places everything for you — including the Visual C++ runtime — so it has no separate prerequisites.

## Data and storage

All data lives in `%LOCALAPPDATA%\ChartDisplay`. The organized charts take several GB; the downloaded zip volumes are kept so the charts can be rebuilt without re-downloading, which pushes the total to roughly 8–10 GB. To put the data on a drive with more room, create a directory symlink named `ChartDisplay` in `%LOCALAPPDATA%`.

To reclaim space, delete everything inside `%LOCALAPPDATA%\ChartDisplay\download` (keep the folder). The only consequence is that **Reload Charts** stops working; the charts can always be fetched again with **Force Chart Update**.

## Downloading and updating

On first run — and, if **Autoupdate on start** is enabled, after an AIRAC cycle change — the program downloads and organizes the charts. This happens *after* the main window opens, behind a progress dialog that shows the current file and phase; expect roughly 10–20 minutes.

- **Force Chart Update** re-downloads the current cycle on demand (this can be cancelled).
- **Reload Charts** rebuilds the organized charts from the already-downloaded zips, with no network access.

## Opening charts

Charts and custom items open with your Windows default app for the file type — exactly like double-clicking them in Explorer — so UWP/Microsoft Store apps such as Photos work. If a file type has no default, Windows' "Open with…" picker appears so you can choose (and optionally set) an app. A PDF-viewer association is recommended; set associations for any other chart file types you use.

## Custom charts

Custom charts are stored in `%LOCALAPPDATA%\ChartDisplay\custom_charts.xml` and edited through the **Custom Charts** dialog. A custom chart whose source file can't be found is dropped when the list is (re)loaded, so keep the referenced files in place — in particular, if a drive letter changes, update the paths before loading.
To load custom charts, select the type of chart in the dialog, either for an airport, a whole artcc or the cwt reference, fill in the fields asked for and click add. To remove select in the list and click remove.
The type field for airport charts determines what section of charts it is added under.
For existing airports in the NAS, airport class is automatically determined by FAA data. To input a fictional airport, click the custom airport button.
Only one CWT reference is allowed. This is what is opened by clicking the "CWT Reference" button. If there is no CWT reference given, an image of the CWT charts in FAA JO 7110.65 5-5-4 will open. This is included with the program.
All files are opened based off the default program associated with them.

## Third-party software

Downloads are performed in-process with the Windows HTTP API (WinHTTP); ChartDisplay no longer uses a separate download helper, cpr, or libcurl.

- **pugixml** — based on the pugixml library (http://pugixml.org), Copyright (C) 2006-2023 Arseny Kapoulkine.
- **sqlite_orm** — GNU Affero General Public License, version 3. See LICENSE.
- **sqlite3** — public domain.
- **libzip** — 3-clause BSD. See LICENSE.
- **zlib** — see LICENSE.
- **bzip2** — see LICENSE.
