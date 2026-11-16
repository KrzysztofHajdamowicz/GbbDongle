#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace gbb_dongle {

/// GbbOptimizer endpoint that exchanges a pairing code for the plant's cloud
/// settings (GbbOptimizer manual, MQTT chapter, "GbbDongle Pairing").
static constexpr const char *DEFAULT_PAIRING_URL = "https://gbboptimizer.gbbsoft.pl/Callback/GbbDongleJoin";

/// Domain every GbbOptimizer broker lives under. A pairing answer naming a
/// broker elsewhere is refused: the firmware also trusts ISRG Root X1, so any
/// host with a Let's Encrypt certificate would pass the MQTT TLS check and
/// receive the Plant Id and token at the first login.
static constexpr const char *PAIRING_BROKER_DOMAIN = ".gbbsoft.pl";

/// Longest plant name kept for display, in bytes (cut on a UTF-8 boundary).
static constexpr size_t PAIRING_PLANT_NAME_MAX = 64;

/// Cloud settings GbbOptimizer hands out for one pairing code.
struct PairingResult {
  std::string plant_id;
  std::string plant_name;  // typed by the plant owner; display only
  std::string plant_token;
  std::string mqtt_server;
  uint16_t mqtt_port{0};
};

/// Longest values the entities that store a result accept (their max_length).
struct PairingLimits {
  size_t plant_id{64};
  size_t plant_token{64};
  size_t mqtt_server{128};
};

/// Reduce a code as someone typed or pasted it to the form GbbOptimizer
/// issues: whitespace dropped, letters upper-cased, and a nine-symbol code
/// regrouped as X-XXXX-XXXX (e.g. "j eeh7abcd" -> "J-EEH7-ABCD"). Codes of
/// any other length keep the dashes they were typed with. Returns false when
/// the input cannot be a code (other characters, too short or too long).
bool normalize_pairing_code(const std::string &input, std::string &out);

/// Parse and check GbbOptimizer's 200 answer. Returns nullptr on success,
/// otherwise a short reason for the log; `out` is only complete on success.
const char *parse_pairing_response(const std::string &body, const PairingLimits &limits, PairingResult &out);

/// One-line description of a result for the panel: "Dom · G12345 · host".
std::string describe_pairing_result(const PairingResult &result);

struct PairingJob;

/// Pairing state machine: one HTTPS exchange of a code for the cloud
/// settings, then a result waiting for the user's confirmation in the panel.
/// The exchange runs on its own FreeRTOS task, so the TLS handshake (seconds
/// on an ESP32) never stalls the main loop and the RS485 timing; everything
/// else, including parsing the answer, happens on the main loop.
/// GbbDongle applies a confirmed result (see GbbDongle::confirm_pairing()).
///
/// The "Pairing Status" text sensor carries one of these values, which the
/// web panel translates: idle, working, confirm, applied, invalid_format,
/// invalid_code, rate_limited, network_error, server_error, bad_response,
/// expired, unavailable.
class CloudPairing {
 public:
  void set_ca_certificate(const char *pem) { this->ca_certificate_ = pem; }
  void set_user_agent(const std::string &user_agent) { this->user_agent_ = user_agent; }
  void set_limits(const PairingLimits &limits) { this->limits_ = limits; }
  void set_status_text_sensor(text_sensor::TextSensor *s) { this->status_sensor_ = s; }
  void set_plant_text_sensor(text_sensor::TextSensor *s) { this->plant_sensor_ = s; }

  /// Publish the starting state ("idle", or "unavailable" without a CA).
  void setup();
  /// Collect a finished exchange and drop a result nobody confirmed in time.
  void loop();

  /// Exchange `code` for the cloud settings. Ignored while an exchange runs;
  /// replaces a result that waits for confirmation.
  void start(const std::string &code);
  /// Hand over the result waiting for confirmation and mark it applied.
  /// Returns false when there is none.
  bool take_result(PairingResult &out);
  /// Drop the waiting result, or the answer of the running exchange.
  void cancel();

  /// Pairing needs the CA certificate: without one (bench builds with a
  /// plain-TCP broker) there is nothing to check GbbOptimizer's server with.
  bool available() const { return this->ca_certificate_ != nullptr; }
  const char *url() const { return this->url_; }

 protected:
  void launch_(const std::string &code);
  void finish_job_();
  void set_status_(const char *status);
  void set_plant_(const std::string &plant);
  void drop_result_();

  const char *url_{DEFAULT_PAIRING_URL};
  const char *ca_certificate_{nullptr};
  std::string user_agent_{"GbbDongle"};
  PairingLimits limits_;
  text_sensor::TextSensor *status_sensor_{nullptr};
  text_sensor::TextSensor *plant_sensor_{nullptr};

  PairingJob *job_{nullptr};  // exchange in flight, owned here until its task is done
  bool discard_job_{false};   // cancelled while in flight: ignore its answer
  std::string queued_code_;   // typed after a cancel, starts when the cancelled exchange returns
  bool has_result_{false};
  PairingResult result_;
  uint32_t result_at_{0};
};

}  // namespace gbb_dongle
}  // namespace esphome
