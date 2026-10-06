# Interface

The native Qt interface uses slim controls, subtle focus outlines, explicit input/text colors, and a complete palette for each theme. TodoBench's local desktop implementation was reviewed as a behavior/design reference for coherent palettes, density choices and saved appearance. Aeris's implementation is independently written under MIT.

The toolbar exposes three layouts immediately: List for a balanced view, Compact for more accounts, and Cards for a grid that adjusts to the window width. The same choices are in View → Layout, with Ctrl/Cmd+1–3 shortcuts. Selection and keyboard navigation use Qt's list model; accounts do not allocate individual widgets.

Theme choices are System, Light, Dark, Midnight, Ocean, Forest, Violet, Rose and Paper. Theme and layout persist between launches. System retains the platform widget style; named themes use Qt's Fusion style with complete palettes. Search, menus, selection, buttons, dialogs and account surfaces receive matching foreground/background colors. Focus outlines remain visible for keyboard navigation.

Click anywhere on an account to copy its current OTP code. The code area briefly shows an animated checkmark and “Copied”, then returns to the code. Enter and Ctrl/Cmd+C on an account provide the same feedback. Codes may be grouped visually for reading; the clipboard receives the unspaced code. Copying still recalculates at the current time and retains the 30-second ownership-aware clipboard clearing.

Drag an account to arrange its position in List, Compact or Cards. A highlighted insertion line shows the destination; in Cards, use the left or right side of a card to insert before or after it. The view scrolls near its top and bottom edges while dragging. Press Escape or release outside the account area to cancel. Dragging does not copy a code; ordinary clicks still do.

Order is saved automatically in the encrypted collection and survives reopening Aeris. The display changes after saving succeeds. Search and copying codes stay available during a save; another reorder, import, or deletion waits until it finishes. Closing during a save keeps the window open until the operation completes. A failed save retains the previous display order and shows an error; after resolving the problem you can drag again. Search text stays in place during reordering. With search active, drop positions refer to visible neighbors (including after the last visible result); hidden accounts keep their relative order. Replacing the collection through import adopts the incoming export’s order.

Recognized issuers automatically show bundled service logos in all layouts. Logos sit on a neutral backing with a small account-specific color dot, so duplicate accounts keep a visual distinction. Unknown or ambiguous issuers retain the initials, stable color and identifying mark badge. Matching ignores case, spaces, hyphens and underscores and uses exact catalog names, identifiers and explicit aliases (including AWS and github.com). Proton and ProtonMail match their respective catalog artwork. Steam tokens always use Steam artwork. Account email addresses and partial names never infer a service.

The 2,949 standard PNGs from selfh.st/icons revision `2053b70b283ffed5f2cc1424d1e17d9c554a846d` plus 15 supplemental service logos are bundled at up to 128×128, with original proportions and colors. No network access, setting or reimport is needed. Missing or unreadable artwork falls back to the badge. Matching is cached when accounts load; an 8 MiB render cache keys on brand, logical size and display scale. Countdown and copy animations reuse that presentation data.

Artwork attribution and CC-BY-4.0 licensing are in `THIRD_PARTY_NOTICES.md` and `licenses/selfhst-icons-CC-BY-4.0.txt`; application code remains MIT. `assets/brands/catalog.json` records source and output checksums. Maintainers can regenerate with `scripts/brand-icons.py <pinned-upstream.tar.gz>` using the pinned Pillow dependency and `rsvg-convert` for supplemental SVG sources. Run `scripts/brand-icons.py` without an archive to refresh explicit aliases and supplemental images. Alias definitions are in `assets/brand-aliases.json`; the retained supplemental source files, URLs, checksums and per-image license status are in `assets/brand-sources`. Supplemental logos retain their respective owners’ rights, as described in `licenses/brand-supplement-NOTICE.md`. Normal CMake builds use only the prepared assets.

Explicit aliases include wordpress.org/wordpress.com, MongoDB/monogodb, Fidelity Investments/fidelity.com, Chase Bank/chase.com, tastyworks/tastytrade, MySonicWall and DWService domains. Additional bundled brands include GoDaddy, Patelco, GOG, DreamHost, Rocket Mortgage, Rocket Money, Spaceship, Zoho, Intuit, Rockstar Games, Ubisoft and ID.me. Matching still uses the issuer alone and does not accept arbitrary subdomains, suffixes or partial names.

## Previews

Nine lossless PNG captures show all nine appearance choices and all three layouts.
The primary view is **Light · Compact**. These are real application captures at
2× display scale (2008 × 1360 or 2008 × 1680 pixels), including the native Linux/Openbox titlebar, with no image
upscaling. Every pictured account uses `user@example.com` and synthetic secrets.
Click any image to inspect its full-resolution original.

![Light theme, Compact layout, native titlebar](screenshots/light-compact.png)

<table>
  <tr><th width="50%">Dark · Cards</th><th width="50%">Ocean · Cards</th></tr>
  <tr>
    <td><a href="screenshots/dark-cards.png"><img src="screenshots/dark-cards.png" alt="Aeris Dark theme in Cards layout with native titlebar and synthetic user@example.com accounts"></a></td>
    <td><a href="screenshots/ocean-cards.png"><img src="screenshots/ocean-cards.png" alt="Aeris Ocean theme in Cards layout with native titlebar and synthetic user@example.com accounts"></a></td>
  </tr>
  <tr><th width="50%">Violet · Cards</th><th width="50%">Forest · Compact</th></tr>
  <tr>
    <td><a href="screenshots/violet-cards.png"><img src="screenshots/violet-cards.png" alt="Aeris Violet theme in Cards layout with native titlebar and synthetic user@example.com accounts"></a></td>
    <td><a href="screenshots/forest-compact.png"><img src="screenshots/forest-compact.png" alt="Aeris Forest theme in Compact layout with native titlebar and synthetic user@example.com accounts"></a></td>
  </tr>
  <tr><th width="50%">Paper · Compact</th><th width="50%">Rose · List</th></tr>
  <tr>
    <td><a href="screenshots/paper-compact.png"><img src="screenshots/paper-compact.png" alt="Aeris Paper theme in Compact layout with native titlebar and synthetic user@example.com accounts"></a></td>
    <td><a href="screenshots/rose-list.png"><img src="screenshots/rose-list.png" alt="Aeris Rose theme in List layout with native titlebar and synthetic user@example.com accounts"></a></td>
  </tr>
  <tr><th width="50%">Midnight · List</th><th width="50%">System · List</th></tr>
  <tr>
    <td><a href="screenshots/midnight-list.png"><img src="screenshots/midnight-list.png" alt="Aeris Midnight theme in List layout with native titlebar and synthetic user@example.com accounts"></a></td>
    <td><a href="screenshots/system-list.png"><img src="screenshots/system-list.png" alt="Aeris System theme in List layout with native titlebar and synthetic user@example.com accounts"></a></td>
  </tr>
</table>

### Reproduce the captures

Build the `test_ui` target, install `xvfb`, `xauth`, and `openbox` (including the
Breeze-ob theme), then run:

```sh
scripts/screenshots-linux.sh build-linux docs/screenshots
```

The script starts an isolated X11 desktop at 2× scale. The capture harness uses
an in-memory keyring, a temporary encrypted snapshot, and isolated preferences;
it does not read the user's accounts or modify the running application. Window
frames are captured directly from the desktop rather than added afterward.
