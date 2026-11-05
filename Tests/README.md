# Tests

`python run_tests.py`: the plugin's real web library in a real web game. `projects/poki_game` is exported for the
web with the plugin, and its page gets `fake_poki_sdk.js` in place of Poki's SDK (which only answers on poki.com).
Headless Chrome plays it twice:

- **With the SDK:** Services takes the `poki` provider. The game gets a commercial break with an ad, a rewarded
  break that gives the reward, a share link, a URL parameter and the player. The SDK is asked in Poki's order:
  loading, gameplay start, stop before each break, start after it, never two of a kind in a row.
- **Without it** (blocked, as an ad blocker does): the game still runs to its end. Breaks end without an ad and
  give no reward, and there is no link and no player.

The export also has to put Poki's SDK in the page's `<head>` (plugin.json `web_head`) and the plugin's web
library next to it.

Needs Python 3, Chrome (or `CHROME=<path>`) and Gobu built next to this repository (`ProjectGobu/Gobu`) with its
web template without threads (`forja build TemplateWeb_r2d_nothreads`), or `python run_tests.py <gobu...console.exe>`.
Build the plugin first (`forja build PluginPoki` in Gobu): the runner copies `../poki` into the project. What a
run leaves (the export, Chrome's logs) is in `.run/`.

By hand, before sending a game to Poki: upload the exported folder to the Poki Inspector
(https://inspector.poki.dev). It plays the game with Poki's real SDK and checks its events and ads.
