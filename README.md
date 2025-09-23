# ChartDisplay
Make sure the ChartDisplay.exe, ChartDisplayDownloadHelper.exe and DLLs are in the same directory. If needed, the command line option --downloader-path can be passed to set the path of ChartDisplayDownloadHelper.
This program uses a lot of storage (>5 GB) for the charts. It downloads them on first opening, and (if auto-update) is checked, after an airac cycle change.
The directory for all the data of this program is Local App Data. If you need everything on a drive with more space, a symlink works with the name ChartDisplay in AppData\Local.
The program keeps the zips around to reload the charts in case of problems, but this pushes the size to 10 GB. 
If you do delete the downloaded zips, the only thing that will not work is the "Reload Charts" button, but if needed, the charts can be redownloaded with the "Force Charts Update" button.
To delete the zips, remove everything in the download subdirectory of the ChartDisplay data directory, but keep the folder (AppData\Local\ChartDisplay\download).
The whole process takes around 10-20 minutes and this happens before the window appears, so it just appears to do nothing.
The displaying of items, including custom charts uses the default file association in Windows (although some apps, like photos don't work and MS Store apps have not been tested). Please make sure you have set up a PDF association,
and if you are using other file types, associations for those as well.

License info for third party software.
pugixml:
This software is based on pugixml library (http://pugixml.org). pugixml is Copyright (C) 2006-2023 Arseny Kapoulkine.
sqlite_orm:
sqlite_orm is distributed under the GNU Affero General Public License, version 3. See LICENSE for a copy of the license.
sqlite3:
SQLite is in the public domain
cpr:
C++ Requests (cpr) is under the MIT License, Copyright (c) 2017-2021 Huu Nguyen 
Copyright (C) 2022 libcpr and many other contributorssee the LICENSE file for a copy.
libcurl:
Modified MIT License, see LICENSE file
libzip:
3 clause BSD. See LICENSE file.
zlib: see LICENSE file.
bzip2: see LICENSE file.
