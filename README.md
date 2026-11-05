# Poki

Publish a Gobu web game on [Poki](https://poki.com), as a Gobu plugin.

The game talks to Poki through Gobu's **Services**, the same API every game service uses: the plugin is its
`poki` provider. Ads, game events, share links, URL parameters, analytics and the player go there:

```grost
async fn on_revive_pressed() {
	if await Services.rewarded_break() {   # Poki's rewarded break: true when the player saw it to the end.
		revive()
	}
}
```

Services also sends the game events by itself (loading finished when the main scene is ready, gameplay from
the player's first input, stop and start around pauses and ads) and mutes the game and ignores its input during
an ad, as Poki asks. The plugin keeps the arrows, the space bar and the wheel from scrolling the page around the
game. The `Poki` singleton has what only Poki has: `Poki.move_pill()`, `Poki.is_ad_blocked()`, `Poki.login()`,
`Poki.is_ready()`.

- `poki/`: the plugin as a project has it: its `plugin.json` (its `web_head` puts Poki's SDK in the exported
  page's `<head>`), `doc_classes/Poki.xml` (the editor's help of the `Poki` class), `icon.svg` and its libraries
  in `bin/`. Install it from Gobu's **Edit > Plugins**, or copy it to `<project>/plugins/poki` by hand.
- `Source/`: its code, C++ against Gobu's plugin interface only (`gobu_plugin.h`, version 4). The web library
  is a WebAssembly side module for the templates without threads; it reaches Poki's SDK through Gobu's
  JavaScriptBridge. The Windows library only has the `Poki` singleton, so the editor knows `Poki.*` (it does
  nothing there, and Services plays its test ads). Built with Gobu's forja: this repository goes in
  `ProjectGobu/Plugins/Poki`, next to Gobu, listed in Gobu's `Engine/Source/external_plugins.txt`; then
  `forja build PluginPoki` builds both libraries into `poki/bin` and copies the plugin next to the editor.
- `Tests/`: `python run_tests.py` exports a test game for the web with the plugin and plays it in headless
  Chrome against a fake PokiSDK (`Tests/fake_poki_sdk.js`), checking what the game got and what the SDK was asked.

Before sending a game to Poki, try it in the [Poki Inspector](https://inspector.poki.dev): export it for the web
(without **Thread Support**) and upload the folder. Guide: https://gobuengine.org/guide/15-services
