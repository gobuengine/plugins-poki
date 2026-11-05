/**************************************************************************/
/*  poki_web.h                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GOBU ENGINE                               */
/*                        https://gobuengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Gobu Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

// What the plugin puts in the page (JavaScriptBridge.eval): window.gobuPoki, over the PokiSDK the page loaded
// (plugin.json "web_head"). The SDK answers with promises; the plugin gets each answer through a callback
// (JavaScriptBridge.create_callback). Without the SDK (blocked, or another page) everything answers "no".
static const char *POKI_WEB_SCRIPT = R"(
(function () {
	if (window.gobuPoki) {
		return;
	}
	const state = { ready: false, blocked: false };
	let initialized = null; // PokiSDK.init()'s promise: what is asked before it ends waits for it, in order.
	const sdk = function () {
		return (typeof PokiSDK !== 'undefined') ? PokiSDK : null;
	};
	const safely = function (work, otherwise) {
		try {
			return work();
		} catch (error) {
			return otherwise;
		}
	};
	const after = function (work) {
		(initialized || Promise.resolve()).then(function () {
			safely(work);
		});
	};
	// Poki's rules for the page: the arrows, the space bar and the wheel never scroll it around the game.
	const scrolling = ['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', ' ', 'PageUp', 'PageDown'];
	window.addEventListener('keydown', function (event) {
		const tag = event.target && event.target.tagName;
		if (tag !== 'INPUT' && tag !== 'TEXTAREA' && scrolling.indexOf(event.key) >= 0) {
			event.preventDefault();
		}
	});
	window.addEventListener('wheel', function (event) {
		event.preventDefault();
	}, { passive: false });
	window.gobuPoki = {
		init: function (debug, done) {
			const poki = sdk();
			if (!poki) {
				initialized = Promise.resolve();
				done(false, false);
				return;
			}
			if (debug) {
				safely(function () { poki.setDebug(true); });
			}
			// The game loads even when it fails (an ad blocker): it goes on without ads.
			const starting = safely(function () { return poki.init(); }, null);
			initialized = (starting ? Promise.resolve(starting) : Promise.reject(new Error('PokiSDK.init'))).then(function () {
				state.ready = true;
				done(true, false);
			}, function () {
				state.blocked = true;
				done(false, true);
			});
		},
		loadingFinished: function () {
			after(function () { if (state.ready) { sdk().gameLoadingFinished(); } });
		},
		gameplayStart: function () {
			after(function () { if (state.ready) { sdk().gameplayStart(); } });
		},
		gameplayStop: function () {
			after(function () { if (state.ready) { sdk().gameplayStop(); } });
		},
		commercialBreak: function (started, done) {
			after(function () {
				if (!state.ready) {
					done(false);
					return;
				}
				let shown = false;
				sdk().commercialBreak(function () {
					shown = true;
					started();
				}).then(function () {
					done(shown);
				}, function () {
					done(false);
				});
			});
		},
		rewardedBreak: function (started, done) {
			after(function () {
				if (!state.ready) {
					done(false);
					return;
				}
				sdk().rewardedBreak(function () {
					started();
				}).then(function (success) {
					done(!!success);
				}, function () {
					done(false);
				});
			});
		},
		shareableURL: function (params, done) {
			after(function () {
				if (!state.ready) {
					done('');
					return;
				}
				sdk().shareableURL(safely(function () { return JSON.parse(params); }, {})).then(function (url) {
					done(url ? String(url) : '');
				}, function () {
					done('');
				});
			});
		},
		getURLParam: function (name) {
			const poki = sdk();
			if (poki) {
				return safely(function () { return String(poki.getURLParam(name) || ''); }, '');
			}
			// Without the SDK, as it reads them: shareableURL() writes "gd<name>".
			const params = new URLSearchParams(window.location.search);
			return params.get('gd' + name) || params.get(name) || '';
		},
		openExternalLink: function (url) {
			// Now, not after a promise: the browser only opens a page in the player's press.
			if (!state.ready || !safely(function () { sdk().openExternalLink(url); return true; }, false)) {
				window.open(url, '_blank');
			}
		},
		measure: function (category, what, action) {
			after(function () { if (state.ready) { sdk().measure(category, what, action); } });
		},
		movePill: function (topPercent, topPx) {
			after(function () { if (state.ready) { sdk().movePill(topPercent, topPx); } });
		},
		getUser: function (done) {
			after(function () {
				const poki = sdk();
				if (!state.ready || typeof poki.getUser !== 'function') {
					done('', '');
					return;
				}
				Promise.resolve(poki.getUser()).then(function (user) {
					done(user && user.username ? String(user.username) : '', user && user.avatarUrl ? String(user.avatarUrl) : '');
				}, function () {
					done('', '');
				});
			});
		},
		login: function () {
			if (state.ready) {
				safely(function () { sdk().login(); });
			}
		},
	};
})();
)";
