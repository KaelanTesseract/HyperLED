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
#include "PluginHttp.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace PluginHttp {

namespace {

// Collects what the HTTP client writes, up to a limit. A longer answer is cut off and reported.
// Going through a stream rather than getString() matters: the client decodes a chunked answer on
// the way, and a source that sends megabytes cannot fill the memory.
class CappedString : public Stream {
public:
    explicit CappedString(size_t cap) : _cap(cap) {}
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t* buf, size_t len) override {
        if (_data.length() + len > _cap) {
            _overflow = true;
            return 0;
        }
        _data.concat((const char*)buf, (unsigned int)len);
        return len;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    bool overflow() const { return _overflow; }
    String& data() { return _data; }

private:
    size_t _cap;
    String _data;
    bool _overflow = false;
};

}  // namespace

bool get(const Request& req, Response& res) {
    res = Response();
    bool secure = req.url.startsWith("https://");
    if (!secure && !req.url.startsWith("http://")) {
        res.error = "bad_url";
        return false;
    }

    WiFiClient plain;
    WiFiClientSecure tls;
    if (secure) tls.setInsecure();  // encrypts; the certificate is not checked, as everywhere else
    HTTPClient http;
    http.setTimeout(req.timeoutMs);
    http.setConnectTimeout(req.timeoutMs);
    http.setFollowRedirects(req.followRedirects ? HTTPC_STRICT_FOLLOW_REDIRECTS : HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setUserAgent("HyperLED-Plugin");

    bool begun = secure ? http.begin(tls, req.url) : http.begin(plain, req.url);
    if (!begun) {
        res.error = "bad_url";
        return false;
    }
    for (const Header& h : req.headers) http.addHeader(h.name, h.value);

    int code = http.GET();
    res.status = code;
    if (code <= 0) {
        res.error = "no_connection";
        http.end();
        return false;
    }
    if (code != 200) {
        res.error = "http_" + String(code);
        http.end();
        return false;
    }
    int size = http.getSize();  // -1 when the answer is chunked
    if (size > (int)req.maxBytes) {
        res.error = "too_large";
        http.end();
        return false;
    }

    CappedString sink(req.maxBytes);
    int written = http.writeToStream(&sink);
    http.end();
    if (sink.overflow()) {
        res.error = "too_large";
        return false;
    }
    if (written < 0) {
        res.error = "read_failed";
        return false;
    }
    res.body = sink.data();
    return true;
}

}  // namespace PluginHttp
