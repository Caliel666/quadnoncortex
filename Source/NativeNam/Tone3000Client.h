#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <thread>

class Tone3000Client
{
public:
    static constexpr const char* kApiBase = "https://www.tone3000.com/api/v1";
    static constexpr const char* kDefaultRedirect = "http://localhost:17301/callback";
    static constexpr int kLoopbackPort = 17301;

    struct Tokens
    {
        juce::String accessToken, refreshToken;
        juce::int64 expiresAtMs = 0;
        bool valid() const { return accessToken.isNotEmpty(); }
    };

    struct ToneInfo
    {
        int id = 0;
        juce::String name;       // API field: title
        juce::String gears;      // API field: gear (amp, pedal, cab, amp-cab, ...)
        juce::String description, userName, format;
        int downloads = 0;
    };

    struct SearchResult
    {
        juce::Array<ToneInfo> tones;
        int page = 1, pageSize = 10, total = 0, totalPages = 1;
        juce::String error;
    };

    struct ModelInfo
    {
        int id = 0;
        juce::String name, modelUrl, format;
        int architecture = 2;
    };

    static Tone3000Client& get()
    {
        static Tone3000Client c;
        return c;
    }

    void setPublishableKey (const juce::String& key) { publishableKey = key; saveTokensToSettings(); }
    juce::String getPublishableKey() const { return publishableKey; }
    bool isLoggedIn() const { return tokens.valid(); }
    juce::String getUserDisplayName() const { return userDisplayName; }

    /** Opens system browser + local loopback on 127.0.0.1:17301 to catch OAuth code automatically. */
    void beginLogin (std::function<void(bool, juce::String)> onDone);
    void cancelLogin();
    void logout();

    /** Manual fallback if loopback fails — paste the code= value from the browser URL. */
    void completeLoginWithCode (const juce::String& code, std::function<void(bool, juce::String)> onDone);

    /** sort: trending | newest | downloads-all-time | best-match
        page is 1-based, pageSize max 10 per API. */
    void searchTones (const juce::String& query, const juce::String& gears, const juce::String& sort,
                      int page, int pageSize, int architecture,
                      std::function<void(SearchResult)> onDone);
    void listCreatedTones (int page, int pageSize, std::function<void(SearchResult)> onDone);
    void listFavoritedTones (int page, int pageSize, std::function<void(SearchResult)> onDone);
    void listModels (int toneId, int architecture,
                     std::function<void(juce::Array<ModelInfo>, juce::String)> onDone);
    void downloadModel (const ModelInfo& model,
                        std::function<void(juce::File, juce::String)> onDone);

    void loadTokensFromSettings();
    void saveTokensToSettings() const;

private:
    Tone3000Client() { loadTokensFromSettings(); }
    ~Tone3000Client() { cancelLogin(); }

    juce::String publishableKey;
    Tokens tokens;
    juce::String userDisplayName;
    juce::String pendingCodeVerifier, pendingState;
    std::function<void(bool, juce::String)> loginCallback;

    std::atomic<bool> loopbackRunning { false };
    std::unique_ptr<std::thread> loopbackThread;

    bool ensureAccessToken (juce::String& error);
    bool refreshAccessToken (juce::String& error);
    bool exchangeCodeForTokens (const juce::String& code, juce::String& error);
    void fetchUserProfile();
    void startLoopbackServer();
    void stopLoopbackServer();

    juce::String httpRequest (const juce::String& method, const juce::String& url,
                              const juce::String& body, const juce::String& contentType,
                              bool auth, juce::String& error);
    static juce::String makeCodeVerifier();
    static juce::String makeCodeChallenge (const juce::String& verifier);
    static juce::String base64UrlEncode (const void* data, size_t n);
};
