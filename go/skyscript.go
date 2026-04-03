// Package skyscript provides Go bindings for the SkyScript CEF browser library
// for X-Plane 12 plugins.
//
// Usage:
//
//	go get github.com/x-z7a/skyscript@v0.2.1
//
// Import:
//
//	import skyscript "github.com/x-z7a/skyscript/go"
//
// Linking requires the pre-built SkyScript static library and its dependencies.
// Set CGO_LDFLAGS to point to the library directory from the distribution zip:
//
//	export CGO_LDFLAGS="-L/path/to/lib/mac_x64 -lSkyScriptLib"
package skyscript

/*
#cgo CFLAGS: -I${SRCDIR}/../src
#include "skyscript_c.h"
#include <stdlib.h>
*/
import "C"
import "unsafe"

// Initialize sets up the SkyScript library and registers the flight loop.
// Call from XPluginStart.
func Initialize() {
	C.skyscript_initialize()
}

// Shutdown tears down the SkyScript library and unregisters the flight loop.
// Call from XPluginStop.
func Shutdown() {
	C.skyscript_shutdown()
}

// LoadAppsFromDirectory scans the apps/ directory for manifest files and
// creates browser windows for each. Returns true if apps were discovered.
func LoadAppsFromDirectory() bool {
	return C.skyscript_load_apps_from_directory() != 0
}

// ReloadApps tears down all apps and rescans the apps/ directory.
func ReloadApps() {
	C.skyscript_reload_apps()
}

// App is an opaque handle to a SkyScript browser window.
type App struct {
	handle C.SkyScriptApp
}

// AppConfig configures a new browser window.
type AppConfig struct {
	Homepage       string
	AudioMuted     bool
	MinimumWidth   uint16
	ScrollSpeed    uint8
	ForcedLanguage string
	UserAgent      string
	HideAddressBar bool
	Framerate      uint8
	Width          uint16
	Height         uint16
	ConsoleLogging bool
}

// DefaultConfig returns the default app configuration.
func DefaultConfig() AppConfig {
	c := C.skyscript_default_config()
	return AppConfig{
		Homepage:       C.GoString(c.homepage),
		AudioMuted:     c.audio_muted != 0,
		MinimumWidth:   uint16(c.minimum_width),
		ScrollSpeed:    uint8(c.scroll_speed),
		ForcedLanguage: C.GoString(c.forced_language),
		UserAgent:      C.GoString(c.user_agent),
		HideAddressBar: c.hide_addressbar != 0,
		Framerate:      uint8(c.framerate),
		Width:          uint16(c.width),
		Height:         uint16(c.height),
		ConsoleLogging: c.console_logging != 0,
	}
}

func boolToInt(b bool) C.int {
	if b {
		return 1
	}
	return 0
}

// CreateAppWindow creates a new browser window with the given name, id, and
// optional configuration. Pass nil for config to use defaults.
func CreateAppWindow(name, id string, config *AppConfig) *App {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	cID := C.CString(id)
	defer C.free(unsafe.Pointer(cID))

	var handle C.SkyScriptApp
	if config != nil {
		cHomepage := C.CString(config.Homepage)
		defer C.free(unsafe.Pointer(cHomepage))
		cLanguage := C.CString(config.ForcedLanguage)
		defer C.free(unsafe.Pointer(cLanguage))
		cUserAgent := C.CString(config.UserAgent)
		defer C.free(unsafe.Pointer(cUserAgent))

		cc := C.SkyScriptAppConfig{
			homepage:        cHomepage,
			audio_muted:     boolToInt(config.AudioMuted),
			minimum_width:   C.ushort(config.MinimumWidth),
			scroll_speed:    C.uchar(config.ScrollSpeed),
			forced_language: cLanguage,
			user_agent:      cUserAgent,
			hide_addressbar: boolToInt(config.HideAddressBar),
			framerate:       C.uchar(config.Framerate),
			width:           C.ushort(config.Width),
			height:          C.ushort(config.Height),
			console_logging: boolToInt(config.ConsoleLogging),
		}
		handle = C.skyscript_create_app_window(cName, cID, &cc)
	} else {
		handle = C.skyscript_create_app_window(cName, cID, nil)
	}

	if handle == nil {
		return nil
	}
	return &App{handle: handle}
}

// DestroyAppWindow destroys a browser window and frees its resources.
func DestroyAppWindow(app *App) {
	if app != nil {
		C.skyscript_destroy_app_window(app.handle)
	}
}

// DestroyAllAppWindows destroys all managed windows.
func DestroyAllAppWindows() {
	C.skyscript_destroy_all_app_windows()
}

// AppWindowCount returns the number of currently managed app windows.
func AppWindowCount() int {
	return int(C.skyscript_get_app_window_count())
}

// ActiveApp returns the active (most recently shown) app, or nil.
func ActiveApp() *App {
	h := C.skyscript_get_active_app()
	if h == nil {
		return nil
	}
	return &App{handle: h}
}

// SetActiveApp sets the active app window.
func SetActiveApp(app *App) {
	if app != nil {
		C.skyscript_set_active_app(app.handle)
	}
}

// FindApp looks up an app by its unique id, or returns nil.
func FindApp(id string) *App {
	cID := C.CString(id)
	defer C.free(unsafe.Pointer(cID))
	h := C.skyscript_find_app(cID)
	if h == nil {
		return nil
	}
	return &App{handle: h}
}

// Name returns the app's display name.
func (a *App) Name() string {
	return C.GoString(C.skyscript_app_get_name(a.handle))
}

// ID returns the app's unique identifier.
func (a *App) ID() string {
	return C.GoString(C.skyscript_app_get_id(a.handle))
}

// Visible reports whether the app window is currently shown.
func (a *App) Visible() bool {
	return C.skyscript_app_is_visible(a.handle) != 0
}

// Show displays the browser window, optionally navigating to a URL.
// Pass an empty string to keep the current page.
func (a *App) Show(url string) {
	cURL := C.CString(url)
	defer C.free(unsafe.Pointer(cURL))
	C.skyscript_app_show(a.handle, cURL)
}

// Hide hides the browser window.
func (a *App) Hide() {
	C.skyscript_app_hide(a.handle)
}
