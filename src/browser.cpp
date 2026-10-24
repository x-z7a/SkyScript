#include "browser.h"

#include "app.h"
#include "config.h"
#include "dataref.h"
#include "json.hpp"
#include "xplm_bridge.h"

#include <cctype>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <XPLMDisplay.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

namespace {

std::string percentEncodePath(const std::string& path) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;
    for (unsigned char c : path) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || c == ':') {
            encoded << static_cast<char>(c);
            continue;
        }
        encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return encoded.str();
}

// A JSON string literal is a valid JavaScript one, and dump() takes care of
// every escape -- quotes, backslashes, control characters, invalid UTF-8.
std::string jsStringLiteral(const std::string& value) {
    return nlohmann::json(value).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

// Spoofs the parts of navigator that embedded sites check before they will
// work offline-of-a-real-browser: a desktop user agent, onLine, permissions
// and geolocation. Geolocation is fed by updateGPSLocation with the aircraft's
// position, so moving-map sites follow the airplane.
std::string navigatorOverrideScript(const std::string& userAgent) {
    return
        "(function(){"
        "function setUserAgent(window, userAgent) {"
        "    try {"
        "        var userAgentProp = Object.getOwnPropertyDescriptor(navigator, 'userAgent');"
        "        if (userAgentProp && userAgentProp.configurable) {"
        "            Object.defineProperty(navigator, 'userAgent', {"
        "                get: function () { return userAgent; },"
        "                configurable: true"
        "            });"
        "        } else if (navigator.__defineGetter__) {"
        "            navigator.__defineGetter__('userAgent', function () {"
        "                return userAgent;"
        "            });"
        "        }"
        "    } catch (e) {}"
        "}"
        "window.skyscript_watchers = (window.skyscript_watchers || {});"
        "Object.defineProperty(navigator, 'onLine', {"
        "    get: function() { return true; },"
        "    configurable: true"
        "});"
        "navigator.permissions.query = (options) => {"
        "    return Promise.resolve({ state: 'granted' });"
        "};"
        "navigator.geolocation.watchPosition = (success, error, options) => {"
        "    window.skyscript_watchers = (window.skyscript_watchers || {});"
        "    const id = Math.round(Date.now() / 1000);"
        "    window.skyscript_watchers[id] = success;"
        "    if (window.skyscript_location) { success(window.skyscript_location); }"
        "    return id;"
        "};"
        "navigator.geolocation.clearWatch = (id) => {"
        "    if (!window.skyscript_watchers) { return; }"
        "    delete window.skyscript_watchers[id];"
        "};"
        "navigator.geolocation.getCurrentPosition = (success, error, options) => {"
        "    if (window.skyscript_location) {"
        "        success(window.skyscript_location);"
        "    } else {"
        "        navigator.geolocation.watchPosition(success, error, options);"
        "    }"
        "};"
        "setUserAgent(window, " + jsStringLiteral(userAgent) + ");"
        "window.dispatchEvent(new Event('load'));"
        "})();";
}

// The address bar for apps that browse the open web. It is part of the page
// rather than chrome we draw, because a browser window draws itself.
const char* addressBarScript() {
    return R"(
        (function() {
            if (!document.body || document.getElementById('cefToolbar')) {
                return;
            }

            const isDarkMode = window.matchMedia('(prefers-color-scheme: dark)').matches;
            const chevron = 'data:image/svg+xml;charset=UTF-8,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 9.971 13.623"><path fill-opacity=".85" fill="#007AFF" d="M0 6.807c0 .214.088.41.244.576l6.035 5.996a.773.773 0 0 0 .576.234.78.78 0 0 0 .801-.8.83.83 0 0 0-.234-.577l-5.45-5.43 5.45-5.43a.847.847 0 0 0 .234-.575.78.78 0 0 0-.8-.801.773.773 0 0 0-.577.234L.244 6.23A.831.831 0 0 0 0 6.807Z"/></svg>');
            const refresh = 'data:image/svg+xml;charset=UTF-8,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 13.994 18.271"><path fill-opacity=".85" fill="#999999" d="M0 9.717a6.816 6.816 0 1 0 13.633 0c0-.44-.303-.742-.733-.742-.41 0-.673.302-.673.732 0 2.988-2.422 5.4-5.41 5.4a5.401 5.401 0 0 1-5.41-5.4 5.41 5.41 0 0 1 5.41-5.41c.507 0 .966.02 1.376.107l-2.07 2.032a.663.663 0 0 0-.195.488c0 .4.302.703.683.703.225 0 .381-.078.508-.195l3.086-3.086a.685.685 0 0 0 .205-.508.719.719 0 0 0-.205-.498L7.12.215A.647.647 0 0 0 6.611 0c-.38 0-.683.322-.683.723 0 .185.068.361.185.498L7.91 2.998A5.769 5.769 0 0 0 6.816 2.9 6.81 6.81 0 0 0 0 9.717Z"/></svg>');

            // Create container
            const toolbar = document.createElement('div');
            toolbar.id = 'cefToolbar';
            toolbar.style.position = 'fixed';
            toolbar.style.top = '0';
            toolbar.style.left = '0';
            toolbar.style.width = '100%';
            toolbar.style.zIndex = '9999';
            toolbar.style.padding = '4px';
            toolbar.style.backgroundColor = isDarkMode ? '#1A1A1A' : '#EEE';
            toolbar.style.boxSizing = 'border-box';
            toolbar.style.display = 'flex';
            toolbar.style.alignItems = 'center';
            toolbar.style.gap = '8px';

            // Back button
            const backBtn = document.createElement('button');
            backBtn.id = "cefBack";
            backBtn.style.background = `url(${chevron}) no-repeat center / contain`;
            backBtn.style.cursor = 'pointer';
            backBtn.style.width = '14px';
            backBtn.style.height = '14px';
            backBtn.style.outline = 'none';
            backBtn.style.border = 'none';
            backBtn.onclick = function() {
                window.history.back();
            };

            // Forward button
            const fwdBtn = document.createElement('button');
            fwdBtn.id = "cefForward";
            fwdBtn.style.cssText = backBtn.style.cssText;
            fwdBtn.style.transform = 'scaleX(-1)';
            fwdBtn.onclick = function() {
                window.history.forward();
            };

            // Address bar
            const addressBar = document.createElement('input');
            addressBar.type = 'text';
            addressBar.id = 'cefAddressBar';
            addressBar.value = window.location.href;
            addressBar.style.flex = '1';
            addressBar.style.fontSize = '12px';
            addressBar.style.border = 'none';
            addressBar.style.outline = 'none';
            addressBar.style.height = '20px';
            addressBar.style.color = isDarkMode ? '#D2D2D2' : '#1A1A1A';
            addressBar.style.backgroundColor = isDarkMode ? '#000000' : '#D2D2D2';
            addressBar.style.padding = '2px 8px';
            addressBar.style.borderRadius = '12px';

            const observer = new MutationObserver(() => {
                if (document.activeElement !== addressBar) {
                    addressBar.value = window.location.href;
                }
            });
            observer.observe(document, { subtree: true, childList: true });

            window.addEventListener("popstate", () => {
                addressBar.value = window.location.href;
            });

            addressBar.addEventListener('keydown', function(e) {
                if (e.key === 'Enter') {
                    if (!addressBar.value.startsWith('http')) {
                        addressBar.value = "https://" + addressBar.value;
                    }

                    window.location.href = addressBar.value;
                }
            });

            addressBar.addEventListener('blur', function(e) {
                if (addressBar.value === '') {
                    addressBar.value = window.location.href;
                }
            });

            // Clear input button
            const clearInputButton = document.createElement('button');
            clearInputButton.textContent = String.fromCharCode(10005);
            clearInputButton.style.cursor = 'pointer';
            clearInputButton.style.color = isDarkMode ? '#D2D2D2' : '#1A1A1A';
            clearInputButton.style.backgroundColor = 'transparent';
            clearInputButton.style.fontSize = '12px';
            clearInputButton.style.width = '14px';
            clearInputButton.style.height = '14px';
            clearInputButton.style.outline = 'none';
            clearInputButton.style.border = 'none';
            clearInputButton.style.position = 'absolute';
            clearInputButton.style.right = '32px';
            clearInputButton.style.zIndex = '9';
            clearInputButton.onclick = function() {
                addressBar.value = '';
                addressBar.focus();
            };

            // Refresh button
            const refreshButton = document.createElement('button');
            refreshButton.style.background = `url(${refresh}) no-repeat center / contain`;
            refreshButton.style.cursor = 'pointer';
            refreshButton.style.width = '14px';
            refreshButton.style.height = '14px';
            refreshButton.style.outline = 'none';
            refreshButton.style.border = 'none';
            refreshButton.onclick = function() {
                window.location.reload();
            };

            // Add elements to the toolbar
            toolbar.appendChild(backBtn);
            toolbar.appendChild(fwdBtn);
            toolbar.appendChild(addressBar);
            toolbar.appendChild(clearInputButton);
            toolbar.appendChild(refreshButton);

            // Insert toolbar at the very top of <body>
            document.body.insertBefore(toolbar, document.body.firstChild);

            // Add some spacing so the page content isn't hidden behind the toolbar
            document.body.style.marginTop = '40px';
        })();
    )";
}

} // namespace

Browser::Browser(App *anApp) {
    app = anApp;
    window = nullptr;
    lastGpsUpdateTime = 0.0f;
    xplmBridge = new XplmBridge();
    currentUrl = "";
}

void Browser::initialize() {
    currentUrl = app->config.homepage;

    std::string datarefPrefix = "skyscript/" + app->id;
    Dataref::getInstance()->createDataref<std::string>((datarefPrefix + "/url").c_str(), &currentUrl, true, [this](std::string newUrl) {
        if (!newUrl.starts_with("http") && !newUrl.starts_with("chrome://") && !newUrl.starts_with("data:") && !newUrl.starts_with("file://")) {
            return false;
        }

        loadUrl(newUrl);
        return true;
    });

    Dataref::getInstance()->createCommand((datarefPrefix + "/refresh").c_str(), "Refresh the current web page", [this](XPLMCommandPhase inPhase) {
        if (inPhase != xplm_CommandBegin) {
            return;
        }

        refresh(false);
    });
}

void Browser::attach(XPLMWindowID aWindow) {
    window = aWindow;
    if (!window) {
        return;
    }

    XPLMWindowAddBrowserFunction(window, XplmBridge::functionName, handleBrowserCall, this);
    if (xplmBridge) {
        xplmBridge->setWindow(window);
    }

    if (!currentUrl.empty()) {
        XPLMWindowSetURL(window, currentUrl.c_str());
    }
}

void Browser::destroy() {
    if (xplmBridge) {
        xplmBridge->setWindow(nullptr);
        delete xplmBridge;
        xplmBridge = nullptr;
    }

    // App destroys the window itself; this only forgets it.
    window = nullptr;
}

void Browser::visibilityWillChange(bool becomesVisible) {
    lastGpsUpdateTime = becomesVisible ? XPLMGetElapsedTime() : 0.0f;
}

void Browser::update() {
    if (xplmBridge) {
        xplmBridge->processPendingPosts();
    }

    if (lastGpsUpdateTime > std::numeric_limits<float>::epsilon() && XPLMGetElapsedTime() > lastGpsUpdateTime + 1.0f) {
        updateGPSLocation();
    }
}

void Browser::loadUrl(const std::string& url) {
    currentUrl = url;
    if (window && !currentUrl.empty()) {
        XPLMWindowSetURL(window, currentUrl.c_str());
    }
}

void Browser::refresh(bool ignoreCache) {
    if (window) {
        XPLMWindowRefresh(window, ignoreCache ? 1 : 0);
    }
}

void Browser::onLoadFinished(const std::string& url) {
    if (!url.empty()) {
        currentUrl = url;
    }

    injectPageScripts();
}

void Browser::onLoadError(const std::string& url, const std::string& error) {
    debug("[%s] Error loading %s: %s\n", app->name.c_str(), url.c_str(), error.empty() ? "unknown error" : error.c_str());
}

void Browser::injectScript(const std::string& script) {
    if (window) {
        XPLMWindowInjectScript(window, script.c_str());
    }
}

// Everything the CEF integration used to inject at load end, in the same order:
// navigator overrides first so the bridge and the page see them, then the
// address bar, then the window.skyscript API. Each script guards itself against
// running twice, because a single-page app can finish loading more than once.
void Browser::injectPageScripts() {
    injectScript(navigatorOverrideScript(app->config.user_agent));

    if (!app->config.hide_addressbar) {
        injectScript(addressBarScript());
    }

    injectScript(XplmBridge::getInjectionScript(app->config.console_logging));
}

const char* Browser::handleBrowserCall(XPLMWindowID inWindowID, const char* inJSON, void* inRefcon) {
    (void)inWindowID;
    Browser *browser = static_cast<Browser*>(inRefcon);
    if (!browser || !browser->xplmBridge) {
        return nullptr;
    }

    // The notification handlers act on App::current, as every other entry
    // point from the sim does.
    App *previous = App::current;
    App::current = browser->app;
    const char *response = browser->xplmBridge->handleRequest(inJSON ? inJSON : "null");
    App::current = previous;
    return response;
}

void Browser::updateGPSLocation() {
    if (!window) {
        return;
    }

    float latitude = Dataref::getInstance()->get<float>("sim/flightmodel/position/latitude");
    float longitude = Dataref::getInstance()->get<float>("sim/flightmodel/position/longitude");
    float speedMetersSecond = Dataref::getInstance()->get<float>("sim/flightmodel/position/groundspeed");
    float altitudeMetersAboveSeaLevel = Dataref::getInstance()->get<float>("sim/flightmodel/position/elevation");
    float magneticHeading = Dataref::getInstance()->get<float>("sim/flightmodel/position/mag_psi");

    float windDirection = Dataref::getInstance()->get<float>("sim/weather/wind_direction_degt");
    float windSpeed = Dataref::getInstance()->get<float>("sim/weather/wind_speed_kt");

    float altitudeMetersAboveGroundLevel = Dataref::getInstance()->get<float>("sim/flightmodel/position/y_agl");
    float airspeedKts = Dataref::getInstance()->get<float>("sim/flightmodel/position/indicated_airspeed");

    std::stringstream stream;
    stream << "window.skyscript_location = { ";
    stream << "coords: { ";
    stream << "latitude: " << std::fixed << std::setprecision(6) << latitude << ", ";
    stream << "longitude: " << std::fixed << std::setprecision(6) << longitude << ", ";
    stream << "accuracy: 10, ";
    stream << "altitude: " << std::fixed << std::setprecision(0) << altitudeMetersAboveSeaLevel << ", ";
    stream << "altitudeAccuracy: 10, ";
    stream << "heading: " << std::fixed << std::setprecision(0) << magneticHeading << ", ";
    stream << "speed: " << std::fixed << std::setprecision(0) << speedMetersSecond << ", ";
    stream << "}, ";
    stream << "wind: { ";
    stream << "direction: " << std::fixed << std::setprecision(0) << windDirection << ", ";
    stream << "speedKts: " << std::fixed << std::setprecision(0) << windSpeed << ", ";
    stream << "}, ";
    stream << "extra: { ";
    stream << "altitudeAgl: " << std::fixed << std::setprecision(0) << altitudeMetersAboveGroundLevel << ", ";
    stream << "airspeedKts: " << std::fixed << std::setprecision(0) << airspeedKts << ", ";
    stream << "}, timestamp: Date.now() }; for (let key in window.skyscript_watchers) { window.skyscript_watchers[key](window.skyscript_location); }";

    injectScript(stream.str());
    lastGpsUpdateTime = XPLMGetElapsedTime();
}

void Browser::onMessage(const std::string& channel, MessageHandler handler) {
    if (xplmBridge) {
        xplmBridge->onMessage(channel, handler);
    }
}

void Browser::postMessage(const std::string& channel, const std::string& payload) {
    if (xplmBridge) {
        xplmBridge->postMessage(channel, payload);
    }
}

std::string Browser::fileUrlForPath(const std::string& path) {
    std::error_code ec;
    std::filesystem::path absolute = std::filesystem::absolute(path, ec);
    std::string generic = (ec ? std::filesystem::path(path) : absolute).generic_string();
    if (generic.empty() || generic.front() != '/') {
        generic.insert(generic.begin(), '/');
    }

    return "file://" + percentEncodePath(generic);
}
