/**************************************************************************/
/*  plugin.cpp                                                            */
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

// Once a frame: frees the page's callbacks of what answered.
static void _frame(void *, double) {
	if (Poki::singleton) {
		Poki::singleton->frame();
	}
}

// The plugin's entry (plugin.json "entry"): Gobu calls it when it loads the library, in a project that enabled
// the plugin. The class and its singleton (freed by Gobu at the end) everywhere, so the editor knows `Poki.*`;
// the provider of Services only in a web game, where Poki's SDK is (elsewhere Services plays without it).
extern "C" GOBU_PLUGIN_EXPORT bool gobu_plugin_init(const GobuPluginApi *p_api, GobuPluginHandle p_plugin, GobuPluginInfo *r_info) {
	if (p_api->version < 4 || p_api->size < sizeof(GobuPluginApi)) {
		return false; // A Gobu older than this plugin: register_services_provider() is version 4.
	}
	gobu::api = p_api;
	gobu::plugin = p_plugin;
	r_info->userdata = nullptr;
	r_info->deinitialize = nullptr;
	if (!Poki::register_class()) {
		return false;
	}
	if (!p_api->register_singleton(p_plugin, Poki::CLASS_NAME, Poki::CLASS_NAME)) {
		return false;
	}
#ifdef __EMSCRIPTEN__
	if (!Poki::register_provider()) {
		return false;
	}
#endif
	p_api->add_process_callback(p_plugin, _frame, nullptr);
	return true;
}
