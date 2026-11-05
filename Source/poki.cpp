/**************************************************************************/
/*  poki.cpp                                                              */
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

#include "poki.h"

#include "poki_web.h"

using gobu::api;
using gobu::Value;

Poki *Poki::singleton = nullptr;

namespace {

Poki *_of(void *p_instance) {
	return static_cast<Poki *>(p_instance);
}

void _return(GobuVariant *r_return, const Value &p_value) {
	api->variant_destroy(r_return);
	api->variant_new_copy(r_return, p_value.raw());
}

void _is_ready(void *, void *p_instance, const GobuVariant *const *, int64_t, GobuVariant *r_return, GobuCallError *) {
	_return(r_return, Value(_of(p_instance)->is_ready()));
}

void _is_ad_blocked(void *, void *p_instance, const GobuVariant *const *, int64_t, GobuVariant *r_return, GobuCallError *) {
	_return(r_return, Value(_of(p_instance)->is_ad_blocked()));
}

void _move_pill(void *, void *p_instance, const GobuVariant *const *p_args, int64_t, GobuVariant *, GobuCallError *) {
	_of(p_instance)->move_pill(Value::copy_of(p_args[0]).to_float(), Value::copy_of(p_args[1]).to_float());
}

void _login(void *, void *p_instance, const GobuVariant *const *, int64_t, GobuVariant *, GobuCallError *) {
	_of(p_instance)->login();
}

void *_create(void *, GobuObject *p_object) {
	return new Poki(p_object);
}

void _free(void *, void *p_instance) {
	delete _of(p_instance);
}

void _method(const char *p_name, GobuMethodFunc p_func, std::initializer_list<GobuArgInfo> p_args = {}, int p_return_type = -1) {
	const std::vector<GobuArgInfo> args(p_args);
	GobuMethodInfo info = {};
	info.name = p_name;
	info.func = p_func;
	info.has_return = p_return_type >= 0;
	info.return_info = { "", p_return_type >= 0 ? p_return_type : GOBU_TYPE_NIL, 0, nullptr };
	info.arg_count = int32_t(args.size());
	info.args = args.data();
	api->register_method(gobu::plugin, Poki::CLASS_NAME, &info);
}

} // namespace

bool Poki::register_class() {
	GobuClassInfo info = {};
	info.name = CLASS_NAME;
	info.parent = "Object";
	info.create_instance = _create;
	info.free_instance = _free;
	if (!api->register_class(gobu::plugin, &info)) {
		return false;
	}
	_method("is_ready", _is_ready, {}, GOBU_TYPE_BOOL);
	_method("is_ad_blocked", _is_ad_blocked, {}, GOBU_TYPE_BOOL);
	_method("move_pill", _move_pill, { { "top_percent", GOBU_TYPE_FLOAT, 0, nullptr }, { "top_px", GOBU_TYPE_FLOAT, 0, nullptr } });
	_method("login", _login);
	return true;
}

bool Poki::register_provider() {
	GobuServicesProviderInfo info = {};
	info.size = sizeof(info);
	info.name = PROVIDER_NAME;
	info.features = GOBU_SERVICES_ADS | GOBU_SERVICES_REWARDED_ADS | GOBU_SERVICES_GAME_EVENTS | GOBU_SERVICES_SHARE | GOBU_SERVICES_EXTERNAL_LINKS | GOBU_SERVICES_ANALYTICS | GOBU_SERVICES_PLAYER;
	info.init = _init;
	info.shutdown = _shutdown;
	info.loading_finished = _loading_finished;
	info.gameplay_start = _gameplay_start;
	info.gameplay_stop = _gameplay_stop;
	info.commercial_break = _commercial_break;
	info.rewarded_break = _rewarded_break;
	info.shareable_url = _shareable_url;
	info.get_url_param = _get_url_param;
	info.open_external_link = _open_external_link;
	info.measure = _measure;
	info.get_player = _get_player;
	return api->register_services_provider(gobu::plugin, &info);
}

Poki::Poki(GobuObject *p_self) :
		self(p_self) {
	singleton = this;
}

Poki::~Poki() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

// The page's JavaScript.

Value Poki::_js_callback(std::function<void(const gobu::Args &)> p_function) {
	GobuObject *js = gobu::singleton("JavaScriptBridge");
	return js ? gobu::call(js, "create_callback", { gobu::callable(std::move(p_function), self) }) : Value();
}

// create_callback() calls back with one Array, of the JavaScript arguments.
Value Poki::_arg(const gobu::Args &p_args, int p_index) {
	if (p_args.empty() || p_args[0].size() <= p_index) {
		return Value();
	}
	return p_args[0].at(p_index);
}

Value Poki::_bridge_call(const char *p_method, std::initializer_list<Value> p_args) {
	if (bridge.object() == nullptr) {
		return Value();
	}
	return gobu::call(bridge.object(), p_method, p_args);
}

void Poki::start_sdk() {
	if (started) {
		return;
	}
	started = true;
	GobuObject *js = gobu::singleton("JavaScriptBridge");
	if (!js) {
		return; // Not a web game: nothing of Poki here.
	}
	gobu::call(js, "eval", { POKI_WEB_SCRIPT, true });
	bridge = gobu::call(js, "get_interface", { "gobuPoki" });
	if (bridge.object() == nullptr) {
		GOBU_WARNING("Poki: the page could not run the plugin's script: the game goes on without Poki.");
		return;
	}
	const Value done = _js_callback([this](const gobu::Args &p_args) {
		ready = _arg(p_args, 0).to_bool();
		ad_blocked = _arg(p_args, 1).to_bool();
		if (!ready) {
			GOBU_VERBOSE(ad_blocked ? "Poki: the SDK did not start (an ad blocker?): the game goes on without ads." : "Poki: the page has no PokiSDK (plugin.json \"web_head\").");
			return;
		}
		_read_player();
	});
	kept.push_back(done);
	_bridge_call("init", { gobu::has_feature("debug"), done });
}

void Poki::_read_player() {
	const Value done = _js_callback([this](const gobu::Args &p_args) {
		const std::string name = _arg(p_args, 0).str();
		const std::string avatar = _arg(p_args, 1).str();
		const bool changed = name != user_name || avatar != user_avatar_url;
		user_name = name;
		user_avatar_url = avatar;
		if (changed && events) {
			events->player_changed(events->services);
		}
	});
	kept.push_back(done);
	_bridge_call("getUser", { done });
}

void Poki::_break(int64_t p_request, bool p_rewarded) {
	if (bridge.object() == nullptr || !events) {
		if (events) {
			events->ad_finished(events->services, p_request, false); // No SDK: no ad, the game goes on.
		}
		return;
	}
	const Value started_callback = _js_callback([this, p_request](const gobu::Args &) {
		if (events) {
			events->ad_started(events->services, p_request);
		}
	});
	const Value done_callback = _js_callback([this, p_request](const gobu::Args &p_args) {
		const bool shown = _arg(p_args, 0).to_bool();
		if (events) {
			events->ad_finished(events->services, p_request, shown);
		}
		// Not freed while JavaScript runs one of them: on the next frame.
		std::map<int64_t, std::vector<Value>>::iterator found = waiting.find(p_request);
		if (found != waiting.end()) {
			retired.insert(retired.end(), found->second.begin(), found->second.end());
			waiting.erase(found);
		}
	});
	waiting[p_request] = { started_callback, done_callback };
	_bridge_call(p_rewarded ? "rewardedBreak" : "commercialBreak", { started_callback, done_callback });
}

void Poki::move_pill(double p_top_percent, double p_top_px) {
	_bridge_call("movePill", { p_top_percent, p_top_px });
}

void Poki::login() {
	_bridge_call("login");
}

// The provider of Services.

void Poki::_init(void *, const GobuServicesEvents *p_events) {
	if (singleton) {
		singleton->events = p_events;
		singleton->start_sdk();
	}
}

void Poki::_shutdown(void *) {
	if (singleton) {
		singleton->events = nullptr;
		singleton->waiting.clear();
	}
}

void Poki::_loading_finished(void *) {
	if (singleton) {
		singleton->_bridge_call("loadingFinished");
	}
}

void Poki::_gameplay_start(void *) {
	if (singleton) {
		singleton->_bridge_call("gameplayStart");
	}
}

void Poki::_gameplay_stop(void *) {
	if (singleton) {
		singleton->_bridge_call("gameplayStop");
	}
}

void Poki::_commercial_break(void *, int64_t p_request) {
	if (singleton) {
		singleton->_break(p_request, false);
	}
}

void Poki::_rewarded_break(void *, int64_t p_request) {
	if (singleton) {
		singleton->_break(p_request, true);
	}
}

void Poki::_shareable_url(void *, int64_t p_request, const GobuVariant *p_params) {
	Poki *poki = singleton;
	if (!poki || !poki->events) {
		return;
	}
	if (poki->bridge.object() == nullptr) {
		poki->events->url_ready(poki->events->services, p_request, "");
		return;
	}
	// JavaScript takes text: the parameters go as JSON.
	const std::string params = gobu::instantiate("JSON").call("stringify", { Value::copy_of(p_params) }).str();
	const Value done = poki->_js_callback([poki, p_request](const gobu::Args &p_args) {
		if (poki->events) {
			poki->events->url_ready(poki->events->services, p_request, _arg(p_args, 0).str().c_str());
		}
		std::map<int64_t, std::vector<Value>>::iterator found = poki->waiting.find(p_request);
		if (found != poki->waiting.end()) {
			poki->retired.insert(poki->retired.end(), found->second.begin(), found->second.end());
			poki->waiting.erase(found);
		}
	});
	poki->waiting[p_request] = { done };
	poki->_bridge_call("shareableURL", { params, done });
}

void Poki::_get_url_param(void *, const char *p_name, GobuVariant *r_value) {
	if (!singleton || singleton->bridge.object() == nullptr) {
		return;
	}
	const std::string value = singleton->_bridge_call("getURLParam", { p_name }).str();
	if (!value.empty()) {
		api->variant_new_string(r_value, value.c_str(), int64_t(value.size()));
	}
}

void Poki::_open_external_link(void *, const char *p_url) {
	if (singleton) {
		singleton->_bridge_call("openExternalLink", { p_url });
	}
}

void Poki::_measure(void *, const char *p_category, const char *p_what, const char *p_action) {
	if (singleton) {
		singleton->_bridge_call("measure", { p_category, p_what, p_action });
	}
}

bool Poki::_get_player(void *, GobuVariant *r_name, GobuVariant *r_avatar_url) {
	if (!singleton || singleton->user_name.empty()) {
		return false;
	}
	api->variant_new_string(r_name, singleton->user_name.c_str(), int64_t(singleton->user_name.size()));
	api->variant_new_string(r_avatar_url, singleton->user_avatar_url.c_str(), int64_t(singleton->user_avatar_url.size()));
	return true;
}
