# NeoVitaDB Downloader
NeoVitaDB Downloader is a PSVita/PSTV homebrew client, a fork of Rinnegatamante's VitaDB Downloader. It was created after the original VitaDB backend at rinnegatamante.eu shut down, and now runs on [NeoVitaDB-Catalog](https://github.com/robin994/NeoVitaDB-Catalog), a static catalog of PSVITA/PSTV homebrew hosted on GitHub Pages.

## Installation
1. Download the latest `.vpk` from the [Releases page](https://github.com/robin994/NeoVitaDB-Downloader/releases), or scan the QR code below — it always points to the newest release, no need to update the link/image after a new version comes out:

   [![QR code linking to the latest NeoVitaDB.vpk release](assets/qr_latest_vpk.png)](https://github.com/robin994/NeoVitaDB-Downloader/releases/latest/download/NeoVitaDB.vpk)
2. Get the `.vpk` onto your Vita's storage. A couple of common ways to do this with [VitaShell](https://github.com/TheOfficialFloW/VitaShell):
   - **FTP** — In VitaShell, press Select to start the FTP server (it will show an IP and port). From your PC: `curl -T NeoVitaDB.vpk ftp://<vita-ip>:1337/ux0:/`.
   - **USB** — In VitaShell, press Select to enable USB mode, then copy the file over like a regular USB drive.
   - **On-device browser** — If your Vita has internet access, you can download the `.vpk` directly from the Releases page using VitaShell's built-in browser.
3. In VitaShell, navigate to where you placed the `.vpk` and press X to install it.
4. Launch NeoVitaDB Downloader from LiveArea. On first boot it needs an internet connection: it will automatically fetch and set up `libshacccg.suprx` (runtime shader compiler) and `kubridge.skprx` if they're missing, then download the initial app list and icons — this first boot takes noticeably longer than later ones.

## Features
- Searching by author/homebrew name.
- Filtering apps by category.
- Viewing of all available screenshots and video trailer for apps.
- Sorting apps by different criteria (Recently Added, Recently Updated, Oldest, Most Downloaded, Least Downloaded, Alphabetical, etc...)
- Showing of several metadata for apps.
- Download and installation of vpk+data files or vpk only at user discretion. (No more need to redownload data files everytime you want to update an homebrew for which data files are unchanged)
- GUI based on dear ImGui, providing a very robust user experience without sacrificing on fancyness and with high customizability.
- Fast boot time (Only the very first boot will take a bit more due to app icons download. Successive boots will be basically instant)
- Low storage usage (Screenshots are served on demand, the only data that are kept on storage are app icons with a complessive storage usage lower than 10 MBs).
- Tracking of installed apps and of their state (outdated/updated) even when not installed through VitaDB Downloader.
- Background music (You can customize it by changing `ux0:data/NeoVitaDB/bg.ogg` with your own preferred track).
- Background image/video (You can customize it by `changing ux0:data/NeoVitaDB/bg.mp4` or `ux0:data/NeoVitaDB/bg.png`).
- Support for themes (Customization of GUI elements via `ux0:data/NeoVitaDB/themes.ini`) with built-in downloader and manager.
- Support for PSP homebrews.
- Daemon support for homebrews update check in background during normal console usage.

## Using a Custom Catalog
By default, NeoVitaDB Downloader fetches its app lists, icons, screenshots and trophies from the
official [NeoVitaDB-Catalog](https://github.com/robin994/NeoVitaDB-Catalog). A dropdown next to
the Search bar lets you switch between catalogs at any time — the official NeoVitaDB catalog and
the original VitaDB (rinnegatamante.eu) backend are always listed first, followed by any custom
catalogs you've added. Switching downloads that catalog's app list right away if it's never been
used before, or reloads it instantly if it has.

To make a custom catalog show up in that dropdown, create `ux0:data/NeoVitaDB/catalogs.cfg` and
list one catalog per line, either as just a URL or as `Alias|URL` if you want a short name to show
up in the dropdown instead of the full URL:
```
https://your-username.github.io/NeoVitaDB-Catalog
My Friend's Catalog|https://someone-else.github.io/NeoVitaDB-Catalog
```
There's no in-app way to add entries to this list for now — edit the file over FTP/USB with
VitaShell (same as customizing `daemon_blacklist.txt` or `theme.ini`) and restart the app; the
dropdown itself is how you then switch between whatever's in it. See
[`catalogs.cfg.example`](catalogs.cfg.example) in this repo for a ready-to-edit starting point —
just fill in real catalog URLs and copy it to `ux0:data/NeoVitaDB/catalogs.cfg`.

**A custom catalog only works if it's a fork of NeoVitaDB-Catalog that keeps its data structure
unchanged** (same `vita.json`/`psp.json` schema, same `icons.db`/`icons/` layout, same
`trophies/` layout). The app talks to whatever URL you give it by building requests exactly the
way it does for the official catalog, so anything that doesn't lay data out identically won't
work. Bootstrap files needed before any catalog is even parsed (`libshacccg.suprx`'s
PSM-runtime-extraction chain, `kubridge.skprx`) always come from the official catalog regardless
of `catalog.cfg` — a custom catalog isn't expected to mirror those.

Each catalog you point the app at gets its own isolated storage under
`ux0:data/NeoVitaDB/catalogs/`, so app lists, icons, favorites, and the daemon blacklist from one
catalog never mix with another's — switching `catalog.cfg` back and forth doesn't lose or corrupt
either one's data.

## Themes
You can find some themes usable with this application on [this repository](https://github.com/CatoTheYounger97/vitaDB_themes).
Those themes can also be accessed in the app itself by pressing L. While in Themes Manager mode, you can download themes by pressing X and install themes in two different ways (that can be interchanged by pressing Select):
- Single = A downloaded theme will be installed as active one by pressing X
- Shuffle = Pressing X will mark a theme, you can mark how many themes you want. Once you've finished, press again Select to install a set of themes for shuffling. This means that every time the app is launched, a random theme will be selected from the set and used as active one.

## Homebrew Updater Daemon
Starting with v.1.7, VitaDB Downloader features an optional daemon that allows to check for all your installed homebrews updates in background. When console is booted and every hour after the first boot, updates will be searched and, if found, notifications will be fired to notify the user of its existence.
By default, a couple of homebrews are blacklisted from this process either cause they are nightly builds (for which it's not reliable to checksum the hash on server side to perform the update veerification) or cause the Title ID of the app is being used by two or more applications (making impossible to perform an update check).
It's also possible to add more blacklisted homebrews (for example, if you use a modded build which would be tagged as outdated by VitaDB Downloader). To do so, create the file `ux0:data/NeoVitaDB/daemon_blacklist.txt` and add inside it a list of Title ID of the homebrews you want to blacklist in this format `ABCD12345;ABCD12346;ABCD12347`.

## Changelog
### v.2.9.0
- On first launch (and every launch until accepted), the app now shows a disclaimer explaining
  that catalog homebrew comes from community sources not individually vetted by the developer,
  and that installing it is done at your own risk. Declining closes the app without saving
  acceptance, so it's shown again next time; nothing else runs until it's accepted.
- Downloads and Likes now show "Unavailable" instead of "0" when there's no real count for an
  entry, instead of displaying a number that could be mistaken for an actual zero.

### v.2.8.6
- No longer sends a spoofed Chrome user agent on every request - some servers were flagging it as
  bot traffic since the rest of the request didn't look like a real browser. Requests now identify
  themselves as the app instead.
- Fixed PSP icons fetched by the new bulk download (see v.2.8.5) landing in a local folder nothing
  else ever looks in, so they never actually showed up despite downloading successfully.
- Fixed the bulk icon download re-fetching the same icons on every single launch instead of only
  when actually needed - extracting one platform's icon bundle was wiping out the record of the
  other platform's icons already having been downloaded, since both share the same on-device
  tracking file.

### v.2.8.5
- Fixed a crash that could happen while downloading the app list or missing icons - any URL
  containing a `%` character (common in percent-encoded filenames) was passed straight to
  `sprintf()` as the format string instead of as an argument, so those characters were
  interpreted as format specifiers instead of literal text.
- Fixed a crash that could happen when most or all icons needed downloading at once (e.g. a
  fresh install or switching to a catalog you've never used before) - the list of icons still
  to fetch was collected into a fixed-size buffer with no bounds check, so a large enough
  catalog could overflow it.
- Missing icons are now fetched in a single bundled download instead of one request per icon
  when there are many of them, which is both faster and avoids the two issues above in the
  first place. Falls back to downloading them individually if the catalog doesn't support this
  yet or the bundle can't be fetched.

### v.2.8.4
- Fixed switching to a different catalog (from the dropdown next to the Search bar) always
  reverting back to the official catalog - the fallback that's meant to apply only when
  `catalog.cfg` is missing or empty ran unconditionally instead, silently discarding whatever
  catalog had just been read from the file.
- Fixed a crash that could happen after switching catalogs twice in a row
- Fixed a heap buffer overflow while parsing a catalog's app list

### v.2.8.3
- Fixed some Vita homebrews (e.g. Freegemas, OceanPop) failing to install with "The installation
  process failed." - their release packages wrap the `.vpk` one or more folders deep inside the
  zip (e.g. `game-vita/game.vpk`), and the fallback that looks for a nested `.vpk` when there's no
  `eboot.bin` at the top level only checked the top folder itself, never its subfolders, so it
  silently found nothing and installation failed at the promotion step. It now searches
  subfolders too.

### v.2.8.2
- Fixed animated theme backgrounds (and homebrew trailers) never actually playing on real hardware
  while working fine in the Vita3K emulator - a permanent black/static screen instead. The video
  memory AVPlayer decoded into needs to come from a directly-allocated, GPU-mapped CDRAM block
  rather than any of vitaGL's own memory pools, which all silently fail to produce a usable buffer
  for this specific purpose on real hardware despite reporting no error.
- Fixed a black screen that could still occur even when the fix above didn't apply - if the first
  video frame never arrives within a few seconds, the app now falls back to the theme's static
  background image (or, for trailers, reports the failure) instead of leaving a black screen up
  indefinitely.
- Fixed a memory leak in video playback cleanup: closing the player didn't always release its
  largest buffer, so repeatedly opening and closing video (e.g. reinstalling a theme, replaying a
  trailer) could exhaust the console's dedicated video memory pool over a session.
- Fixed the previous theme's background lingering on screen ("ghosting") after switching to a
  theme whose own background never actually loaded (e.g. an animated background real hardware's
  decoder rejected, with no static image to fall back to) - the old background's GPU texture was
  never released or cleared, so it kept rendering underneath.
- Fixed a persistent ghosting/trail effect on real hardware, most visible while scrolling the app
  list: AVPlayer was configured to keep 5 buffered output frames in flight for the background
  video, and cycling through that many textures on real hardware's decoder left stale frames
  visible on screen. Dropping it to a single buffered frame removed the effect.

### v.2.8.1
- Fixed the in-app self-update looping forever on a fresh update: a stray extra slash in the
  extraction path meant a `.vpk`-packaged update (as opposed to the old PSARC format) got written
  somewhere other than where the app actually loads from, so it kept detecting itself as outdated
  and re-downloading the same update every time it relaunched. If you're stuck in this loop on
  v.2.8.0, install this version's `.vpk` manually once to break out of it.

### v.2.8.0
- Fixed themes potentially corrupting `theme.ini` into a multi-gigabyte file and crashing the app
  on every subsequent launch, if a theme download failed partway (e.g. hitting GitHub's rate
  limit). Theme downloads and installs now fail cleanly with an error message instead.
- Fixed a stack buffer overflow when parsing a malformed or corrupted `theme.ini`.
- Self-update version checks now compare major.minor.**patch** instead of just major.minor, so a
  future patch release (e.g. v.2.8.1) is correctly detected as newer and can be installed in-app.
- Fixed the first-boot SharkF00D bootstrap (needed to set up `libshacccg.suprx`) freezing forever
  on an interrupted or corrupted download, which could look like the app crashing. A failed
  download now shows a clear error asking you to check your connection and relaunch, instead of
  hanging or silently continuing with a broken install.
- Fixed the apps list not actually being sorted after switching catalogs — it stayed in raw parse
  order until you manually reselected a sorting mode, even though the active sort was still shown
  as selected.
- Added a new default sort mode, "Recently Added", showing what's newest in the catalog itself
  rather than the underlying homebrew's own release date. The old "Most Recent" (which sorts by
  that release date) is still there, renamed to "Recently Updated" to make the difference clear.

### v.2.7
- Added an in-app catalog switcher: a dropdown next to the Search bar lets you change which
  catalog the app uses at any time, with the official NeoVitaDB catalog always listed first.
- Added the original VitaDB (rinnegatamante.eu) backend as a second always-available catalog
  choice in that same dropdown, for anyone who wants to browse and download from it now that it's
  back online, alongside the official NeoVitaDB catalog and any custom ones you've added.
- Added support for custom catalogs via `ux0:data/NeoVitaDB/catalogs.cfg`, listing any
  NeoVitaDB-Catalog-compatible fork you want to be able to switch to from that dropdown.
- Added optional aliases for custom catalogs (`Alias|URL` in `catalogs.cfg`), so the dropdown can
  show a short name instead of the full URL.
- Switching catalogs now automatically downloads that catalog's app list and icons if it's never
  been used before, or reloads it instantly if it has, without needing to restart the app.
- Each catalog keeps its own isolated app list, icons, favorites and daemon blacklist, so switching
  back and forth between catalogs never mixes or loses either one's data.
- Added PSP homebrew support: browse, download and install PSP homebrews from the catalog, the
  same way as PSVITA ones.
- Added "Trusted by the community" and "Uses AI"/"Does not use AI" labels shown for an app when
  hovering it.
- Fixed the app incorrectly re-downloading and reinstalling an older published release over itself
  whenever its version didn't exactly match the catalog's, including when the installed build was
  already newer than what the catalog listed.
- Fixed a potential crash when an app's icon is missing or fails to download from the catalog
  (e.g. a broken link): the app now falls back to showing its own icon instead.
- Fixed `catalog.cfg` breaking if it ended up with more than one line in it — it's meant to hold a
  single catalog URL; use `catalogs.cfg` (plural) instead to list several catalogs.
- Fixed `catalog.cfg` ending up with leftover bytes from a previously selected, longer catalog URL
  appended to a shorter one picked afterwards, corrupting the saved URL.
- Fixed installation getting stuck at a fixed percentage forever on a truncated or corrupted
  download, instead of failing with an error.
- Fixed potential corruption of `tai/config.txt` (and other on-device files read back before being
  rewritten, such as the icon manifest) on a failed read.
- Fixed the app getting stuck exiting immediately on every launch if the currently selected catalog
  became unreachable, with no way back into the UI to pick a different one — it now automatically
  falls back to the official catalog instead.
- Fixed a failed in-app catalog switch leaving the app on a broken, empty catalog with no way to
  recover short of editing `catalog.cfg` manually — it now reverts to the previously working one.
- Fixed the PSP apps list failing to download (and silently retrying every single frame for as
  long as PSP mode stayed open) due to a leftover HTTP status code carried over from an unrelated,
  earlier download.
- Fixed the in-app self-update marking itself as updated (and relaunching) without actually
  installing the new version, since it always assumed the downloaded release was in the old
  VitaDB backend's PSARC format instead of the plain `.vpk` this project's own releases use.

**Adding a custom catalog:** create `ux0:data/NeoVitaDB/catalogs.cfg` and add one catalog per line,
either a bare URL or `Alias|URL` for a short name in the dropdown — see
[`catalogs.cfg.example`](catalogs.cfg.example) for a ready-to-edit template, and "Using a Custom
Catalog" below for the full details. Any catalog you add must be a fork of NeoVitaDB-Catalog that
keeps its data structure unchanged, since the app talks to it exactly the way it talks to the
official one.

**If you run into problems after updating** — the app not starting, stuck loading, or behaving
unexpectedly — try deleting `ux0:data/NeoVitaDB` entirely over FTP/VitaShell and letting the app
recreate it from scratch on next boot. This only clears cached catalog data, icons and your local
customizations (themes/backgrounds/favorites); it does not touch your installed homebrews.

> ⚠️ **If you're on v.2.7, you must install v.2.8.1 manually (via VitaShell) — the in-app
> self-update cannot complete on v.2.7**, since it always assumes the old PSARC format instead of
> checking what was actually downloaded (this project's releases are plain `.vpk`), so it silently
> fails and keeps re-offering the same update on every launch. Once on v.2.8.1 (or later),
> self-update works normally for all future releases. v.2.8.1 also fixes v.2.8.0's own self-update
> bug (a stray extra slash made an update install somewhere the app doesn't actually load from,
> looping the same way) and a theme download that failed partway (e.g. hitting GitHub's rate
> limit) corrupting `theme.ini` into a multi-gigabyte file that crashed the app on every launch —
> if you're already affected by that, deleting `ux0:data/NeoVitaDB/theme.ini` over FTP/VitaShell
> resolves it.

### v.2.6
This is the first release under the new NeoVitaDB name, after the original rinnegatamante.eu
backend that VitaDB Downloader relied on shut down.
- Forked as NeoVitaDB Downloader and migrated to a new backend: application lists, icons,
  screenshots, trophies, themes and bootstrap files (SharkF00D, kubridge, PSM Runtime) are now
  served from a static catalog (NeoVitaDB-Catalog) hosted on GitHub Pages instead of the old PHP
  backend.
- App download URLs are now provided directly by the catalog instead of being resolved through a
  redirect endpoint on the old backend.
- Renamed the on-device data folder from `ux0:data/VitaDB` to `ux0:data/NeoVitaDB`.
- Updated LiveArea artwork for the NeoVitaDB rebrand.
- Added "Trusted Apps"/"Not Trusted Apps" filters for both Vita and PSP homebrews.
- Fixed a bug that could corrupt the newlib heap and crash the app when downloading a large batch
  of missing icons at boot.
- Fixed a bug causing an icon to be wrongly marked as downloaded when its download actually failed.
- Fixed a crash when the apps list is empty.
- Fixed downloads hanging indefinitely on a stalled connection with no way to cancel.
- Fixed a memory leak on every retried download.
- Fixed a data race between overlapping download threads.
- Fixed the apps/PSP apps list being silently discarded if the server response was smaller than
  expected.
- Fixed a potential crash on first boot on systems without an existing `tai/config.txt` (e.g.
  Vita3K).

### v.2.6
- Added distinction between vibecoded and AI assisted apps: now they will use different icons.
- Made so that filters for PSP and PSVita homebrews are now stackable: this will allow for more granular researches in the database.
- Made so that filters results are cached instead of being recalculated each frame: this reduces the CPU workload of the application.
- Added Favorites filter for PSP homebrews.
- Added Vibecoded Apps filter for PSP and PSVita homebrews.
- Fixed an out of bound bug in the favorites list population that caused VitaDB Downloader to crash at boot under certain circumstances.
- Added Game Score metadata for game ports having one in the database. The Game Score is a weighted average of the original game ported scores on different critics aggregator websites (it is NOT related to the quality of the port itself).
- Added two new sorting methods: Highest Game Score, Lowest Game Score.
- Made YoYo Loader and Nazi Zombies Portable no more hardcoded blacklisted in the daemon blacklist since the new CI support offered by VitaDB ensures that the update hashes are properly fresh.
- Fixed a bug causing visual loss of selected application after performing some specific actions (eg: Uninstalling an app, watching a trailer, ...).
- Updated to latest vitaGL commit.

### v.2.5
- Added a Crank icon nearby apps using AI.
- Added two new filters: "Apps using AI" and "Apps not using AI" for Vita homebrews.
- Moved to MariaDB IDs usage for the Favorites homebrew system: This means now applications with clashing TitleIDs can be properly put in the favorite list singularly.
- Added support to Favorites system for PSP homebrews.
- Reduced memory usage of the application.
- Updated to latest vitaGL commit.
- Made so that the search feature searches also inside homebrews/themes descriptions.
- Made so that the search feature is now cached, not impacting performance of the app.
- Made so that Vita applist and PSP applist are downloaded only when there's an update on the server (using If-Modified-Since HTTP header): this will make booting the app and switching to PSP apps way faster.
- Fixed a bug causing Favorites list and the daemon blacklist to get corrupted when deleting entries in some circumstances.
- Fixed a bug causing the blacklisted status to not be visually updated inside the app when blacklisting or whitelisting an application in certain circumstances.

### v.2.4
- Fixed a bug in the daemon causing the console to panic when downloading the homebrews list for scanning new updates. (Part of a v.2.3 hotfix)
- Added a progressbar and video time (current/total) info for the trailers player.
- Made so that when a trailer ends, the app will automatically transition back to the main apps list.
- Made so that the apps list downloader at boot will retry the download if it fails or gets stuck.
- Added a check for VitaDB state: now if the website is offline, the application will show a warning when launched.
- Fixed several bugs in the trailers player causing deadlocks or app crashes in certain circumstances.
- Made more robust the whole downloader logic: this should solve some edge cases leading to app softlocks.
- Added a custom header on VitaDB backend and implemented its usage in the app: this will make file size shown when downloading apps list be correct and not just guessed as it was before.
- Implemented a feature that allows to edit the daemon blacklist from the app itself: now an option will be available in the Manage menu of installed apps that will allow to blacklist or whitelist apps for the daemon.
- Added showing of the daemon blacklist state in the Info panel of the currently hovered application.
- Added proper support for update detection of applications made with LifeLua.
- Fixed empagination of the Manage submenu being inconsistent depending on how many options are available.
- Fixed a bug causing Manage submenu to have the last option offscreen when the related homebrew has trophies available.
- Added possibility to add Vita homebrews in a Favorites list from the Manage submenu: favorites homebrews will show a star icon near their name in the main apps list.
- Added "Favorites Apps" filter to the available Vita homebrews filters.
- Removed the "Apps with Trophies" filter from the PSP homebrews list: PSP homebrews can't have trophies so the filter would always return an empty list.

### v.2.3
- Fixed a bug causing renpy games to install with missing files.
- Added the possibility to view Release Page and Sourcecode Page for homebrews (Available in the Manage submenu).
- Added the possibility to view trailers for homebrews having one (Available in the Manage submenu or by pressing Start).
- Made so that the PSVita homebrew icons are rendered with an animated 3D bubble effect simulating the look of the Livearea bubbles.
- Made so that the titlebars for the subwindows properly respect console button assignation for Cancel/Confirm.
- Fixed an issue causing memory corruption when swapping several times themes with animated backgrounds.
- Made so that vpks are installed from a lower depth folder. This can prevent edge cases where vpks with a lot of nested folders may fail to install correctly.

### v.2.2
- Fixed a bug causing background to flicker during popups (example: during downloads).
- Replaced Sony CDN links with archive.org links for PSM Runtimes. Now the libshacccg.suprx auto-installer will work again.
- Fixed progressbar during PSM Runtimes download to not properly update in realtime.
- Made so that multi-downloads (example: missing icons download) will properly show total count and updated count during the process.
- Restored erroneously removed message box during ShaRKF00D extraction during libshacccg.suprx auto-installer process.
- Replaced zip/vpk usage with psarc files. These will be faster to extract resulting in faster installation process for apps.

### v.2.1
- Fixed a bug causing the background image to get flipped during some applications installations.
- Fixed a bug causing trophies and icons to still get recovered using old backend.
- Greatly optimized sorting algorithm (faster booting time and faster switching between sorting modes).
- Added an automatic updater for the Daemon plugin. (Prior it wasn't getting updated at all).
- Added a feature that will mark applications with clashing TitleIDs by showing said TitleIDs in red.
- Added a confirmation check when installing applications with clashing TitleIDs when an application sharing the same TitleID is already installed.
- Fixed a filehandle leak in themes installer.
- Made application extractions faster. Now installing homebrews will take less time.

### v.2.0
- Updated to latest vitaGL commit.
- Now the application UI will respect OS confirm/cancel button settings (O/X).
- Added TitleID info shown for PSVita homebrews.
- Added safety checks for when single themes installations occurred.
- Made progressbar during SharkF00D installation smoother.
- Made possible to cancel applications downloads/installations by pressing the cancel button (O/X).
- Made so that when an application installation fails, eventual downloaded data files are deleted as well.
- Migrated to new backend webhost.

### v.1.9
- Added possibility to skip database update at boot by holding R trigger.
- Added possibility to launch VitaDB Downloader by other means (eg: from other applications).
- Updated to latest vitaGL commit.
- Fixed a vertical alignment mismatch between homebrew entries and install tag info.
- Added an icon showing homebrews providing trophies support.
- Added "Apps with Trophies" filter.
- Added the possibility to view available trophies for PSVita homebrew from the Manage submenu.

### v.1.8
- Fixed an issue causing libshacccg.suprx extraction to fail under certain circumstances.
- Fixed an issue causing kubridge.skprx to not be activated under certain circumstances.
- Made so that libshacccg.suprx extraction will proceed if the app is launched after only some steps are performed instead of restarting from scratch.
- Added more homebrew offered as Nightly releases to the Daemon blacklist (Xash3D, Nazi Zombies Portable).
- Made so that VitaDB Downloader will automatically cleanup storage for leftover of failed homebrew installs.
- Added a new Manage submenu accessible by pressing Select with different features.
- Moved "View Changelog" feature to Manage submenu.
- Added the possibility to launch PSVita homebrew from the Manage submenu.
- Added the possibility to uninstall PSVita/PSP homebrew from the Manage submenu.
- Added the possibility to view homebrew requirements from the Manage submenu.
- Added the possibility to tag an homebrew as Updated from the Manage submenu.
- Made possible to cancel an homebrew install if it has requirements from the requirements popup.
- Added a new filter: Freeware Apps. It will show all the apps not requiring user to supply game data files manually in order to be used.

### v.1.7
- Added an optional auto-updater daemon for installed Vita homebrews. It will check for any homebrew update every hour and at console boot even with VitaDB Downloader closed and send a notification to quickly perform the update.
- Added an auto-downloader and extractor of libshacccg.suprx if this is missing.
- Added proper support for PSP homebrews over different locations based on Adrenaline settings.
- Fixed a bug causing all PSP homebrews to be categorized as Original Games.
- Now requirements popup won't show up for homebrews requiring only libshacccg.suprx since already present if VitaDB Downloader is being used.
- Added an optional kubridge.skprx updater/installer when attempting to install an homebrew requiring it.

### v.1.6
- Fixed a bug causing VitaDB Downloader to be reported always as Outdated.
- Added shadowing support for texts for themes (TextShadow).
- Enhanced icons loading time. Now scrolling through apps will be considerably faster.
- Fixed a bug causing potential filesystem issues due to how icons were stored internally on storage.
- Fixed a bug that caused app bootup to take more time the more apps got downloaded from the app itself.
- Added support for PSP homebrews download and installation (L will now cycle through Vita Homebrews, PSP Homebrews, Themes).
- Moved version value to top left of the screen.
- Added info about current mode (Vita Homebrews, PSP Homebrews, Themes) on the top right of the screen.
- Improved version checking for installed homebrews made in Unity, Game Maker Studio, Godot or Lua. Now they will be correctly detected as Outdated if they are so.
- Made so that Vita homebrews icons are rendered as rounded.
- Made so that, if connection is lost during a download, the download will get resumed at the point where it stopped instead of failing the download.

### v.1.5
- Fixed a bug causing potential crashes if you had a few specific apps installed with a very big eboot.bin file.
- Fixed a bug causing more than a popup to not always show in certain circmustances.
- Fixed a bug causing the first icon of an app being shown after a sort mode change, a search or a filter change to be wrong.
- Fixed a bug causing cached hash files to be incorrectly generated when installing an app (Resulting in slower boot times).
- Fixed a bug that prevented changelog parser to properly escape " char.
- Made cleanup check for leftover unfinished app installs more robust.
- Added app name and version on changelog viewer titlebar.
- Properly aligned Filter text to Search text.
- Added some padding between Filter and Sort mode.
- Added possibility to customize font.
- Added possibility to properly customize any leftover uncustomizable element of the GUI.
- Moved missing icons download from everytime you hit an app lacking the icon to boot time (for all of them).
- Made so that Sort mode can be cycled only with R.
- Added themes downloader and manager with single theme and shuffling themes support (Reachable with L).
- Moved from SoLoud to SDL2 Mixer as audio backend. (Way faster booting time for the background audio playback).
- Fixed a bug causing a crash when opening the changelog viewer in certain circumstances.

### v.1.4
- Added a check after installing an app wether the installation succeded or failed.
- Added proper cleanup of leftover files when an installation is abruptly aborted or fails.
- Fixed a bug causing wrong icon to be shown when performing a search and moving to the first app of the list.
- Fixed a bug causing app info to be shown also when cursor is not on an app.
- Added requirements popup when attempting to install an app having extra requirements for a proper setup (Eg. Plugin requirements or full data files from original game).
- Added possibility to customize color scheme for all GUI elements (ux0:data/NeoVitaDB/themes.ini).
- Added proper tracking of applications state (Not Installed, Outdated, Updated).
- Speeded up boot time. Now VitaDB Downloader will launch approximately one second faster.
- Added possibility to check changelog for the selected app by pressing Select button.
- Renamed "Category: " option to "Filter: ".
- Added possibility to filter applications by Not Installed/Installed/Outdated criterias.
- Fixed a bug causing page down (Right arrow) to not properly reach end of the list when a filter or search was active in certain circumstances.

### v.1.3
- Made so that fast paging down with right arrow will go as down as the very last entry.
- Made visible on the top menubar the currently in-use filter for the apps list.
- Reworded data files installation question to sound more correct.
- Added Smallest and Largest sorting modes.
- Added a dropmenu to change sorting mode (L / R is still usable for cycling between sorting modes).
- Added possibility to cycle between category filters with Square button.
- Using different granularity (B, KB, MB, GB) for homebrew sizes depending on the size itself.
- Added free and total storage info on bottom right of the screen.
- Aligned to left homebrew names in the apps list.
- Added support for backgrounds (Both static (ux0:data/NeoVitaDB/bg.png) and animated (ux0:data/NeoVitaDB/bg.mp4)).
- Added a check prior downloading an app wether free storage is enough to install it.

### v.1.2
- Added possibility to start a search rapidly by pressing the Triangle button.
- Fixed a bug causing the app to crash if the background music file was missing.
- Fixed a bug preventing the app to be updated from within the app itself.
- Added auto updater.
- Fixed an issue causing crackling and stuttering with audio during archive extractions.

### v.1.1
- Added a check when more than a month passed since last boot. If this happens, provide an option to the user to re-download all app icons at once.
- Added possibility to fast scroll apps list with Left/Right arrows.
- Added possibility to fast scroll apps list by moving the scrollbar with left analog.
- Added possibility to instantly return to the top of the list by pressing Circle (Previously it was Circle + Left).
- Fixed a bug not making scrollbar instantly reposition when going to the top of the list.
- Fixed a bug causing selected app icon to get corrupted temporarily after installing an app.
- Added background music (You can disable it or change the track by removing/replacing ux0:data/NeoVitaDB/bg.ogg)
- Fixed a bug causing selected app to change randomly when changing sort mode.

## Credits
- noname120 for the code related to head.bin generation.
- PrincessOfSleeping for the original code related to notification sendings.
- gl33ntwine for helping reversing a small part of Friends app to understand how to intercept notification boots.
- CatoTheYounger and Brandonheat8 for testing the homebrew.
- Once13One for the Livearea assets.
- [phloam](https://www.youtube.com/channel/UCO-COkqKBV1KeBifq0HMK0g) for the audio track used as base for the background music feature.
