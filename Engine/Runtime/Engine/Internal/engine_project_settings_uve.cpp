// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/core/engine_project_settings_uve.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <vector>

#include "uve/commandline/i_command_line_uve.h"
#include "uve/logging/logging_macros_uve.h"

namespace UVE::Core {
namespace {

using Config::SettingDescriptorUVE;
using Config::SettingValueUVE;

[[nodiscard]] std::string_view GetPlatformSettingsTargetUVE() noexcept {
#if defined(__ANDROID__)
    return "android";
#elif defined(__EMSCRIPTEN__)
    return "web";
#elif defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "apple";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

/// One engine project setting and the EngineConfigUVE field it overrides.
struct EngineProjectSettingUVE final {
    SettingDescriptorUVE descriptor;
    void (*apply)(EngineConfigUVE& config, const SettingValueUVE& value) = nullptr;
};

[[nodiscard]] SettingDescriptorUVE RestartRequiredUVE(SettingDescriptorUVE descriptor) {
    descriptor.flags |= Config::kSettingFlagRestartRequiredUVE;
    return descriptor;
}

[[nodiscard]] SettingDescriptorUVE PlatformOverrideUVE(SettingDescriptorUVE descriptor) {
    descriptor.flags |= Config::kSettingFlagPerPlatformUVE;
    return descriptor;
}

[[nodiscard]] SettingDescriptorUVE CommandLineOnlyUVE(SettingDescriptorUVE descriptor) {
    descriptor.flags |= Config::kSettingFlagHiddenUVE | Config::kSettingFlagNotPersistedUVE;
    return descriptor;
}

[[nodiscard]] const std::vector<EngineProjectSettingUVE>& GetEngineProjectSettingsUVE() {
    namespace Id = EngineProjectSettingIdUVE;
    const EngineConfigUVE defaults{};
    static const std::vector<EngineProjectSettingUVE> settings = {
        {RestartRequiredUVE(Config::MakeFloatSettingUVE(
             std::string(Id::kPhysicsTicksPerSecondUVE), defaults.fixedUpdateFps, 1.0, 1000.0, "Ticks Per Second",
             "Physics/Common", "How many fixed simulation steps run each second.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.fixedUpdateFps = std::get<double>(value); }},
        {RestartRequiredUVE(Config::MakeFloatSettingUVE(
             std::string(Id::kPhysicsMaxFrameTimeUVE), defaults.maxDeltaTimeSeconds, 0.01, 1.0, "Max Frame Time",
             "Physics/Common",
             "The longest frame, in seconds, the simulation catches up on. Anything longer - a stall, a "
             "breakpoint - is cut to this, so the simulation slows down instead of spiralling.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.maxDeltaTimeSeconds = std::get<double>(value);
         }},
        {RestartRequiredUVE(Config::MakeIntSettingUVE(
             std::string(Id::kPhysicsMaxStepsPerFrameUVE), defaults.maxFixedStepsPerFrame, 1, 64,
             "Max Steps Per Frame",
             "Physics/Common",
             "The most fixed steps one frame runs to catch up. Higher keeps the simulation on time "
             "through slow frames; lower keeps a slow frame from getting slower still.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.maxFixedStepsPerFrame = static_cast<int>(std::get<std::int64_t>(value));
         }},
        {RestartRequiredUVE(Config::MakeVector3SettingUVE(
             std::string(Id::kPhysicsGravityUVE),
             Config::SettingVector3UVE{defaults.gravity.x, defaults.gravity.y, defaults.gravity.z}, -1000.0, 1000.0,
             "Gravity", "Physics/3D",
             "Acceleration every rigid body, character and particle falls with, in metres per second squared. "
             "Each body scales it by its own gravity scale.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             const auto& gravity = std::get<Config::SettingVector3UVE>(value);
             config.gravity = Math::Vector3UVE{static_cast<float>(gravity.x), static_cast<float>(gravity.y),
                                               static_cast<float>(gravity.z)};
         }},
        {RestartRequiredUVE(PlatformOverrideUVE(Config::MakeEnumSettingUVE(
             std::string(Id::kShadowMapResolutionUVE), static_cast<std::int64_t>(defaults.shadowMapResolution),
             {{512, "512"}, {1024, "1024"}, {2048, "2048"}, {4096, "4096"}}, "Map Resolution", "Rendering/Shadows",
             "Width and height, in texels, of the directional light's shadow map."))),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.shadowMapResolution = static_cast<std::uint32_t>(std::get<std::int64_t>(value));
         }},
        {RestartRequiredUVE(PlatformOverrideUVE(Config::MakeEnumSettingUVE(
             std::string(Id::kShadowFilterUVE), static_cast<std::int64_t>(defaults.shadowPcfKernelRadius),
             {{0, "Hard"}, {1, "Soft (3x3)"}, {2, "Softer (5x5)"}}, "Filter", "Rendering/Shadows",
             "How shadow edges are softened. Softer costs more samples per pixel."))),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.shadowPcfKernelRadius = static_cast<std::uint32_t>(std::get<std::int64_t>(value));
         }},
        {RestartRequiredUVE(Config::MakeFloatSettingUVE(
             std::string(Id::kAutoSaveIntervalUVE), defaults.autoSaveIntervalSecondsUVE, 10.0, 3600.0,
             "Auto-Save Interval", "Application/Save", "Seconds between writes to the game's auto-save slot.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.autoSaveIntervalSecondsUVE = std::get<double>(value);
         }},
        {Config::MakeBoolSettingUVE(std::string(Id::kQuitOnLastWindowClosedUVE),
                                    defaults.quitOnLastWindowClosedUVE, "Quit on Last Window Closed",
                                    "Application/Runtime",
                                    "Request application shutdown when the last native window is closed."),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.quitOnLastWindowClosedUVE = std::get<bool>(value);
         }},
        {Config::MakeColorSettingUVE(std::string(Id::kBootBackgroundColorUVE),
                                     Config::SettingColorUVE{defaults.backgroundColorUVE[0],
                                                             defaults.backgroundColorUVE[1],
                                                             defaults.backgroundColorUVE[2],
                                                             defaults.backgroundColorUVE[3]},
                                     true, "Background Color", "Application/Boot",
                                     "RGBA clear color shown behind an empty scene and during the boot screen."),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             const Config::SettingColorUVE color = std::get<Config::SettingColorUVE>(value);
             config.backgroundColorUVE = {color.r, color.g, color.b, color.a};
         }},
        {Config::MakeFilePathSettingUVE(std::string(Id::kBootSplashImageUVE), "", 256U,
                                        "Boot Screen Image", "Application/Boot",
                                        "Optional Content-relative image rendered over the background during standalone startup."),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.bootSplashImagePathUVE = std::get<std::string>(value);
         }},
        {Config::MakeFloatSettingUVE(std::string(Id::kBootSplashFadeSecondsUVE),
                                     defaults.splashFadeSecondsUVE, 0.0, 60.0, "Fade Time", "Application/Boot",
                                     "Boot-screen fade duration in seconds."),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.splashFadeSecondsUVE = std::get<double>(value);
         }},
        {Config::MakeFloatSettingUVE(std::string(Id::kBootSplashMinimumDisplaySecondsUVE),
                                     defaults.splashMinimumDisplaySecondsUVE, 0.0, 600.0, "Minimum Display Time",
                                     "Application/Boot", "Minimum boot-screen display time in seconds."),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.splashMinimumDisplaySecondsUVE = std::get<double>(value);
         }},
        {Config::MakeBoolSettingUVE(std::string(Id::kBootSplashSkippableUVE), defaults.splashSkippableUVE,
                                    "Skippable", "Application/Boot",
                                    "Allow users to skip the boot screen after its minimum display time."),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.splashSkippableUVE = std::get<bool>(value);
         }},
        {Config::MakeBoolSettingUVE(std::string(Id::kSkipBootSplashInEditorPlayModeUVE),
                                    defaults.skipSplashInEditorPlayModeUVE, "Skip in Editor Play Mode",
                                    "Application/Boot",
                                    "Do not show the boot screen when starting a game from the Editor."),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.skipSplashInEditorPlayModeUVE = std::get<bool>(value);
         }},
        {Config::MakeBoolSettingUVE(std::string(Id::kCrashHandlerEnabledUVE), defaults.crashHandlerEnabledUVE,
                                    "Crash Handler", "Application/Crash Reporting",
                                    "Install native unhandled-exception and fatal-signal reporting for standalone runs."),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.crashHandlerEnabledUVE = std::get<bool>(value);
         }},
        {Config::MakeFilePathSettingUVE(std::string(Id::kCrashDumpDirectoryUVE), "crash-dumps", 256U,
                                        "Crash Dump Directory", "Application/Crash Reporting",
                                        "Directory under user data for crash reports and platform-native minidumps."),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.crashDumpDirectoryUVE = std::get<std::string>(value);
         }},
        {Config::MakeStringSettingUVE(std::string(Id::kSymbolUploadEndpointUVE), "", 2048U,
                                      "Symbol Upload Endpoint", "Application/Crash Reporting",
                                      "Optional HTTPS endpoint reserved for host integrations; the runtime never uploads automatically."),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.symbolUploadEndpointUVE = std::get<std::string>(value);
         }},
        {RestartRequiredUVE(PlatformOverrideUVE(Config::MakeIntSettingUVE(
             std::string(Id::kWindowWidthUVE), defaults.windowWidth, 1, 16384, "Width", "Window/Display",
             "Initial window width in framebuffer-independent screen units."))),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.windowWidth = static_cast<std::uint32_t>(std::get<std::int64_t>(value));
         }},
        {RestartRequiredUVE(PlatformOverrideUVE(Config::MakeIntSettingUVE(
             std::string(Id::kWindowHeightUVE), defaults.windowHeight, 1, 16384, "Height", "Window/Display",
             "Initial window height in framebuffer-independent screen units."))),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.windowHeight = static_cast<std::uint32_t>(std::get<std::int64_t>(value));
         }},
        {RestartRequiredUVE(PlatformOverrideUVE(Config::MakeEnumSettingUVE(
             std::string(Id::kWindowModeUVE), static_cast<std::int64_t>(defaults.windowModeUVE),
             {{0, "Windowed"}, {1, "Maximized"}, {2, "Fullscreen"},
              {3, "Exclusive Fullscreen"}, {4, "Borderless"}}, "Mode", "Window/Display",
             "Windowed and maximized are decorated unless Borderless is enabled. Fullscreen uses a "
             "borderless desktop-sized window; Exclusive Fullscreen attaches to the monitor mode."))),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.windowModeUVE = static_cast<Platform::WindowModeUVE>(std::get<std::int64_t>(value));
         }},
        {RestartRequiredUVE(Config::MakeBoolSettingUVE(
             std::string(Id::kWindowResizableUVE), defaults.windowResizableUVE, "Resizable", "Window/Display")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.windowResizableUVE = std::get<bool>(value);
         }},
        {RestartRequiredUVE(Config::MakeBoolSettingUVE(
             std::string(Id::kWindowBorderlessUVE), defaults.windowBorderlessUVE, "Borderless", "Window/Display")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.windowBorderlessUVE = std::get<bool>(value); }},
        {RestartRequiredUVE(Config::MakeBoolSettingUVE(
             std::string(Id::kWindowAlwaysOnTopUVE), defaults.windowAlwaysOnTopUVE, "Always on Top", "Window/Display")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.windowAlwaysOnTopUVE = std::get<bool>(value); }},
        {RestartRequiredUVE(Config::MakeBoolSettingUVE(
             std::string(Id::kWindowTransparentUVE), defaults.windowTransparentUVE, "Transparent Background",
             "Window/Display", "Requests a transparent native framebuffer; compositor/backend support is required.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.windowTransparentUVE = std::get<bool>(value); }},
        {RestartRequiredUVE(Config::MakeIntSettingUVE(
             std::string(Id::kWindowMinimumWidthUVE), 0, 0, 16384, "Minimum Width", "Window/Size Limits",
             "Zero leaves the width unconstrained.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.windowMinimumWidthUVE = static_cast<std::uint32_t>(std::get<std::int64_t>(value)); }},
        {RestartRequiredUVE(Config::MakeIntSettingUVE(
             std::string(Id::kWindowMinimumHeightUVE), 0, 0, 16384, "Minimum Height", "Window/Size Limits",
             "Zero leaves the height unconstrained.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.windowMinimumHeightUVE = static_cast<std::uint32_t>(std::get<std::int64_t>(value)); }},
        {RestartRequiredUVE(Config::MakeIntSettingUVE(
             std::string(Id::kWindowMaximumWidthUVE), 0, 0, 16384, "Maximum Width", "Window/Size Limits",
             "Zero leaves the width unconstrained.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.windowMaximumWidthUVE = static_cast<std::uint32_t>(std::get<std::int64_t>(value)); }},
        {RestartRequiredUVE(Config::MakeIntSettingUVE(
             std::string(Id::kWindowMaximumHeightUVE), 0, 0, 16384, "Maximum Height", "Window/Size Limits",
             "Zero leaves the height unconstrained.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.windowMaximumHeightUVE = static_cast<std::uint32_t>(std::get<std::int64_t>(value)); }},
        {RestartRequiredUVE(Config::MakeBoolSettingUVE(
             std::string(Id::kWindowPositionSpecifiedUVE), defaults.windowPositionSpecifiedUVE, "Set Initial Position",
             "Window/Placement")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.windowPositionSpecifiedUVE = std::get<bool>(value); }},
        {RestartRequiredUVE(Config::MakeIntSettingUVE(
             std::string(Id::kWindowPositionXUVE), 0, -131072, 131072, "X", "Window/Placement")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.windowPositionXUVE = static_cast<std::int32_t>(std::get<std::int64_t>(value)); }},
        {RestartRequiredUVE(Config::MakeIntSettingUVE(
             std::string(Id::kWindowPositionYUVE), 0, -131072, 131072, "Y", "Window/Placement")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.windowPositionYUVE = static_cast<std::int32_t>(std::get<std::int64_t>(value)); }},
        {RestartRequiredUVE(Config::MakeStringSettingUVE(
             std::string(Id::kWindowMonitorUVE), "", 256U, "Monitor", "Window/Placement",
             "Exact monitor name; empty selects the primary monitor.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.windowMonitorNameUVE = std::get<std::string>(value); }},
        {RestartRequiredUVE(PlatformOverrideUVE(Config::MakeBoolSettingUVE(
             std::string(Id::kWindowHighDpiAwareUVE), defaults.highDpiAwareUVE, "High-DPI Aware", "Window/Scaling"))),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.highDpiAwareUVE = std::get<bool>(value); }},
        {RestartRequiredUVE(PlatformOverrideUVE(Config::MakeBoolSettingUVE(
             std::string(Id::kWindowPerMonitorScalingUVE), defaults.perMonitorScalingUVE, "Per-Monitor Scaling",
             "Window/Scaling"))),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.perMonitorScalingUVE = std::get<bool>(value); }},
        {RestartRequiredUVE(Config::MakeFloatSettingUVE(
             std::string(Id::kWindowContentScaleOverrideUVE), defaults.contentScaleOverrideUVE, 0.0, 4.0,
             "Content Scale Override", "Window/Scaling", "Zero follows the monitor's reported content scale.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.contentScaleOverrideUVE = std::get<double>(value); }},
        {RestartRequiredUVE(Config::MakeEnumSettingUVE(
             std::string(Id::kWindowStretchModeUVE), static_cast<std::int64_t>(defaults.stretchModeUVE),
             {{0, "Disabled"}, {1, "Canvas Items"}, {2, "Viewport"}}, "Stretch Mode", "Window/Scaling")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.stretchModeUVE = static_cast<Platform::StretchModeUVE>(std::get<std::int64_t>(value)); }},
        {RestartRequiredUVE(Config::MakeEnumSettingUVE(
             std::string(Id::kWindowAspectPolicyUVE), static_cast<std::int64_t>(defaults.aspectPolicyUVE),
             {{0, "Ignore"}, {1, "Keep"}, {2, "Keep Width"}, {3, "Keep Height"}, {4, "Expand"}},
             "Aspect Policy", "Window/Scaling")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.aspectPolicyUVE = static_cast<Platform::AspectPolicyUVE>(std::get<std::int64_t>(value)); }},
        {RestartRequiredUVE(Config::MakeBoolSettingUVE(
             std::string(Id::kWindowIntegerOnlyScalingUVE), defaults.integerOnlyScalingUVE, "Integer-Only Scaling",
             "Window/Scaling", "Avoid fractional scale factors for pixel-art viewports.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.integerOnlyScalingUVE = std::get<bool>(value); }},
        {RestartRequiredUVE(PlatformOverrideUVE(Config::MakeEnumSettingUVE(
             std::string(Id::kWindowOrientationUVE), static_cast<std::int64_t>(defaults.orientationUVE),
             {{0, "Auto"}, {1, "Landscape"}, {2, "Portrait"}, {3, "Landscape Left"},
              {4, "Landscape Right"}, {5, "Portrait Upside Down"}}, "Orientation", "Window/Handheld"))),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.orientationUVE = static_cast<Platform::DisplayOrientationUVE>(std::get<std::int64_t>(value)); }},
        {RestartRequiredUVE(PlatformOverrideUVE(Config::MakeStringListSettingUVE(
             std::string(Id::kWindowAllowedOrientationsUVE), {"landscape", "portrait"}, 6U, 24U,
             "Allowed Orientations", "Window/Handheld"))),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             const auto& names = std::get<Config::SettingStringListUVE>(value);
             std::vector<Platform::DisplayOrientationUVE> parsed;
             bool valid = !names.empty();
             for (const std::string& name : names) {
                 const auto orientation = Platform::ParseDisplayOrientationUVE(name);
                 if (!orientation || *orientation == Platform::DisplayOrientationUVE::Auto ||
                     std::find(parsed.begin(), parsed.end(), *orientation) != parsed.end()) {
                     valid = false;
                     break;
                 }
                 parsed.push_back(*orientation);
             }
             if (valid && (config.orientationUVE == Platform::DisplayOrientationUVE::Auto ||
                           std::find(parsed.begin(), parsed.end(), config.orientationUVE) != parsed.end())) {
                 config.allowedOrientationsUVE = std::move(parsed);
             }
         }},
        {RestartRequiredUVE(PlatformOverrideUVE(Config::MakeEnumSettingUVE(
             std::string(Id::kWindowVSyncModeUVE), static_cast<std::int64_t>(defaults.vsyncModeUVE),
             {{0, "Off"}, {1, "On"}, {2, "Adaptive"}, {3, "Mailbox"}}, "V-Sync Mode", "Window/Performance"))),
         [](EngineConfigUVE& config, const SettingValueUVE& value) {
             config.vsyncModeUVE = static_cast<Platform::VSyncModeUVE>(std::get<std::int64_t>(value));
             config.vsyncModeExplicitUVE = true;
             config.vsyncEnabledUVE = config.vsyncModeUVE != Platform::VSyncModeUVE::Off;
         }},
        {RestartRequiredUVE(Config::MakeIntSettingUVE(
             std::string(Id::kWindowFocusedFrameRateCapUVE), 0, 0, 1000, "Focused Frame Rate Cap",
             "Window/Performance", "Frames per second; zero is uncapped.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.focusedFrameRateCapUVE = static_cast<std::uint32_t>(std::get<std::int64_t>(value)); }},
        {RestartRequiredUVE(Config::MakeIntSettingUVE(
             std::string(Id::kWindowUnfocusedFrameRateCapUVE), 0, 0, 1000, "Unfocused Frame Rate Cap",
             "Window/Performance", "Frames per second while unfocused; zero is uncapped.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.unfocusedFrameRateCapUVE = static_cast<std::uint32_t>(std::get<std::int64_t>(value)); }},
        {PlatformOverrideUVE(Config::MakeBoolSettingUVE(
             std::string(Id::kWindowAllowDisplaySleepUVE), defaults.allowDisplaySleepUVE, "Allow Display Sleep",
             "Window/Performance", "When disabled, requests that the platform keep the display awake.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.allowDisplaySleepUVE = std::get<bool>(value); }},
        {RestartRequiredUVE(Config::MakeFilePathSettingUVE(
             std::string(Id::kWindowCursorImageUVE), "", 256U, "Cursor Image", "Window/Cursor",
             "Content-relative PNG cursor image; decoded as RGBA8 by the project runtime.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.cursorImagePathUVE = std::get<std::string>(value); }},
        {RestartRequiredUVE(Config::MakeIntSettingUVE(
             std::string(Id::kWindowCursorHotspotXUVE), 0, 0, 8192, "Hotspot X", "Window/Cursor")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.cursorHotspotXUVE = static_cast<std::uint32_t>(std::get<std::int64_t>(value)); }},
        {RestartRequiredUVE(Config::MakeIntSettingUVE(
             std::string(Id::kWindowCursorHotspotYUVE), 0, 0, 8192, "Hotspot Y", "Window/Cursor")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.cursorHotspotYUVE = static_cast<std::uint32_t>(std::get<std::int64_t>(value)); }},
        {Config::MakeBoolSettingUVE(std::string(Id::kWindowCursorVisibleUVE), defaults.cursorVisibleUVE,
                                    "Visible by Default", "Window/Cursor"),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.cursorVisibleUVE = std::get<bool>(value); }},
        {Config::MakeBoolSettingUVE(std::string(Id::kWindowCursorConfinedUVE), defaults.cursorConfinedToWindowUVE,
                                    "Confined to Window", "Window/Cursor",
                                    "Capture the pointer inside the game window; backend behavior varies by host."),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.cursorConfinedToWindowUVE = std::get<bool>(value); }},
        {RestartRequiredUVE(Config::MakeStringSettingUVE(
             std::string(Id::kWindowTitleFormatUVE), defaults.windowTitleFormatUVE, 256U, "Title Format",
             "Window/Title", "Supported tokens: {productName}, {projectName}, and {sceneName}.")),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.windowTitleFormatUVE = std::get<std::string>(value); }},
        {Config::MakeBoolSettingUVE(std::string(Id::kWindowAppendSceneInEditorPlayUVE),
                                    defaults.appendSceneNameInEditorPlayModeUVE, "Append Scene in Editor Play",
                                    "Window/Title", "Append the active scene name while the editor is playing."),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.appendSceneNameInEditorPlayModeUVE = std::get<bool>(value); }},
        {RestartRequiredUVE(CommandLineOnlyUVE(Config::MakeBoolSettingUVE(
             std::string(Id::kHeadlessUVE), defaults.headlessUVE, "Headless", "Application/Runtime",
             "Run without a visible window. Command-line-only; use --headless."))),
         [](EngineConfigUVE& config, const SettingValueUVE& value) { config.headlessUVE = std::get<bool>(value); }},
    };
    return settings;
}

[[nodiscard]] bool ParseCommandLineNumberUVE(const std::string_view text, std::int64_t& value) noexcept {
    if (text.empty()) {
        return false;
    }
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}

[[nodiscard]] bool ParseCommandLineNumberUVE(const std::string_view text, double& value) noexcept {
    if (text.empty()) {
        return false;
    }
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value,
                                               std::chars_format::general);
    return error == std::errc{} && end == text.data() + text.size();
}

[[nodiscard]] bool EqualsAsciiCaseInsensitiveUVE(const std::string_view left, const std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < left.size(); ++index) {
        const auto leftCharacter = static_cast<unsigned char>(left[index]);
        const auto rightCharacter = static_cast<unsigned char>(right[index]);
        if (std::tolower(leftCharacter) != std::tolower(rightCharacter)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<std::size_t> ParseCommaSeparatedNumbersUVE(const std::string_view text,
                                                                        const std::span<double> values) noexcept {
    std::size_t count = 0U;
    std::size_t start = 0U;
    while (true) {
        if (count >= values.size()) {
            return std::nullopt;
        }
        const std::size_t comma = text.find(',', start);
        const std::string_view component = comma == std::string_view::npos
                                               ? text.substr(start)
                                               : text.substr(start, comma - start);
        if (!ParseCommandLineNumberUVE(component, values[count])) {
            return std::nullopt;
        }
        ++count;
        if (comma == std::string_view::npos) {
            return count;
        }
        start = comma + 1U;
        if (start == text.size()) {
            return std::nullopt;
        }
    }
}

[[nodiscard]] std::optional<Config::SettingValueUVE> ParseCommandLineSettingValueUVE(
    const Config::SettingDescriptorUVE& descriptor, const std::optional<std::string>& argument) {
    if (!argument.has_value()) {
        return descriptor.type == Config::SettingTypeUVE::Bool
                   ? std::optional<Config::SettingValueUVE>{Config::SettingValueUVE{true}}
                   : std::nullopt;
    }
    const std::string_view text = *argument;
    switch (descriptor.type) {
    case Config::SettingTypeUVE::Bool:
        if (EqualsAsciiCaseInsensitiveUVE(text, "true") || text == "1" ||
            EqualsAsciiCaseInsensitiveUVE(text, "on")) {
            return Config::SettingValueUVE{true};
        }
        if (EqualsAsciiCaseInsensitiveUVE(text, "false") || text == "0" ||
            EqualsAsciiCaseInsensitiveUVE(text, "off")) {
            return Config::SettingValueUVE{false};
        }
        return std::nullopt;
    case Config::SettingTypeUVE::Int: {
        std::int64_t value = 0;
        return ParseCommandLineNumberUVE(text, value) ? std::optional<Config::SettingValueUVE>{value} : std::nullopt;
    }
    case Config::SettingTypeUVE::Float: {
        double value = 0.0;
        return ParseCommandLineNumberUVE(text, value) ? std::optional<Config::SettingValueUVE>{value} : std::nullopt;
    }
    case Config::SettingTypeUVE::String:
    case Config::SettingTypeUVE::FilePath:
    case Config::SettingTypeUVE::KeyBinding:
        return Config::SettingValueUVE{std::string{text}};
    case Config::SettingTypeUVE::StringList:
        return std::nullopt; // The command-line layer does not split or escape list values.
    case Config::SettingTypeUVE::Enum: {
        std::int64_t value = 0;
        if (ParseCommandLineNumberUVE(text, value)) {
            return Config::SettingValueUVE{value};
        }
        for (const Config::SettingEnumEntryUVE& entry : descriptor.enumEntries) {
            if (EqualsAsciiCaseInsensitiveUVE(text, entry.label)) {
                return Config::SettingValueUVE{entry.value};
            }
        }
        return std::nullopt;
    }
    case Config::SettingTypeUVE::Color: {
        std::array<double, 4U> channels{};
        const std::optional<std::size_t> count = ParseCommaSeparatedNumbersUVE(text, channels);
        if (!count.has_value() || *count < 3U) {
            return std::nullopt;
        }
        Config::SettingColorUVE color{static_cast<float>(channels[0U]), static_cast<float>(channels[1U]),
                                      static_cast<float>(channels[2U])};
        if (*count == 4U) {
            color.a = static_cast<float>(channels[3U]);
        }
        return Config::SettingValueUVE{color};
    }
    case Config::SettingTypeUVE::Vector3: {
        std::array<double, 3U> components{};
        const std::optional<std::size_t> count = ParseCommaSeparatedNumbersUVE(text, components);
        if (!count.has_value() || *count != components.size()) {
            return std::nullopt;
        }
        return Config::SettingValueUVE{Config::SettingVector3UVE{components[0U], components[1U], components[2U]}};
    }
    }
    return std::nullopt;
}

void PopulateEngineCommandLineSettingsUVEInternal(const Config::SettingsRegistryUVE& registry,
                                    const CommandLine::ICommandLineUVE& commandLine,
                                    Config::IConfigManagerUVE& store) {
    for (const Config::SettingDescriptorUVE* descriptor : registry.GetAllUVE()) {
        if (!IsEngineConfigSettingIdUVE(descriptor->id) || !commandLine.HasFlagUVE(descriptor->id)) {
            continue;
        }
        const std::optional<Config::SettingValueUVE> value =
            ParseCommandLineSettingValueUVE(*descriptor, commandLine.GetOptionalValueUVE(descriptor->id));
        if (!value.has_value() || !registry.SetValueUVE(store, descriptor->id, *value)) {
            UVE_WARNING("EngineSettingsUVE: ignoring invalid command-line setting override '--{}'", descriptor->id);
        }
    }
}

} // namespace

std::string GetLayerNameSettingIdUVE(const LayerSetUVE set, const std::size_t index) {
    return std::string(set == LayerSetUVE::Physics ? "layers.physics." : "layers.render.") +
           std::to_string(index + 1U);
}

std::string GetLayerNameUVE(const Config::SettingsDocumentUVE& document, const LayerSetUVE set,
                            const std::size_t index) {
    if (index >= kLayerCountUVE) {
        return {};
    }
    const std::optional<SettingValueUVE> name = document.GetValueUVE(GetLayerNameSettingIdUVE(set, index));
    const auto* text = name ? std::get_if<std::string>(&*name) : nullptr;
    return text != nullptr ? *text : std::string{};
}

bool RegisterEngineProjectSettingsUVE(Config::SettingsRegistryUVE& registry) {
    bool allRegistered = true;
    for (const EngineProjectSettingUVE& setting : GetEngineProjectSettingsUVE()) {
        allRegistered = registry.RegisterUVE(setting.descriptor) && allRegistered;
    }
    constexpr std::size_t kMaximumContentPathBytesUVE = 512U;
    allRegistered = registry.RegisterUVE(Config::MakeFilePathSettingUVE(
                        std::string(EngineProjectSettingIdUVE::kDefaultPlayerEntityUVE), "",
                        kMaximumContentPathBytesUVE, "Default Player", "Game/Player",
                        "The entity asset (.uventity, relative to Content) the player is spawned from. Set it "
                        "from the Content panel: right-click an entity, Set as Default Player.")) &&
                    allRegistered;
    // Layer names, 1 to 32 as a person counts them; bit 0 is layer 1.
    constexpr std::size_t kMaximumLayerNameBytesUVE = 32U;
    for (const LayerSetUVE set : {LayerSetUVE::Physics, LayerSetUVE::Render}) {
        const bool physics = set == LayerSetUVE::Physics;
        for (std::size_t index = 0U; index < kLayerCountUVE; ++index) {
            allRegistered =
                registry.RegisterUVE(Config::MakeStringSettingUVE(
                    GetLayerNameSettingIdUVE(set, index), "", kMaximumLayerNameBytesUVE,
                    "Layer " + std::to_string(index + 1U), physics ? "Layers/Physics" : "Layers/Render",
                    physics ? "Shown wherever a collider's layer or mask is picked."
                            : "Shown wherever a mesh's, light's or decal's render layers are picked.")) &&
                allRegistered;
        }
    }
    return allRegistered;
}

bool IsEngineConfigSettingIdUVE(const std::string_view id) {
    const auto& settings = GetEngineProjectSettingsUVE();
    return std::any_of(settings.begin(), settings.end(),
                       [id](const EngineProjectSettingUVE& setting) { return setting.descriptor.id == id; });
}

void PopulateEngineCommandLineSettingsUVE(const Config::SettingsRegistryUVE& registry,
                                          const CommandLine::ICommandLineUVE& commandLine,
                                          Config::IConfigManagerUVE& store) {
    PopulateEngineCommandLineSettingsUVEInternal(registry, commandLine, store);
}

std::filesystem::path GetPlatformSettingsFilePathUVE(const EngineConfigUVE& config) {
    if (!config.platformSettingsFilePath.empty()) {
        return config.platformSettingsFilePath;
    }
    return config.settingsFilePath.parent_path() / "platforms" / std::string{GetPlatformSettingsTargetUVE()} /
           config.settingsFilePath.filename();
}

bool AttachEngineSettingsLayersUVE(Config::SettingsStackUVE& settings, const Config::IConfigManagerUVE& project,
                                   const Config::IConfigManagerUVE& user, const Config::IConfigManagerUVE& platform,
                                   const Config::IConfigManagerUVE& commandLine) noexcept {
    return settings.AttachLayerUVE(Config::SettingValueSourceUVE::Project, project) &&
           settings.AttachLayerUVE(Config::SettingValueSourceUVE::User, user) &&
           settings.AttachLayerUVE(Config::SettingValueSourceUVE::Platform, platform) &&
           settings.AttachLayerUVE(Config::SettingValueSourceUVE::CommandLine, commandLine);
}

void ApplyEngineSettingsUVE(const Config::SettingsStackUVE& settings, EngineConfigUVE& config) {
    for (const EngineProjectSettingUVE& setting : GetEngineProjectSettingsUVE()) {
        if (const std::optional<Config::SettingResolutionUVE> resolved =
                settings.ResolveUVE(setting.descriptor.id);
            resolved.has_value() && resolved->source != Config::SettingValueSourceUVE::EngineDefault) {
            setting.apply(config, resolved->value);
        }
    }
}

void ApplyEngineProjectSettingsUVE(const Config::SettingsDocumentUVE& document, EngineConfigUVE& config) {
    Config::SettingsStackUVE settings(document.GetRegistryUVE());
    if (settings.AttachLayerUVE(Config::SettingValueSourceUVE::Project, document.GetStoreUVE())) {
        ApplyEngineSettingsUVE(settings, config);
    }
}

} // namespace UVE::Core
