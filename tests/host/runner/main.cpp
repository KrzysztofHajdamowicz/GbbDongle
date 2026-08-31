#include "host_adapters.h"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <unistd.h>

using namespace esphome::gbb_dongle;
using namespace gbb_host;

static volatile std::sig_atomic_t signal_received = 0;
static void signal_handler(int) { signal_received = 1; }

static std::map<std::string, std::string> parse_args(int argc, char **argv) {
  std::map<std::string, std::string> options;
  for (int index = 1; index + 1 < argc; index += 2)
    options[argv[index]] = argv[index + 1];
  return options;
}

static std::string required(const std::map<std::string, std::string> &options, const std::string &name) {
  const auto found = options.find(name);
  if (found == options.end()) {
    std::cerr << "Missing required option " << name << '\n';
    std::exit(2);
  }
  return found->second;
}

int main(int argc, char **argv) {
  const auto options = parse_args(argc, argv);
  const std::string broker = required(options, "--broker");
  const uint16_t port = static_cast<uint16_t>(std::stoul(required(options, "--port")));
  const std::string plant_id = required(options, "--plant-id");
  const std::string token = required(options, "--token");
  const std::string uart_path = required(options, "--uart");
  const std::string state_file = required(options, "--state-file");
  const unsigned baud = options.count("--baud") ? static_cast<unsigned>(std::stoul(options.at("--baud"))) : 9600;
  const std::string parity = options.count("--parity") ? options.at("--parity") : "NONE";

  HostClock clock;
  PosixSerialPort serial(uart_path, baud, parity);
  if (!serial.valid()) {
    std::cerr << "Cannot open UART " << uart_path << '\n';
    return 2;
  }
  FileBlobStore blob(state_file);
  DongleCore core;
  std::vector<char> log_storage(64 * 1024);
  core.log_buffer().init(log_storage.data(), log_storage.size());
  core.log_buffer().set_level_gate(5);
  HostLogger logger(&core.log_buffer());
  core.set_clock(&clock);
  core.set_modbus_port(&serial);
  core.set_logger(&logger);
  core.set_blob_store(&blob);
  core.set_identity("host", "GbbDongle", "GbbDongle/host");
  core.set_topics(plant_id + "/ModbusInMqtt/fromDevice", plant_id + "/keepalive");
  core.set_client_info_provider([]() { return std::string("Host"); });
  core.set_cloud_configured(true);
  if (options.count("--response-timeout"))
    core.set_response_timeout(static_cast<uint32_t>(std::stoul(options.at("--response-timeout"))));
  if (options.count("--read-gap"))
    core.set_read_gap(static_cast<uint32_t>(std::stoul(options.at("--read-gap"))));
  if (options.count("--write-gap"))
    core.set_write_gap(static_cast<uint32_t>(std::stoul(options.at("--write-gap"))));
  core.set_baud_rate_hint(baud);
  if (options.count("--persist") && options.at("--persist") == "1") {
    core.set_persist_enabled(true);
    core.restore_emergency();
  }

  MosquittoCloud cloud(broker, port, plant_id, token, &core);
  core.set_cloud_transport(&cloud);
  if (!cloud.start()) {
    std::cerr << "Cannot initialize MQTT connection\n";
    return 2;
  }
  ControlSocket control(options.count("--control-socket") ? options.at("--control-socket") : "");
  if (!control.start()) {
    std::cerr << "Cannot create control socket\n";
    return 2;
  }

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);
  bool stop = false;
  while (!stop && signal_received == 0) {
    cloud.loop();
    control.poll(clock, core, stop);
    core.loop();
    usleep(1000);
  }
  return 0;
}
