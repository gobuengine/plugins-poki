# The Poki plugin in a real web game: projects/poki_game is exported for the web with the plugin, its page gets the
# fake PokiSDK of fake_poki_sdk.js in place of Poki's (which only answers on poki.com), and headless Chrome plays
# it. The game asks Services for breaks, a share link, a URL parameter, analytics and the player; the runner checks
# what the game got ("POKI_TEST" lines) and what the SDK was asked, in order ("POKI_SDK" lines).
#
#   python run_tests.py [gobu...console.exe]
#
# Needs Gobu built next to this repository (ProjectGobu/Gobu) with its web template without threads, Chrome, and
# the plugin built (`forja build PluginPoki` in Gobu).
import functools
import http.server
import os
import re
import shutil
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
GOBU = os.path.normpath(os.path.join(HERE, '..', '..', '..', 'Gobu'))
EXE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(GOBU, 'Engine', 'Binaries', 'gobu.windows.editor.x86_64.console.exe')
CHROME = os.environ.get('CHROME', 'C:/Program Files/Google/Chrome/Application/chrome.exe')
PROJECT = os.path.join(HERE, 'projects', 'poki_game')
RUN = os.path.join(HERE, '.run')
OUT = os.path.join(RUN, 'web')
PORT = 8071
POKI_SDK = 'https://game-cdn.poki.com/scripts/v2/poki-sdk.js'

failures = []


def check(what, ok, detail=''):
	print(('  ok   ' if ok else '  FAIL ') + what + ('' if ok or not detail else ': ' + detail))
	if not ok:
		failures.append(what)


def export():
	if os.path.exists(RUN):
		shutil.rmtree(RUN)
	os.makedirs(OUT)
	plugin = os.path.join(PROJECT, 'plugins', 'poki')
	if os.path.exists(plugin):
		shutil.rmtree(plugin)
	shutil.copytree(os.path.join(HERE, '..', 'poki'), plugin)
	# Authentication (next to this repository), for Poki's sign-in.
	authentication = os.path.join(HERE, '..', '..', 'Authentication', 'authentication')
	plugin = os.path.join(PROJECT, 'plugins', 'authentication')
	if os.path.exists(plugin):
		shutil.rmtree(plugin)
	if os.path.exists(authentication):
		shutil.copytree(authentication, plugin)
	appdata = os.path.join(RUN, 'appdata')
	os.makedirs(appdata)
	env = dict(os.environ, APPDATA=appdata, LOCALAPPDATA=appdata)
	# The project's first import, then the export.
	subprocess.run([EXE, '--headless', '--path', PROJECT, '--editor', '--quit'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600)
	result = subprocess.run([EXE, '--headless', '--path', PROJECT, '--export-release', 'Web', os.path.join(OUT, 'index.html')], env=env, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=1200)
	open(os.path.join(RUN, 'export.log'), 'w', encoding='utf-8').write(result.stdout + result.stderr)
	page = os.path.join(OUT, 'index.html')
	if not os.path.exists(page):
		print(result.stdout[-2000:], result.stderr[-2000:])
		raise SystemExit('The export made no page: see .run/export.log.')
	html = open(page, encoding='utf-8').read()
	check('the page loads Poki\'s SDK in its <head> (plugin.json "web_head")', '<script src="%s"></script>' % POKI_SDK in html.split('</head>')[0])
	check('the plugin\'s web library is in the export', any(name.startswith('libpoki') for name in os.listdir(OUT)), ', '.join(os.listdir(OUT)))
	# The fake SDK in its place.
	shutil.copy(os.path.join(HERE, 'fake_poki_sdk.js'), os.path.join(OUT, 'fake_poki_sdk.js'))
	open(page, 'w', encoding='utf-8').write(html.replace(POKI_SDK, 'fake_poki_sdk.js'))
	# The same page with the SDK blocked (as an ad blocker does): its script never loads.
	open(os.path.join(OUT, 'blocked.html'), 'w', encoding='utf-8').write(html.replace(POKI_SDK, 'blocked/poki-sdk.js'))


class QuietHandler(http.server.SimpleHTTPRequestHandler):
	def log_message(self, *args):
		pass


def play(page='index.html'):
	handler = functools.partial(QuietHandler, directory=OUT)
	server = http.server.ThreadingHTTPServer(('127.0.0.1', PORT), handler)
	threading.Thread(target=server.serve_forever, daemon=True).start()
	name = page.split('.')[0]
	profile = os.path.join(RUN, 'chrome_' + name)
	log_path = os.path.join(RUN, 'chrome_%s.log' % name)
	log = open(log_path, 'w', encoding='utf-8')
	chrome = subprocess.Popen([CHROME, '--headless=new', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--enable-logging=stderr', '--v=0', '--no-first-run', '--autoplay-policy=no-user-gesture-required', '--user-data-dir=' + profile, 'http://127.0.0.1:%d/%s' % (PORT, page)], stdout=log, stderr=subprocess.STDOUT)
	deadline = time.time() + 90
	text = ''
	while time.time() < deadline:
		time.sleep(1)
		text = open(log_path, encoding='utf-8', errors='replace').read()
		if 'POKI_TEST done' in text:
			time.sleep(1)
			text = open(log_path, encoding='utf-8', errors='replace').read()
			break
	chrome.kill()
	chrome.wait()
	server.shutdown()
	return text


def console(text, prefix):
	lines = []
	# Chrome writes them as `INFO:CONSOLE:<line>] "<text>", source: <file> (<line>)`.
	for match in re.finditer(r'CONSOLE[:(]\d+\)?\] "(.*)", source:', text):
		line = match.group(1)
		if line.startswith(prefix + ' '):
			lines.append(line[len(prefix) + 1:])
	return lines


def main():
	print('Export')
	export()
	print('Play')
	text = play()
	game = console(text, 'POKI_TEST')
	sdk = console(text, 'POKI_SDK')
	check('the game ran to its end', 'done' in game, ' | '.join(game) or 'no POKI_TEST line (see .run/chrome_index.log)')
	expected = {
		'provider=poki': 'Services takes the plugin\'s provider',
		'commercial=true': 'a commercial break shows its ad',
		'rewarded=true': 'a rewarded break gives the reward',
		'url=https://poki.com/en/g/test-game?gdlevel=3': 'shareable_url gives Poki\'s link',
		'param=3': 'get_url_param reads it',
		'ready=true player=TestUser': 'the SDK is ready and the player is Poki\'s',
		'signed_in=poki:fake-poki-token username=TestUser': 'Authentication signs in with Poki (its token, for a server to check)',
	}
	for line, what in expected.items():
		check(what, line in game, ' | '.join(game))
	# What the SDK was asked, in order (getUser goes when it is ready, wherever that falls).
	calls = [call for call in sdk if call not in ('getUser', 'login', 'getToken')]
	order = ['init', 'gameLoadingFinished', 'gameplayStart', 'gameplayStop', 'commercialBreak', 'gameplayStart', 'gameplayStop', 'rewardedBreak', 'gameplayStart', 'shareableURL {"level":"3"}', 'getURLParam level', 'measure level 3 complete', 'movePill 10 20']
	check('the SDK is asked in Poki\'s order (never two starts in a row, stop before a break, start after it)', calls[:len(order)] == order, ' | '.join(calls))
	check('the player is read from the SDK', 'getUser' in sdk)
	check('the sign-in asks Poki for its login and a token', 'login' in sdk and 'getToken' in sdk)
	errors = [line for line in text.splitlines() if 'Uncaught' in line or 'SCRIPT ERROR' in line]
	check('no JavaScript or script errors', not errors, ' | '.join(errors[:5]))

	print('Play without the SDK (blocked)')
	text = play('blocked.html')
	game = console(text, 'POKI_TEST')
	check('the game still runs to its end', 'done' in game, ' | '.join(game) or 'no POKI_TEST line (see .run/chrome_blocked.log)')
	for line, what in {
		'commercial=false': 'a break ends without an ad',
		'rewarded=false': 'a rewarded break gives no reward',
		'url=': 'no share link',
		'ready=false player=': 'the SDK is not ready, and there is no player',
	}.items():
		check(what, line in game, ' | '.join(game))
	errors = [line for line in text.splitlines() if 'Uncaught' in line or 'SCRIPT ERROR' in line]
	check('no JavaScript or script errors without it', not errors, ' | '.join(errors[:5]))
	print('%d failed' % len(failures) if failures else 'All passed')
	return 1 if failures else 0


if __name__ == '__main__':
	sys.exit(main())
