// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/core/engine_core_uve.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "uve/core/engine_project_settings_uve.h"

namespace UVE::Core::Tests {
namespace {

class SettingsTestDirectoryUVE final {
public:
    SettingsTestDirectoryUVE() {
        static std::atomic<std::uint64_t> sequence{0U};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() /
                 ("uve_engine_core_settings_" + std::to_string(timestamp) + "_" +
                  std::to_string(sequence.fetch_add(1U, std::memory_order_relaxed)));
        std::filesystem::create_directories(m_path);
    }

    ~SettingsTestDirectoryUVE() {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    [[nodiscard]] const std::filesystem::path& GetPathUVE() const noexcept { return m_path; }

private:
    std::filesystem::path m_path;
};

[[nodiscard]] bool WriteTextFileUVE(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output << text;
    return output.good();
}

[[nodiscard]] EngineConfigUVE MakeSettingsTestConfigUVE(const std::filesystem::path& directory) {
    EngineConfigUVE config{};
    config.enableConsoleLogging = false;
    config.logFilePath = directory / "engine.log";
    config.threadPoolWorkerCount = 1U;
    config.projectSettingsFilePath = directory / "project.uvsettings";
    config.settingsFilePath = directory / "user.uvsettings";
    config.platformSettingsFilePath = directory / "platform.uvsettings";
    config.inputMapFilePath = directory / "project.uvinput";
    config.assetDatabaseFilePath = directory / "assets.uvassetdb";
    config.headlessUVE = true;
    return config;
}

TEST(EngineCoreSettingsLayeringUVETest, LoadsAllLayersAndAppliesCommandLineBeforeCoreStartupCompletes) {
    SettingsTestDirectoryUVE directory;
    EngineConfigUVE config = MakeSettingsTestConfigUVE(directory.GetPathUVE());
    config.headlessUVE = false; // The presence-only CLI flag must resolve into EngineConfig.
    config.autoSaveIntervalSecondsUVE = 777.0; // Keep the caller's base when no setting layer supplies it.
    config.commandLineArgs = {"--physics.common.ticksPerSecond", "120",
                              "--rendering.shadows.mapResolution", "512", "--headless"};
    ASSERT_TRUE(WriteTextFileUVE(
        config.projectSettingsFilePath,
        R"({"physics":{"common":{"ticksPerSecond":30,"maxFrameTime":0.5,"maxStepsPerFrame":16}},"rendering":{"shadows":{"mapResolution":4096}}})"));
    ASSERT_TRUE(WriteTextFileUVE(
        config.settingsFilePath,
        R"({"physics":{"common":{"ticksPerSecond":90,"maxFrameTime":0.4}},"rendering":{"shadows":{"mapResolution":2048}}})"));
    ASSERT_TRUE(WriteTextFileUVE(config.platformSettingsFilePath,
                                 R"({"rendering":{"shadows":{"mapResolution":1024}}})"));

    EngineCoreUVE engine(config);
    engine.Init();
    EXPECT_DOUBLE_EQ(engine.GetConfigUVE().fixedUpdateFps, 120.0);
    EXPECT_DOUBLE_EQ(engine.GetConfigUVE().maxDeltaTimeSeconds, 0.4);
    EXPECT_EQ(engine.GetConfigUVE().maxFixedStepsPerFrame, 16);
    EXPECT_EQ(engine.GetConfigUVE().shadowMapResolution, 512U);
    EXPECT_TRUE(engine.GetConfigUVE().headlessUVE);
    EXPECT_DOUBLE_EQ(engine.GetConfigUVE().autoSaveIntervalSecondsUVE, 777.0);
    engine.Shutdown();
}

TEST(EngineCoreSettingsLayeringUVETest, InvalidCommandLineValueFallsThroughToLoadedPlatformFile) {
    SettingsTestDirectoryUVE directory;
    EngineConfigUVE config = MakeSettingsTestConfigUVE(directory.GetPathUVE());
    config.platformSettingsFilePath.clear(); // Exercise the generated platforms/<target>/ path.
    config.commandLineArgs = {"--rendering.shadows.mapResolution", "1536"};
    ASSERT_TRUE(WriteTextFileUVE(config.projectSettingsFilePath,
                                 R"({"rendering":{"shadows":{"mapResolution":4096}}})"));
    ASSERT_TRUE(WriteTextFileUVE(config.settingsFilePath,
                                 R"({"rendering":{"shadows":{"mapResolution":2048}}})"));
    const std::filesystem::path platformPath = GetPlatformSettingsFilePathUVE(config);
    ASSERT_TRUE(std::filesystem::create_directories(platformPath.parent_path()));
    ASSERT_TRUE(WriteTextFileUVE(platformPath, R"({"rendering":{"shadows":{"mapResolution":1024}}})"));

    EngineCoreUVE engine(config);
    engine.Init();
    EXPECT_EQ(engine.GetConfigUVE().shadowMapResolution, 1024U);
    engine.Shutdown();
}

} // namespace
} // namespace UVE::Core::Tests
