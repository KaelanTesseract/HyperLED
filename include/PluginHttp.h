/*
 * HyperLED - Open Source LED Controller
 *
 * Copyright (c) 2026 Dennis Guse
 *
 * Licensed under the EUPL, Version 1.2 or – as soon they will be approved by
 * the European Commission - subsequent versions of the EUPL (the "Licence");
 * You may not use this work except in compliance with the Licence.
 * You may obtain a copy of the Licence at:
 *
 * https://joinup.ec.europa.eu/software/page/eupl
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the Licence is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing permissions and
 * limitations under the Licence.
 */
#pragma once

#include <Arduino.h>
#include <vector>

// One GET request for a plugin source, with a limit on how long it may take and on how much it may
// bring back. It blocks until it is done - for as long as the time limit - so it is only ever called
// from the plugin task, never from the main loop or the web server's task.
namespace PluginHttp {

struct Header {
    String name;
    String value;
};

struct Request {
    String url;  // http:// or https://
    std::vector<Header> headers;
    uint16_t timeoutMs = 5000;
    size_t maxBytes = 8192;
    bool followRedirects = false;
};

struct Response {
    int status = 0;
    String body;
    // "", "bad_url", "no_connection", "http_<code>", "too_large", "read_failed"
    String error;
};

bool get(const Request& req, Response& res);

}  // namespace PluginHttp
