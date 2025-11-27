#include "CLI11.hpp"
#include "recorder_controller.h"
#include "player_controller.h"
#include "zet_backend/recorder_adapter.h"
#include "zet_backend/player_adapter.h"
#include "mcap_backend/recorder_adapter.h"
#include "mcap_backend/player_adapter.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>

std::string get_file_path(const std::string &base, const std::string &type) {
  // Get anything after the last /
  size_t last_slash = base.find_last_of("/\\");
  std::string filename = (last_slash == std::string::npos)
                             ? base
                             : base.substr(last_slash + 1);

  if (filename.find('.') != std::string::npos) {
    return base;
  }

  return base + "." + type;
}

int main(int argc, char **argv) {
  CLI::App app{"timeskip - Zetabus recording and playback tool"};

  CLI::App *record =
      app.add_subcommand("record", "Record Zetabus subject(s) into a file");

  // Format current date and time as YYYY_MM_DD_HHMMSS
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::tm buf;
  localtime_r(&in_time_t, &buf);
  char timestamp[100];
  strftime(timestamp, sizeof(timestamp), "%Y_%m_%d_%H%M%S", &buf);

  std::string subject;
  std::string output_file = std::string("timeskip_") + timestamp;
  std::string file_type = "mcap";
  std::string nats_url;

  record
      ->add_option("subject,--subject", subject,
                   "Specify the Zetabus subject(s) to record")
      ->required();
  record->add_option("output,-o,--output", output_file,
                     "Specify the output file");
  record
      ->add_option("--format", file_type,
                   "Specify the recording file format (zet or mcap)")
      ->check(CLI::IsMember({"zet", "mcap"}))
      ->default_val("mcap");
  record->add_option(
      "-s,--server", nats_url,
      "NATS server URL (default: env NATS_URL or nats://localhost:4222)");

  CLI::App *play =
      app.add_subcommand("play", "Play back a recorded Zetabus file");
  std::string playback_file;
  std::string play_nats_url;
  double speed = 1.0;
  bool interactive = true;

  play->add_option("file", playback_file, "The recorded file to play back")
      ->required();
  play->add_option(
      "-s,--server", play_nats_url,
      "NATS server URL (default: env NATS_URL or nats://localhost:4222)");
  play->add_option("--speed", speed,
                   "Playback speed multiplier (1.0=real-time, 2.0=2x, 0=max)")
      ->default_val(1.0);
  play->add_flag("--no-interactive,!--interactive", interactive,
                 "Disable interactive controls")
      ->default_val(true);

  CLI11_PARSE(app, argc, argv);

  // Determine NATS server URL (shared logic)
  auto get_server_url = [](const std::string& url_arg) {
    if (!url_arg.empty()) {
      return url_arg;
    }
    const char *env_url = std::getenv("NATS_URL");
    if (env_url) {
      return std::string(env_url);
    }
    return std::string("nats://localhost:4222");
  };

  if (record->parsed()) {
    std::string server_url = get_server_url(nats_url);
    std::string file_path = get_file_path(output_file, file_type);

    std::cout << "🔴 Recording Zetabus subject: " << subject << "\n";
    std::cout << "📁 Output file: " << file_path << "\n";
    std::cout << "🌐 NATS server: " << server_url << "\n";
    if (file_type == "zet") {
      std::cout << "⚠️  .zet format is deprecated\n";
    }
    std::cout << "\n🎮 Controls:\n";
    std::cout << "  p      : Pause/Resume recording\n";
    std::cout << "  Ctrl+C : Stop and save\n\n";

    // Create appropriate backend
    std::unique_ptr<timeskip::IRecorder> recorder;
    if (file_type == "zet") {
      recorder = std::make_unique<timeskip::zet::ZetRecorderAdapter>(
          server_url.c_str(), subject.c_str(), file_path.c_str(), 0);
    } else {
      recorder = std::make_unique<timeskip::mcap::McapRecorderAdapter>(
          server_url.c_str(), subject.c_str(), file_path.c_str(), 0);
    }

    if (!recorder) {
      std::cerr << "❌ Failed to create recorder\n";
      return 1;
    }

    // Create controller and run
    timeskip::RecorderController controller(std::move(recorder), true);
    return controller.run();

  } else if (play->parsed()) {
    std::string server_url = get_server_url(play_nats_url);

    // Detect file type from extension
    std::string detected_type = "zet";
    if (playback_file.find(".mcap") != std::string::npos) {
      detected_type = "mcap";
    }

    std::cout << "▶️  Playing back recorded Zetabus file\n";
    std::cout << "📁 Input file: " << playback_file << "\n";
    std::cout << "🌐 NATS server: " << server_url << "\n";
    std::cout << "⚡ Speed: " << (speed == 0 ? "MAX" : std::to_string(speed))
              << "x\n";
    if (interactive) {
      std::cout << "🎮 Interactive mode: enabled\n";
      std::cout << "\n🎮 Controls:\n";
      std::cout << "  ←/→    : Seek backward/forward\n";
      std::cout << "  ↑/↓    : Speed up/down\n";
      std::cout << "  p/Space: Pause/Resume\n";
      std::cout << "  n      : Skip to next message\n";
      std::cout << "  q      : Quit\n";
    }
    std::cout << "\n";

    // Create appropriate backend
    std::unique_ptr<timeskip::IPlayer> player;
    if (detected_type == "zet") {
      player = std::make_unique<timeskip::zet::ZetPlayerAdapter>(
          server_url.c_str(), playback_file.c_str(), speed);
    } else {
      player = std::make_unique<timeskip::mcap::McapPlayerAdapter>(
          server_url.c_str(), playback_file.c_str(), speed);
    }

    if (!player) {
      std::cerr << "❌ Failed to create player (file not found or invalid format)\n";
      return 1;
    }

    // Create controller and run
    timeskip::PlayerController controller(std::move(player), interactive);
    return controller.run();

  } else {
    std::cout << app.help() << "\n";
    return 1;
  }

  return 0;
}
