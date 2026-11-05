// A PokiSDK for the tests, in place of game-cdn.poki.com's (run_tests.py swaps it into the exported page): it
// answers as Poki's does and writes each call to the console ("POKI_SDK <call>"), in order, for the runner.
(function () {
	const log = function (text) {
		console.log('POKI_SDK ' + text);
	};
	const later = function (ms, value) {
		return new Promise(function (resolve) {
			setTimeout(function () { resolve(value); }, ms);
		});
	};
	window.PokiSDK = {
		init: function () {
			log('init');
			return later(100);
		},
		setDebug: function (on) {
			log('setDebug ' + on);
		},
		gameLoadingFinished: function () {
			log('gameLoadingFinished');
		},
		gameplayStart: function () {
			log('gameplayStart');
		},
		gameplayStop: function () {
			log('gameplayStop');
		},
		commercialBreak: function (onStart) {
			log('commercialBreak');
			if (onStart) {
				onStart();
			}
			return later(300);
		},
		rewardedBreak: function (onStart) {
			log('rewardedBreak');
			if (onStart) {
				onStart();
			}
			return later(300, true);
		},
		shareableURL: function (params) {
			log('shareableURL ' + JSON.stringify(params));
			const query = Object.keys(params).map(function (key) { return 'gd' + key + '=' + encodeURIComponent(params[key]); }).join('&');
			return later(50, 'https://poki.com/en/g/test-game?' + query);
		},
		getURLParam: function (name) {
			log('getURLParam ' + name);
			return name === 'level' ? '3' : '';
		},
		openExternalLink: function (url) {
			log('openExternalLink ' + url);
		},
		measure: function (category, what, action) {
			log('measure ' + category + ' ' + what + ' ' + action);
		},
		movePill: function (topPercent, topPx) {
			log('movePill ' + topPercent + ' ' + topPx);
		},
		getUser: function () {
			log('getUser');
			return later(20, { username: 'TestUser', avatarUrl: 'https://example.com/avatar.png' });
		},
		login: function () {
			log('login');
			return later(20);
		},
	};
})();
