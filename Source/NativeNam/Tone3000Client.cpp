#include "Tone3000Client.h"
#include "AppSettings.h"
#include "NamLibrary.h"
#include "DevLog.h"

namespace
{
struct Sha256
{
    uint32_t state[8] {};
    uint64_t bitlen = 0;
    uint8_t data[64] {};
    size_t datalen = 0;

    static uint32_t rotr (uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    static uint32_t ch (uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static uint32_t maj (uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static uint32_t ep0 (uint32_t x) { return rotr (x, 2) ^ rotr (x, 13) ^ rotr (x, 22); }
    static uint32_t ep1 (uint32_t x) { return rotr (x, 6) ^ rotr (x, 11) ^ rotr (x, 25); }
    static uint32_t sig0 (uint32_t x) { return rotr (x, 7) ^ rotr (x, 18) ^ (x >> 3); }
    static uint32_t sig1 (uint32_t x) { return rotr (x, 17) ^ rotr (x, 19) ^ (x >> 10); }

    Sha256()
    {
        state[0] = 0x6a09e667; state[1] = 0xbb67ae85; state[2] = 0x3c6ef372; state[3] = 0xa54ff53a;
        state[4] = 0x510e527f; state[5] = 0x9b05688c; state[6] = 0x1f83d9ab; state[7] = 0x5be0cd19;
    }

    void transform()
    {
        static const uint32_t k[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
        };
        uint32_t m[64];
        for (int i = 0, j = 0; i < 16; ++i, j += 4)
            m[i] = ((uint32_t) data[j] << 24) | ((uint32_t) data[j + 1] << 16)
                 | ((uint32_t) data[j + 2] << 8) | ((uint32_t) data[j + 3]);
        for (int i = 16; i < 64; ++i)
            m[i] = sig1 (m[i - 2]) + m[i - 7] + sig0 (m[i - 15]) + m[i - 16];

        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
        for (int i = 0; i < 64; ++i)
        {
            const uint32_t t1 = h + ep1 (e) + ch (e, f, g) + k[i] + m[i];
            const uint32_t t2 = ep0 (a) + maj (a, b, c);
            h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }

    void update (const uint8_t* in, size_t len)
    {
        for (size_t i = 0; i < len; ++i)
        {
            data[datalen++] = in[i];
            if (datalen == 64) { transform(); bitlen += 512; datalen = 0; }
        }
    }

    void final (uint8_t out[32])
    {
        size_t i = datalen;
        data[i++] = 0x80;
        if (i > 56)
        {
            while (i < 64) data[i++] = 0;
            transform();
            i = 0;
        }
        while (i < 56) data[i++] = 0;
        bitlen += datalen * 8;
        data[63] = (uint8_t) bitlen;
        data[62] = (uint8_t) (bitlen >> 8);
        data[61] = (uint8_t) (bitlen >> 16);
        data[60] = (uint8_t) (bitlen >> 24);
        data[59] = (uint8_t) (bitlen >> 32);
        data[58] = (uint8_t) (bitlen >> 40);
        data[57] = (uint8_t) (bitlen >> 48);
        data[56] = (uint8_t) (bitlen >> 56);
        transform();
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 8; ++k)
                out[j + k * 4] = (uint8_t) ((state[k] >> (24 - j * 8)) & 0xff);
    }
};
} // namespace

void Tone3000Client::loadTokensFromSettings()
{
    auto f = AppSettings::get().getSettingsFile();
    if (! f.existsAsFile()) return;
    if (auto xml = juce::XmlDocument::parse (f))
    {
        publishableKey = xml->getStringAttribute ("t3kPubKey");
        tokens.accessToken  = xml->getStringAttribute ("t3kAccess");
        tokens.refreshToken = xml->getStringAttribute ("t3kRefresh");
        tokens.expiresAtMs  = xml->getStringAttribute ("t3kExpires").getLargeIntValue();
        userDisplayName     = xml->getStringAttribute ("t3kUser");
    }
}

void Tone3000Client::saveTokensToSettings() const
{
    auto f = AppSettings::get().getSettingsFile();
    std::unique_ptr<juce::XmlElement> xml;
    if (f.existsAsFile())
        xml = juce::XmlDocument::parse (f);
    if (xml == nullptr)
        xml = std::make_unique<juce::XmlElement> ("QuadnonCortexSettings");

    xml->setAttribute ("t3kPubKey",  publishableKey);
    xml->setAttribute ("t3kAccess",  tokens.accessToken);
    xml->setAttribute ("t3kRefresh", tokens.refreshToken);
    xml->setAttribute ("t3kExpires", juce::String (tokens.expiresAtMs));
    xml->setAttribute ("t3kUser",    userDisplayName);
    xml->writeTo (f);
}

juce::String Tone3000Client::makeCodeVerifier()
{
    juce::MemoryBlock mb (32);
    auto* p = static_cast<juce::uint8*> (mb.getData());
    juce::Random& r = juce::Random::getSystemRandom();
    for (int i = 0; i < 32; ++i)
        p[i] = (juce::uint8) r.nextInt (256);
    return base64UrlEncode (mb.getData(), mb.getSize());
}

juce::String Tone3000Client::base64UrlEncode (const void* data, size_t n)
{
    auto s = juce::Base64::toBase64 (data, n)
                 .replaceCharacter ('+', '-')
                 .replaceCharacter ('/', '_');
    while (s.endsWithChar ('='))
        s = s.dropLastCharacters (1);
    return s;
}

juce::String Tone3000Client::makeCodeChallenge (const juce::String& verifier)
{
    Sha256 sha;
    const auto* utf8 = (const uint8_t*) verifier.toRawUTF8();
    sha.update (utf8, (size_t) verifier.getNumBytesAsUTF8());
    uint8_t digest[32];
    sha.final (digest);
    return base64UrlEncode (digest, 32);
}

juce::String Tone3000Client::httpRequest (const juce::String& method, const juce::String& urlStr,
                                          const juce::String& body, const juce::String& contentType,
                                          bool auth, juce::String& error)
{
    juce::URL url (urlStr);
    juce::String headers = "User-Agent: quadnoncortex\r\n";
    if (contentType.isNotEmpty())
        headers += "Content-Type: " + contentType + "\r\n";
    if (auth && tokens.accessToken.isNotEmpty())
        headers += "Authorization: Bearer " + tokens.accessToken + "\r\n";

    if (method == "POST" && body.isNotEmpty())
        url = url.withPOSTData (body);

    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                    .withExtraHeaders (headers)
                    .withConnectionTimeoutMs (30000)
                    .withHttpRequestCmd (method);

    std::unique_ptr<juce::InputStream> in (url.createInputStream (opts));
    if (in == nullptr)
    {
        error = "Network request failed";
        return {};
    }
    return in->readEntireStreamAsString();
}

bool Tone3000Client::refreshAccessToken (juce::String& error)
{
    if (tokens.refreshToken.isEmpty() || publishableKey.isEmpty())
    {
        error = "No refresh token";
        return false;
    }
    juce::StringPairArray fields;
    fields.set ("grant_type", "refresh_token");
    fields.set ("refresh_token", tokens.refreshToken);
    fields.set ("client_id", publishableKey);
    juce::String body;
    for (auto& k : fields.getAllKeys())
    {
        if (body.isNotEmpty()) body << "&";
        body << juce::URL::addEscapeChars (k, true) << "="
             << juce::URL::addEscapeChars (fields[k], true);
    }
    auto resp = httpRequest ("POST", juce::String (kApiBase) + "/oauth/token", body,
                             "application/x-www-form-urlencoded", false, error);
    if (auto parsed = juce::JSON::parse (resp))
        if (auto* o = parsed.getDynamicObject())
        {
            tokens.accessToken  = o->getProperty ("access_token").toString();
            if (o->hasProperty ("refresh_token"))
                tokens.refreshToken = o->getProperty ("refresh_token").toString();
            int exp = (int) o->getProperty ("expires_in");
            if (exp <= 0) exp = 3600;
            tokens.expiresAtMs = juce::Time::currentTimeMillis() + (juce::int64) exp * 1000;
            saveTokensToSettings();
            return tokens.valid();
        }
    error = "Token refresh failed";
    return false;
}

bool Tone3000Client::ensureAccessToken (juce::String& error)
{
    if (! tokens.valid())
    {
        error = "Not logged in";
        return false;
    }
    if (juce::Time::currentTimeMillis() > tokens.expiresAtMs - 30000)
        return refreshAccessToken (error);
    return true;
}

bool Tone3000Client::exchangeCodeForTokens (const juce::String& code, juce::String& error)
{
    if (publishableKey.isEmpty() || pendingCodeVerifier.isEmpty())
    {
        error = "Login session expired — try Connect again";
        return false;
    }
    juce::StringPairArray fields;
    fields.set ("grant_type", "authorization_code");
    fields.set ("code", code);
    fields.set ("redirect_uri", kDefaultRedirect);
    fields.set ("client_id", publishableKey);
    fields.set ("code_verifier", pendingCodeVerifier);
    juce::String body;
    for (auto& k : fields.getAllKeys())
    {
        if (body.isNotEmpty()) body << "&";
        body << juce::URL::addEscapeChars (k, true) << "="
             << juce::URL::addEscapeChars (fields[k], true);
    }
    auto resp = httpRequest ("POST", juce::String (kApiBase) + "/oauth/token", body,
                             "application/x-www-form-urlencoded", false, error);
    DevLog::log ("T3K token response: " + resp.substring (0, 200));
    if (auto parsed = juce::JSON::parse (resp))
        if (auto* o = parsed.getDynamicObject())
        {
            tokens.accessToken  = o->getProperty ("access_token").toString();
            tokens.refreshToken = o->getProperty ("refresh_token").toString();
            int exp = (int) o->getProperty ("expires_in");
            if (exp <= 0) exp = 3600;
            tokens.expiresAtMs = juce::Time::currentTimeMillis() + (juce::int64) exp * 1000;
            if (! tokens.valid())
            {
                error = o->getProperty ("error_description").toString();
                if (error.isEmpty()) error = "Token exchange failed";
                return false;
            }
            fetchUserProfile();
            saveTokensToSettings();
            return true;
        }
    error = "Token exchange failed";
    return false;
}

void Tone3000Client::fetchUserProfile()
{
    juce::String error;
    auto body = httpRequest ("GET", juce::String (kApiBase) + "/user", {}, {}, true, error);
    if (auto parsed = juce::JSON::parse (body))
        if (auto* o = parsed.getDynamicObject())
        {
            userDisplayName = o->getProperty ("username").toString();
            if (userDisplayName.isEmpty())
                userDisplayName = o->getProperty ("name").toString();
            if (userDisplayName.isEmpty())
                userDisplayName = o->getProperty ("email").toString();
        }
}

void Tone3000Client::startLoopbackServer()
{
    stopLoopbackServer();
    loopbackRunning = true;
    loopbackThread = std::make_unique<std::thread> ([this]
    {
        juce::StreamingSocket server;
        if (! server.createListener (kLoopbackPort, "0.0.0.0"))
        {
            DevLog::log ("T3K: failed to bind loopback port " + juce::String (kLoopbackPort));
            juce::MessageManager::callAsync ([this]
            {
                if (loginCallback)
                    loginCallback (false, "Could not open local port " + juce::String (kLoopbackPort)
                                              + ". Close other apps using it, or paste the code manually.");
            });
            loopbackRunning = false;
            return;
        }
        DevLog::log ("T3K: loopback listening on 0.0.0.0:" + juce::String (kLoopbackPort));
        while (loopbackRunning.load())
        {
            std::unique_ptr<juce::StreamingSocket> client (server.waitForNextConnection());
            if (client == nullptr)
                continue;

            // Read HTTP request (first line is enough for ?code=)
            juce::MemoryBlock mb;
            char buf[1024];
            juce::String request;
            // Block briefly for first chunk so we don't parse an empty partial request
            {
                const int got = client->read (buf, sizeof (buf) - 1, true);
                if (got > 0)
                {
                    buf[got] = 0;
                    request += buf;
                }
            }
            for (int n = 0; n < 32 && ! request.contains ("\r\n\r\n") && ! request.contains ("code="); ++n)
            {
                const int got = client->read (buf, sizeof (buf) - 1, false);
                if (got <= 0) break;
                buf[got] = 0;
                request += buf;
            }

            DevLog::log ("T3K loopback request: " + request.substring (0, 180));

            // Parse GET /callback?code=...&state=...  (robust — code may be anywhere in request)
            juce::String code, state;
            {
                auto extract = [&] (const juce::String& key) -> juce::String
                {
                    const juce::String needle = key + "=";
                    const int i = request.indexOfIgnoreCase (0, needle);
                    if (i < 0) return {};
                    auto v = request.substring (i + needle.length());
                    // stop at & space CR LF HTTP
                    for (auto sep : { "&", " ", "\r", "\n", "?" })
                        v = v.upToFirstOccurrenceOf (sep, false, false);
                    return juce::URL::removeEscapeChars (v.trim());
                };
                code  = extract ("code");
                state = extract ("state");
                DevLog::log ("T3K parsed code='" + code.substring (0, 12) + "...' state='" + state.substring (0, 8) + "'");
            }

            // Always respond so the browser doesn't hang
            juce::String html;
            if (code.isNotEmpty())
            {
                html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Connected</title></head>"
                       "<body style='font-family:system-ui,sans-serif;background:#0d1117;color:#e6edf3;"
                       "display:flex;align-items:center;justify-content:center;height:100vh;margin:0'>"
                       "<div style='text-align:center'><h1 style='color:#3fb950'>Connected</h1>"
                       "<p>You can close this tab and return to quadnoncortex.</p></div>"
                       "<script>setTimeout(function(){window.close()},800);</script></body></html>";
            }
            else if (request.containsIgnoreCase ("/callback"))
            {
                html = "<html><body style='font-family:sans-serif;background:#111;color:#eee;text-align:center;padding:40px'>"
                       "<h2>Login incomplete</h2><p>No code in request. Paste the code from the URL into the app.</p>"
                       "<p style='font-size:12px;opacity:0.6'>" + request.substring (0, 120) + "</p></body></html>";
            }
            else
            {
                html = "ok";
            }
            juce::String response;
            response << "HTTP/1.1 200 OK\r\n"
                     << "Content-Type: text/html; charset=utf-8\r\n"
                     << "Connection: close\r\n"
                     << "Content-Length: " << juce::String (html.getNumBytesAsUTF8()) << "\r\n\r\n"
                     << html;
            client->write (response.toRawUTF8(), (int) response.getNumBytesAsUTF8());
            client->close();

            if (code.isNotEmpty())
            {
                juce::String error;
                const bool ok = exchangeCodeForTokens (code, error);
                juce::MessageManager::callAsync ([this, ok, error]
                {
                    stopLoopbackServer();
                    if (loginCallback)
                        loginCallback (ok, ok ? ("Logged in as " + userDisplayName) : error);
                });
                break;
            }
        }
        server.close();
        loopbackRunning = false;
    });
}

void Tone3000Client::stopLoopbackServer()
{
    loopbackRunning = false;
    // Nudge the listener by connecting so waitForNextConnection returns
    {
        juce::StreamingSocket poke;
        poke.connect ("127.0.0.1", kLoopbackPort, 200);
    }
    if (loopbackThread && loopbackThread->joinable())
    {
        if (std::this_thread::get_id() != loopbackThread->get_id())
            loopbackThread->join();
    }
    loopbackThread.reset();
}

void Tone3000Client::cancelLogin()
{
    stopLoopbackServer();
}

void Tone3000Client::beginLogin (std::function<void(bool, juce::String)> onDone)
{
    loginCallback = std::move (onDone);
    if (publishableKey.isEmpty())
    {
        if (loginCallback)
            loginCallback (false, "Enter your TONE3000 publishable key first (Settings > API Keys)");
        return;
    }

    // Stop any previous attempt cleanly
    stopLoopbackServer();

    pendingCodeVerifier = makeCodeVerifier();
    pendingState = juce::Uuid().toDashedString();
    const auto challenge = makeCodeChallenge (pendingCodeVerifier);

    // Listener MUST be up before the browser redirects (Windows localhost may use IPv4 or IPv6)
    startLoopbackServer();
    juce::Thread::sleep (150); // give bind a moment

    // Full API Access flow — prompt omitted (see tone3000.com/api Authentication)
    juce::String url = juce::String (kApiBase) + "/oauth/authorize"
        + "?client_id=" + juce::URL::addEscapeChars (publishableKey, true)
        + "&redirect_uri=" + juce::URL::addEscapeChars (kDefaultRedirect, true)
        + "&response_type=code"
        + "&code_challenge=" + juce::URL::addEscapeChars (challenge, true)
        + "&code_challenge_method=S256"
        + "&state=" + juce::URL::addEscapeChars (pendingState, true);

    DevLog::log ("T3K authorize URL: " + url);
    juce::URL (url).launchInDefaultBrowser();

    if (loginCallback)
        loginCallback (false, "Sign in on the browser. This app will finish automatically when TONE3000 redirects back.");
}

void Tone3000Client::completeLoginWithCode (const juce::String& code,
                                            std::function<void(bool, juce::String)> onDone)
{
    juce::Thread::launch ([this, code, onDone]
    {
        juce::String error;
        const bool ok = exchangeCodeForTokens (code.trim(), error);
        juce::MessageManager::callAsync ([onDone, ok, error, this]
        {
            onDone (ok, ok ? ("Logged in as " + userDisplayName) : error);
        });
    });
}

void Tone3000Client::logout()
{
    cancelLogin();
    tokens = {};
    userDisplayName = {};
    saveTokensToSettings();
}

static Tone3000Client::ToneInfo parseToneObject (juce::DynamicObject* o)
{
    Tone3000Client::ToneInfo t;
    if (o == nullptr) return t;
    t.id = (int) o->getProperty ("id");
    // API uses title (not name) and gear (not gears)
    t.name = o->getProperty ("title").toString();
    if (t.name.isEmpty())
        t.name = o->getProperty ("name").toString();
    t.gears = o->getProperty ("gear").toString();
    if (t.gears.isEmpty())
        t.gears = o->getProperty ("gears").toString();
    t.description = o->getProperty ("description").toString();
    t.format = o->getProperty ("format").toString();
    t.downloads = (int) o->getProperty ("downloads_count");
    if (auto* u = o->getProperty ("user").getDynamicObject())
        t.userName = u->getProperty ("username").toString();
    return t;
}

static Tone3000Client::SearchResult parseSearchBody (const juce::String& body, juce::String& error)
{
    Tone3000Client::SearchResult r;
    if (body.isEmpty())
    {
        if (error.isEmpty()) error = "Empty response";
        r.error = error;
        return r;
    }
    auto parsed = juce::JSON::parse (body);
    if (auto* root = parsed.getDynamicObject())
    {
        r.page = (int) root->getProperty ("page");
        r.pageSize = (int) root->getProperty ("page_size");
        r.total = (int) root->getProperty ("total");
        r.totalPages = (int) root->getProperty ("total_pages");
        if (r.page <= 0) r.page = 1;
        if (r.pageSize <= 0) r.pageSize = 10;
        if (r.totalPages <= 0) r.totalPages = 1;
        juce::var list = root->getProperty ("data");
        if (auto* arr = list.getArray())
            for (auto& item : *arr)
                r.tones.add (parseToneObject (item.getDynamicObject()));
    }
    return r;
}

void Tone3000Client::searchTones (const juce::String& query, const juce::String& gears, const juce::String& sort,
                                  int page, int pageSize, int architecture,
                                  std::function<void(SearchResult)> onDone)
{
    juce::Thread::launch ([this, query, gears, sort, page, pageSize, architecture, onDone]
    {
        juce::String error;
        SearchResult result;
        if (! ensureAccessToken (error))
        {
            result.error = error;
            juce::MessageManager::callAsync ([onDone, result] { onDone (result); });
            return;
        }
        const int ps = juce::jlimit (1, 10, pageSize);
        const int pg = juce::jmax (1, page);
        juce::String path = juce::String (kApiBase) + "/tones/search"
            + "?page=" + juce::String (pg)
            + "&page_size=" + juce::String (ps)
            + "&architecture=" + juce::String (architecture);
        if (query.isNotEmpty()) path += "&query=" + juce::URL::addEscapeChars (query, true);
        if (gears.isNotEmpty()) path += "&gears=" + juce::URL::addEscapeChars (gears, true);
        if (sort.isNotEmpty())  path += "&sort=" + juce::URL::addEscapeChars (sort, true);
        auto body = httpRequest ("GET", path, {}, {}, true, error);
        result = parseSearchBody (body, error);
        if (result.error.isEmpty() && error.isNotEmpty() && result.tones.isEmpty())
            result.error = error;
        juce::MessageManager::callAsync ([onDone, result] { onDone (result); });
    });
}

void Tone3000Client::listCreatedTones (int page, int pageSize, std::function<void(SearchResult)> onDone)
{
    juce::Thread::launch ([this, page, pageSize, onDone]
    {
        juce::String error;
        SearchResult result;
        if (! ensureAccessToken (error))
        {
            result.error = error;
            juce::MessageManager::callAsync ([onDone, result] { onDone (result); });
            return;
        }
        juce::String path = juce::String (kApiBase) + "/tones/created?page=" + juce::String (juce::jmax (1, page))
            + "&page_size=" + juce::String (juce::jlimit (1, 10, pageSize));
        auto body = httpRequest ("GET", path, {}, {}, true, error);
        result = parseSearchBody (body, error);
        juce::MessageManager::callAsync ([onDone, result] { onDone (result); });
    });
}

void Tone3000Client::listFavoritedTones (int page, int pageSize, std::function<void(SearchResult)> onDone)
{
    juce::Thread::launch ([this, page, pageSize, onDone]
    {
        juce::String error;
        SearchResult result;
        if (! ensureAccessToken (error))
        {
            result.error = error;
            juce::MessageManager::callAsync ([onDone, result] { onDone (result); });
            return;
        }
        juce::String path = juce::String (kApiBase) + "/tones/favorited?page=" + juce::String (juce::jmax (1, page))
            + "&page_size=" + juce::String (juce::jlimit (1, 10, pageSize));
        auto body = httpRequest ("GET", path, {}, {}, true, error);
        result = parseSearchBody (body, error);
        juce::MessageManager::callAsync ([onDone, result] { onDone (result); });
    });
}

void Tone3000Client::listModels (int toneId, int architecture,
                                 std::function<void(juce::Array<ModelInfo>, juce::String)> onDone)
{
    juce::Thread::launch ([this, toneId, architecture, onDone]
    {
        juce::String error;
        juce::Array<ModelInfo> results;
        if (! ensureAccessToken (error))
        {
            juce::MessageManager::callAsync ([onDone, error] { onDone ({}, error); });
            return;
        }
        juce::String path = juce::String (kApiBase) + "/models?tone_id=" + juce::String (toneId)
                          + "&architecture=" + juce::String (architecture);
        auto body = httpRequest ("GET", path, {}, {}, true, error);
        if (auto parsed = juce::JSON::parse (body))
        {
            auto* root = parsed.getDynamicObject();
            juce::var list = root ? root->getProperty ("data") : parsed;
            if (auto* arr = list.getArray())
                for (auto& item : *arr)
                    if (auto* o = item.getDynamicObject())
                    {
                        ModelInfo m;
                        m.id = (int) o->getProperty ("id");
                        m.name = o->getProperty ("name").toString();
                        m.modelUrl = o->getProperty ("model_url").toString();
                        m.architecture = (int) o->getProperty ("architecture");
                        if (m.architecture == 0)
                        {
                            auto av = o->getProperty ("architecture_version").toString();
                            m.architecture = av.getIntValue();
                            if (m.architecture == 0 && av == "2") m.architecture = 2;
                            if (m.architecture == 0 && av == "1") m.architecture = 1;
                        }
                        m.format = o->getProperty ("format").toString();
                        if (m.format.isEmpty())
                            m.format = o->getProperty ("size").toString();
                        results.add (m);
                    }
        }
        juce::MessageManager::callAsync ([onDone, results] { onDone (results, {}); });
    });
}

void Tone3000Client::downloadModel (const ModelInfo& model,
                                    std::function<void(juce::File, juce::String)> onDone)
{
    juce::Thread::launch ([this, model, onDone]
    {
        juce::String error;
        if (! ensureAccessToken (error))
        {
            juce::MessageManager::callAsync ([onDone, error] { onDone ({}, error); });
            return;
        }
        if (model.modelUrl.isEmpty())
        {
            juce::MessageManager::callAsync ([onDone] { onDone ({}, "No model URL"); });
            return;
        }
        juce::URL url (model.modelUrl);
        auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withExtraHeaders ("Authorization: Bearer " + tokens.accessToken + "\r\nUser-Agent: quadnoncortex\r\n")
                        .withConnectionTimeoutMs (60000);
        std::unique_ptr<juce::InputStream> in (url.createInputStream (opts));
        if (in == nullptr)
        {
            juce::MessageManager::callAsync ([onDone] { onDone ({}, "Download failed"); });
            return;
        }
        juce::String ext = ".nam";
        if (model.format.equalsIgnoreCase ("ir") || model.modelUrl.containsIgnoreCase (".wav"))
            ext = ".wav";
        auto dest = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("quadnoncortex-dl")
                        .getChildFile (model.name + ext);
        dest.getParentDirectory().createDirectory();
        dest.deleteFile();
        std::unique_ptr<juce::FileOutputStream> out (dest.createOutputStream());
        if (out == nullptr)
        {
            juce::MessageManager::callAsync ([onDone] { onDone ({}, "Cannot write file"); });
            return;
        }
        out->writeFromInputStream (*in, -1);
        out.reset();
        juce::MessageManager::callAsync ([onDone, dest] { onDone (dest, {}); });
    });
}
