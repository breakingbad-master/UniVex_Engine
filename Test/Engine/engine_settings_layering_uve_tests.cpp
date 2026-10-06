// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/core/engine_project_settings_uve.h"

#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "uve/commandline/command_line_uve.h"
#include "uve/config/config_manager_uve.h"

namespace UVE::Core::Tests {
namespace {

using Config::ConfigManagerUVE;
using Config::SettingValueSourceUVE;

TEST(EngineSettingsLayeringUVETest, AttachesLayersAndAppliesResolvedSourcesToEngineConfig) {
    Config::SettingsRegistryUVE registry;
    ASSERT_TRUE(RegisterEngineProjectSettingsUVE(registry));

    ConfigManagerUVE project;
    ConfigManagerUVE user;
    ConfigManagerUVE platform;
    ConfigManagerUVE commandLine;
    Config::SettingsStackUVE settings(registry);
    ASSERT_TRUE(AttachEngineSettingsLayersUVE(settings, project, user, platform, commandLine));

    namespace Id = EngineProjectSettingIdUVE;
    project.SetDoubleUVE(Id::kPhysicsTicksPerSecondUVE, 30.0);
    user.SetDoubleUVE(Id::kPhysicsTicksPerSecondUVE, 90.0);
    platform.SetDoubleUVE(Id::kPhysicsTicksPerSecondUVE, 240.0); // Not marked PerPlatform.

    project.SetIntUVE(Id::kShadowMapResolutionUVE, 4096);
    user.SetIntUVE(Id::kShadowMapResolutionUVE, 2048);
    platform.SetIntUVE(Id::kShadowMapResolutionUVE, 1024);
    commandLine.SetIntUVE(Id::kShadowMapResolutionUVE, 512);

    // NotPersisted settings only read from the command-line layer.
    project.SetBoolUVE(Id::kHeadlessUVE, true);
    user.SetBoolUVE(Id::kHeadlessUVE, true);
    platform.SetBoolUVE(Id::kHeadlessUVE, true);
    commandLine.SetBoolUVE(Id::kHeadlessUVE, true);
    project.SetBoolUVE(Id::kQuitOnLastWindowClosedUVE, true);
    user.SetBoolUVE(Id::kQuitOnLastWindowClosedUVE, false);
    commandLine.SetBoolUVE(Id::kQuitOnLastWindowClosedUVE, true);

    const auto ticks = settings.ResolveUVE(Id::kPhysicsTicksPerSecondUVE);
    ASSERT_TRUE(ticks.has_value());
    EXPECT_EQ(ticks->source, SettingValueSourceUVE::User);
    EXPECT_DOUBLE_EQ(std::get<double>(ticks->value), 90.0);
    EXPECT_FALSE(settings.GetStoredValueUVE(SettingValueSourceUVE::Platform, Id::kPhysicsTicksPerSecondUVE)
                     .has_value());

    const auto shadowResolution = settings.ResolveUVE(Id::kShadowMapResolutionUVE);
    ASSERT_TRUE(shadowResolution.has_value());
    EXPECT_EQ(shadowResolution->source, SettingValueSourceUVE::CommandLine);
    EXPECT_EQ(std::get<std::int64_t>(shadowResolution->value), 512);

    const auto headless = settings.ResolveUVE(Id::kHeadlessUVE);
    ASSERT_TRUE(headless.has_value());
    EXPECT_EQ(headless->source, SettingValueSourceUVE::CommandLine);

    EngineConfigUVE config{};
    config.autoSaveIntervalSecondsUVE = 777.0; // No layer supplies this setting; preserve the caller's base.
    ApplyEngineSettingsUVE(settings, config);
    EXPECT_DOUBLE_EQ(config.fixedUpdateFps, 90.0);
    EXPECT_EQ(config.shadowMapResolution, 512U);
    EXPECT_TRUE(config.headlessUVE);
    EXPECT_TRUE(config.quitOnLastWindowClosedUVE);
    EXPECT_DOUBLE_EQ(config.autoSaveIntervalSecondsUVE, 777.0);

    // An invalid command-line enum value must not mask the valid platform value below it.
    commandLine.SetIntUVE(Id::kShadowMapResolutionUVE, 1536);
    const auto fallback = settings.ResolveUVE(Id::kShadowMapResolutionUVE);
    ASSERT_TRUE(fallback.has_value());
    EXPECT_EQ(fallback->source, SettingValueSourceUVE::Platform);
    EXPECT_EQ(std::get<std::int64_t>(fallback->value), 1024);
    ApplyEngineSettingsUVE(settings, config);
    EXPECT_EQ(config.shadowMapResolution, 1024U);
}

TEST(EngineSettingsLayeringUVETest, WindowSettingsApplyTypedPoliciesAndRespectPlatformOverrides) {
    Config::SettingsRegistryUVE registry;
    ASSERT_TRUE(RegisterEngineProjectSettingsUVE(registry));
    namespace Id = EngineProjectSettingIdUVE;

    ConfigManagerUVE project;
    ConfigManagerUVE user;
    ConfigManagerUVE platform;
    ConfigManagerUVE commandLine;
    Config::SettingsStackUVE settings(registry);
    ASSERT_TRUE(AttachEngineSettingsLayersUVE(settings, project, user, platform, commandLine));

    project.SetIntUVE(Id::kWindowWidthUVE, 1920);
    project.SetIntUVE(Id::kWindowHeightUVE, 1080);
    project.SetIntUVE(Id::kWindowModeUVE, 3);
    project.SetIntUVE(Id::kWindowVSyncModeUVE, 2);
    project.SetIntUVE(Id::kWindowMaximumWidthUVE, 2560);
    project.SetDoubleUVE(Id::kWindowContentScaleOverrideUVE, 1.5);
    project.SetIntUVE(Id::kWindowFocusedFrameRateCapUVE, 144);
    project.SetIntUVE(Id::kWindowUnfocusedFrameRateCapUVE, 30);
    project.SetStringUVE(Id::kWindowTitleFormatUVE, "{projectName} — {sceneName}");
    project.SetBoolUVE(Id::kWindowAppendSceneInEditorPlayUVE, true);
    ASSERT_TRUE(registry.SetValueUVE(
        project, Id::kWindowAllowedOrientationsUVE,
        Config::SettingValueUVE{Config::SettingStringListUVE{"landscapeLeft", "portrait"}}));
    platform.SetBoolUVE(Id::kWindowPerMonitorScalingUVE, false);
    user.SetBoolUVE(Id::kWindowAlwaysOnTopUVE, true);

    EngineConfigUVE config;
    ApplyEngineSettingsUVE(settings, config);
    EXPECT_EQ(config.windowWidth, 1920U);
    EXPECT_EQ(config.windowHeight, 1080U);
    EXPECT_EQ(config.windowModeUVE, Platform::WindowModeUVE::ExclusiveFullscreen);
    EXPECT_EQ(config.vsyncModeUVE, Platform::VSyncModeUVE::Adaptive);
    EXPECT_TRUE(config.vsyncModeExplicitUVE);
    EXPECT_TRUE(config.vsyncEnabledUVE);
    EXPECT_EQ(config.windowMaximumWidthUVE, 2560U);
    EXPECT_DOUBLE_EQ(config.contentScaleOverrideUVE, 1.5);
    EXPECT_FALSE(config.perMonitorScalingUVE);
    EXPECT_TRUE(config.windowAlwaysOnTopUVE);
    EXPECT_EQ(config.focusedFrameRateCapUVE, 144U);
    EXPECT_EQ(config.unfocusedFrameRateCapUVE, 30U);
    EXPECT_EQ(config.allowedOrientationsUVE,
              (std::vector<Platform::DisplayOrientationUVE>{Platform::DisplayOrientationUVE::LandscapeLeft,
                                                           Platform::DisplayOrientationUVE::Portrait}));
    EXPECT_EQ(config.windowTitleFormatUVE, "{projectName} — {sceneName}");
    EXPECT_TRUE(config.appendSceneNameInEditorPlayModeUVE);
    EXPECT_TRUE(IsEngineConfigSettingIdUVE(Id::kWindowModeUVE));
    EXPECT_TRUE(IsEngineConfigSettingIdUVE(Id::kWindowCursorImageUVE));
}

TEST(EngineSettingsLayeringUVETest, ConvertsTypedCommandLineFlagsAndUsesThemAsTopLayer) {
    Config::SettingsRegistryUVE registry;
    ASSERT_TRUE(RegisterEngineProjectSettingsUVE(registry));

    CommandLine::CommandLineUVE commandLine({
        "--physics.common.ticksPerSecond", "120",
        "--physics.common.maxFrameTime", "0.25",
        "--physics.common.maxStepsPerFrame", "12",
        "--physics.3d.gravity", "1,-9.81,2",
        "--rendering.shadows.filter", "softer (5x5)",
        "--application.quitOnLastWindowClosed", "false",
        "--application.boot.backgroundColor", "0.1,0.2,0.3,0.4",
        "--application.boot.minimumDisplaySeconds", "2.5",
        "--application.boot.skippable", "false",
        "--application.crash.enabled", "false",
        "--headless",
        "--layers.physics.1", "CLI must not register project layer names",
    });
    ConfigManagerUVE commandLineValues;
    PopulateEngineCommandLineSettingsUVE(registry, commandLine, commandLineValues);

    namespace Id = EngineProjectSettingIdUVE;
    const auto ticksValue = registry.GetStoredValueUVE(commandLineValues, Id::kPhysicsTicksPerSecondUVE);
    ASSERT_TRUE(ticksValue.has_value());
    EXPECT_DOUBLE_EQ(std::get<double>(*ticksValue), 120.0);

    const auto maxFrameTime = registry.GetStoredValueUVE(commandLineValues, Id::kPhysicsMaxFrameTimeUVE);
    ASSERT_TRUE(maxFrameTime.has_value());
    EXPECT_DOUBLE_EQ(std::get<double>(*maxFrameTime), 0.25);

    const auto maxSteps = registry.GetStoredValueUVE(commandLineValues, Id::kPhysicsMaxStepsPerFrameUVE);
    ASSERT_TRUE(maxSteps.has_value());
    EXPECT_EQ(std::get<std::int64_t>(*maxSteps), 12);

    const auto gravity = registry.GetStoredValueUVE(commandLineValues, Id::kPhysicsGravityUVE);
    ASSERT_TRUE(gravity.has_value());
    EXPECT_EQ(std::get<Config::SettingVector3UVE>(*gravity), (Config::SettingVector3UVE{1.0, -9.81, 2.0}));

    const auto shadowFilter = registry.GetStoredValueUVE(commandLineValues, Id::kShadowFilterUVE);
    ASSERT_TRUE(shadowFilter.has_value());
    EXPECT_EQ(std::get<std::int64_t>(*shadowFilter), 2);

    const auto headless = registry.GetStoredValueUVE(commandLineValues, Id::kHeadlessUVE);
    ASSERT_TRUE(headless.has_value());
    EXPECT_TRUE(std::get<bool>(*headless));
    const auto quitOnLastWindowClosed =
        registry.GetStoredValueUVE(commandLineValues, Id::kQuitOnLastWindowClosedUVE);
    ASSERT_TRUE(quitOnLastWindowClosed.has_value());
    EXPECT_FALSE(std::get<bool>(*quitOnLastWindowClosed));
    const auto bootBackground = registry.GetStoredValueUVE(commandLineValues, Id::kBootBackgroundColorUVE);
    ASSERT_TRUE(bootBackground.has_value());
    EXPECT_EQ(std::get<Config::SettingColorUVE>(*bootBackground),
              (Config::SettingColorUVE{0.1F, 0.2F, 0.3F, 0.4F}));
    EXPECT_FALSE(registry.GetStoredValueUVE(commandLineValues, "layers.physics.1").has_value());

    ConfigManagerUVE project;
    ConfigManagerUVE user;
    ConfigManagerUVE platform;
    project.SetDoubleUVE(Id::kPhysicsTicksPerSecondUVE, 30.0);
    user.SetDoubleUVE(Id::kPhysicsTicksPerSecondUVE, 60.0);
    platform.SetIntUVE(Id::kShadowMapResolutionUVE, 1024);
    Config::SettingsStackUVE settings(registry);
    ASSERT_TRUE(AttachEngineSettingsLayersUVE(settings, project, user, platform, commandLineValues));

    const auto resolution = settings.ResolveUVE(Id::kPhysicsTicksPerSecondUVE);
    ASSERT_TRUE(resolution.has_value());
    EXPECT_EQ(resolution->source, SettingValueSourceUVE::CommandLine);
    EngineConfigUVE config{};
    ApplyEngineSettingsUVE(settings, config);
    EXPECT_DOUBLE_EQ(config.fixedUpdateFps, 120.0);
    EXPECT_DOUBLE_EQ(config.maxDeltaTimeSeconds, 0.25);
    EXPECT_EQ(config.maxFixedStepsPerFrame, 12);
    EXPECT_FLOAT_EQ(config.gravity.x, 1.0F);
    EXPECT_FLOAT_EQ(config.gravity.y, -9.81F);
    EXPECT_FLOAT_EQ(config.gravity.z, 2.0F);
    EXPECT_EQ(config.shadowPcfKernelRadius, 2U);
    EXPECT_TRUE(config.headlessUVE);
    EXPECT_FALSE(config.quitOnLastWindowClosedUVE);
    EXPECT_FLOAT_EQ(config.backgroundColorUVE[0U], 0.1F);
    EXPECT_FLOAT_EQ(config.backgroundColorUVE[1U], 0.2F);
    EXPECT_FLOAT_EQ(config.backgroundColorUVE[2U], 0.3F);
    EXPECT_FLOAT_EQ(config.backgroundColorUVE[3U], 0.4F);
    EXPECT_DOUBLE_EQ(config.splashMinimumDisplaySecondsUVE, 2.5);
    EXPECT_FALSE(config.splashSkippableUVE);
    EXPECT_FALSE(config.crashHandlerEnabledUVE);
}

TEST(EngineSettingsLayeringUVETest, RejectsInvalidTypedCliValueAndPreservesPlatformFallback) {
    Config::SettingsRegistryUVE registry;
    ASSERT_TRUE(RegisterEngineProjectSettingsUVE(registry));

    CommandLine::CommandLineUVE commandLine({"--rendering.shadows.mapResolution", "1536"});
    ConfigManagerUVE commandLineValues;
    PopulateEngineCommandLineSettingsUVE(registry, commandLine, commandLineValues);
    EXPECT_FALSE(registry.GetStoredValueUVE(commandLineValues, EngineProjectSettingIdUVE::kShadowMapResolutionUVE)
                     .has_value());

    ConfigManagerUVE project;
    ConfigManagerUVE user;
    ConfigManagerUVE platform;
    project.SetIntUVE(EngineProjectSettingIdUVE::kShadowMapResolutionUVE, 4096);
    user.SetIntUVE(EngineProjectSettingIdUVE::kShadowMapResolutionUVE, 2048);
    platform.SetIntUVE(EngineProjectSettingIdUVE::kShadowMapResolutionUVE, 1024);

    Config::SettingsStackUVE settings(registry);
    ASSERT_TRUE(AttachEngineSettingsLayersUVE(settings, project, user, platform, commandLineValues));
    const auto resolution = settings.ResolveUVE(EngineProjectSettingIdUVE::kShadowMapResolutionUVE);
    ASSERT_TRUE(resolution.has_value());
    EXPECT_EQ(resolution->source, SettingValueSourceUVE::Platform);
    EXPECT_EQ(std::get<std::int64_t>(resolution->value), 1024);

    EngineConfigUVE config{};
    ApplyEngineSettingsUVE(settings, config);
    EXPECT_EQ(config.shadowMapResolution, 1024U);
}

TEST(EngineSettingsLayeringUVETest, ResolvesPlatformSettingsPathAndHonorsExplicitOverride) {
    EngineConfigUVE config{};
    config.settingsFilePath = std::filesystem::path{"User"} / "settings.uvsettings";

    const std::filesystem::path generatedPath = GetPlatformSettingsFilePathUVE(config);
    EXPECT_EQ(generatedPath.filename(), config.settingsFilePath.filename());
    EXPECT_EQ(generatedPath.parent_path().parent_path().filename(), "platforms");
    EXPECT_EQ(generatedPath.parent_path().parent_path().parent_path(), config.settingsFilePath.parent_path());

    config.platformSettingsFilePath = std::filesystem::path{"Overrides"} / "desktop.uvsettings";
    EXPECT_EQ(GetPlatformSettingsFilePathUVE(config), config.platformSettingsFilePath);
}

} // namespace
} // namespace UVE::Core::Tests
