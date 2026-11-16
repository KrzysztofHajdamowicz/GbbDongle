#include "cloud_pairing.h"

#include <atomic>
#include <memory>

#include <esp_http_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gbb_dongle {

static const char *const TAG = "gbb_dongle.pairing";

// GbbOptimizer codes: the account prefix of one or two letters, then eight
// symbols grouped 4-4, e.g. J-EEH7-ABCD or AB-EEH7-ABCD.
static const size_t CODE_BODY_SYMBOLS = 8;
static const size_t CODE_MAX_PREFIX = 2;
static const size_t CODE_MIN_SYMBOLS = 4;
static const size_t CODE_MAX_SYMBOLS = 32;

// How long a fetched result waits for the user's confirmation.
static const uint32_t RESULT_TTL_MS = 10 * 60 * 1000;

static const int HTTP_TIMEOUT_MS = 15 * 1000;
// The answer is a few hundred bytes; a much larger body is not that answer.
static const size_t MAX_RESPONSE_BYTES = 2048;
// Same stack as ESPHome's http_request update task: the mbedTLS handshake,
// including certificate verification, runs on it.
static const uint32_t TASK_STACK_BYTES = 8192;
static const UBaseType_t TASK_PRIORITY = 1;

/// One code exchange. The main loop fills the inputs and hands the job to the
/// task; the task writes the outputs and then sets `done`. Neither side
/// touches the job while the other one owns it.
struct PairingJob {
  std::string url;
  std::string body;
  std::string user_agent;
  const char *ca_pem{nullptr};

  esp_err_t error{ESP_OK};
  int status{0};
  bool too_large{false};
  std::string response;
  std::atomic<bool> done{false};
};

// --- Code normalization --------------------------------------------------

static bool is_ascii_alnum(unsigned char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool normalize_pairing_code(const std::string &input, std::string &out) {
  std::string typed;    // upper-cased, whitespace dropped, dashes kept
  std::string symbols;  // letters and digits only
  for (size_t i = 0; i < input.size(); i++) {
    const unsigned char c = input[i];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      continue;
    if (c == '-') {
      typed += '-';
      continue;
    }
    if (is_ascii_alnum(c)) {
      const char upper = (c >= 'a' && c <= 'z') ? (char) (c - 'a' + 'A') : (char) c;
      typed += upper;
      symbols += upper;
      continue;
    }
    // What phones and word processors substitute in a copied code: no-break
    // space (U+00A0), hyphens and dashes (U+2010..U+2015), minus (U+2212).
    const unsigned char c1 = i + 1 < input.size() ? input[i + 1] : 0;
    const unsigned char c2 = i + 2 < input.size() ? input[i + 2] : 0;
    if (c == 0xC2 && c1 == 0xA0) {
      i += 1;
      continue;
    }
    if (c == 0xE2 && ((c1 == 0x80 && c2 >= 0x90 && c2 <= 0x95) || (c1 == 0x88 && c2 == 0x92))) {
      typed += '-';
      i += 2;
      continue;
    }
    return false;
  }
  if (symbols.size() < CODE_MIN_SYMBOLS || symbols.size() > CODE_MAX_SYMBOLS)
    return false;
  const size_t count = symbols.size();
  if (count >= CODE_BODY_SYMBOLS && count <= CODE_BODY_SYMBOLS + CODE_MAX_PREFIX) {
    const size_t prefix = count - CODE_BODY_SYMBOLS;
    out = prefix > 0 ? symbols.substr(0, prefix) + "-" : "";
    out += symbols.substr(prefix, 4) + "-" + symbols.substr(prefix + 4, 4);
  } else {
    out = typed;
  }
  return true;
}

// --- Answer validation -----------------------------------------------------

// Plant Id ends up in MQTT topics ("<id>/ModbusInMqtt/toDevice"), the MQTT
// user name and the client id, so '/', '+', '#' and the like are refused.
static bool is_plant_id(const std::string &s) {
  for (unsigned char c : s) {
    if (!is_ascii_alnum(c) && c != '-' && c != '_')
      return false;
  }
  return true;
}

// Base64-like (the manual shows '+' and '/'); printable ASCII without spaces.
static bool is_plant_token(const std::string &s) {
  for (unsigned char c : s) {
    if (c < 0x21 || c > 0x7E)
      return false;
  }
  return true;
}

// Lower-cases `host` and checks it is a DNS name under PAIRING_BROKER_DOMAIN.
static const char *check_broker_host(std::string &host) {
  for (char &c : host) {
    if (c >= 'A' && c <= 'Z')
      c = (char) (c - 'A' + 'a');
  }
  size_t label = 0;
  for (size_t i = 0; i <= host.size(); i++) {
    const char c = i < host.size() ? host[i] : '.';
    if (c == '.') {
      if (label == 0 || host[i - 1] == '-' || host[i - label] == '-' || label > 63)
        return "mqttServer is not a host name";
      label = 0;
    } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
      label++;
    } else {
      return "mqttServer is not a host name";
    }
  }
  const std::string domain = PAIRING_BROKER_DOMAIN;
  if (host.size() <= domain.size() || host.compare(host.size() - domain.size(), domain.size(), domain) != 0)
    return "mqttServer is not a GbbOptimizer broker (*.gbbsoft.pl)";
  return nullptr;
}

// Well-formed UTF-8 only, without control or text-direction characters (they
// could reorder the line the panel shows), cut to `max` bytes.
static std::string clean_display_text(const std::string &in, size_t max) {
  std::string out;
  size_t i = 0;
  while (i < in.size()) {
    const unsigned char c = in[i];
    size_t len;
    if (c < 0x80) {
      len = 1;
    } else if (c >= 0xC2 && c <= 0xDF) {
      len = 2;
    } else if (c >= 0xE0 && c <= 0xEF) {
      len = 3;
    } else if (c >= 0xF0 && c <= 0xF4) {
      len = 4;
    } else {
      i++;
      continue;
    }
    bool valid = i + len <= in.size();
    for (size_t k = 1; valid && k < len; k++)
      valid = ((unsigned char) in[i + k] & 0xC0) == 0x80;
    if (!valid) {
      i++;
      continue;
    }
    const unsigned char c1 = len > 1 ? in[i + 1] : 0;
    const unsigned char c2 = len > 2 ? in[i + 2] : 0;
    const bool control = (len == 1 && (c < 0x20 || c == 0x7F)) || (len == 2 && c == 0xC2 && c1 < 0xA0);
    const bool bidi = len == 3 && c == 0xE2 &&
                      ((c1 == 0x80 && ((c2 >= 0x8E && c2 <= 0x8F) || (c2 >= 0xAA && c2 <= 0xAE))) ||
                       (c1 == 0x81 && c2 >= 0xA6 && c2 <= 0xA9));
    if (!control && !bidi) {
      if (out.size() + len > max)
        break;
      out.append(in, i, len);
    }
    i += len;
  }
  const size_t first = out.find_first_not_of(' ');
  if (first == std::string::npos)
    return "";
  return out.substr(first, out.find_last_not_of(' ') - first + 1);
}

const char *parse_pairing_response(const std::string &body, const PairingLimits &limits, PairingResult &out) {
  const char *reason = "answer is not a JSON object";
  json::parse_json(body, [&](JsonObject root) -> bool {
    if (root.isNull())
      return false;
    if (!root["plantId"].is<const char *>()) {
      reason = "plantId missing";
      return false;
    }
    out.plant_id = root["plantId"].as<const char *>();
    if (out.plant_id.empty() || out.plant_id.size() > limits.plant_id) {
      reason = "plantId empty or longer than the Plant Id entity accepts";
      return false;
    }
    if (!is_plant_id(out.plant_id)) {
      reason = "plantId has characters not allowed in an MQTT topic";
      return false;
    }
    if (!root["plantToken"].is<const char *>()) {
      reason = "plantToken missing";
      return false;
    }
    out.plant_token = root["plantToken"].as<const char *>();
    if (out.plant_token.empty() || out.plant_token.size() > limits.plant_token) {
      reason = "plantToken empty or longer than the Plant Token entity accepts";
      return false;
    }
    if (!is_plant_token(out.plant_token)) {
      reason = "plantToken has characters other than printable ASCII";
      return false;
    }
    if (!root["mqttServer"].is<const char *>()) {
      reason = "mqttServer missing";
      return false;
    }
    out.mqtt_server = root["mqttServer"].as<const char *>();
    if (out.mqtt_server.size() > limits.mqtt_server) {
      reason = "mqttServer longer than the MQTT Server entity accepts";
      return false;
    }
    reason = check_broker_host(out.mqtt_server);
    if (reason != nullptr)
      return false;
    if (!root["mqttPort"].is<int32_t>()) {
      reason = "mqttPort missing or not a whole number";
      return false;
    }
    const int32_t port = root["mqttPort"].as<int32_t>();
    if (port < 1 || port > 65535) {
      reason = "mqttPort out of range";
      return false;
    }
    out.mqtt_port = (uint16_t) port;
    out.plant_name.clear();
    if (root["plantName"].is<const char *>())
      out.plant_name = clean_display_text(root["plantName"].as<const char *>(), PAIRING_PLANT_NAME_MAX);
    return true;
  });
  return reason;
}

std::string describe_pairing_result(const PairingResult &result) {
  std::string text = result.plant_name.empty() ? result.plant_id : result.plant_name + " · " + result.plant_id;
  return text + " · " + result.mqtt_server;
}

// --- HTTPS exchange (pairing task) -------------------------------------------

static esp_err_t read_failure(int64_t value) {
  return value == -ESP_ERR_HTTP_EAGAIN ? ESP_ERR_TIMEOUT : ESP_FAIL;
}

static esp_err_t run_exchange(PairingJob &job) {
  esp_http_client_config_t config = {};
  config.url = job.url.c_str();
  config.method = HTTP_METHOD_POST;
  config.timeout_ms = HTTP_TIMEOUT_MS;
  config.cert_pem = job.ca_pem;
  // A redirect could point at another host or at plain http.
  config.disable_auto_redirect = true;
  config.user_agent = job.user_agent.c_str();
  config.buffer_size = 1024;
  config.buffer_size_tx = 1024;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr)
    return ESP_ERR_NO_MEM;
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_header(client, "Accept", "application/json");

  esp_err_t err = esp_http_client_open(client, (int) job.body.size());
  size_t sent = 0;
  while (err == ESP_OK && sent < job.body.size()) {
    const int written = esp_http_client_write(client, job.body.data() + sent, (int) (job.body.size() - sent));
    if (written <= 0) {
      err = ESP_FAIL;
    } else {
      sent += written;
    }
  }
  if (err == ESP_OK) {
    const int64_t length = esp_http_client_fetch_headers(client);
    if (length < 0) {
      err = read_failure(length);
    } else {
      job.status = esp_http_client_get_status_code(client);
    }
  }
  // Only the 200 answer carries data; the refusals are told apart by status.
  if (err == ESP_OK && job.status == 200) {
    char buffer[256];
    while (true) {
      const int n = esp_http_client_read(client, buffer, sizeof(buffer));
      if (n == 0)
        break;
      if (n < 0) {
        err = read_failure(n);
        break;
      }
      if (job.response.size() + n > MAX_RESPONSE_BYTES) {
        job.too_large = true;
        break;
      }
      job.response.append(buffer, n);
    }
    if (err == ESP_OK && !job.too_large && !esp_http_client_is_complete_data_received(client))
      err = ESP_ERR_INVALID_SIZE;
  }
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  return err;
}

static void pairing_task(void *param) {
  auto *job = static_cast<PairingJob *>(param);
  job->error = run_exchange(*job);
  job->done.store(true, std::memory_order_release);
  vTaskDelete(nullptr);
}

// --- State machine (main loop) -------------------------------------------------

void CloudPairing::setup() {
  this->set_plant_("");
  this->set_status_(this->available() ? "idle" : "unavailable");
}

void CloudPairing::loop() {
  if (this->job_ != nullptr && this->job_->done.load(std::memory_order_acquire))
    this->finish_job_();
  if (this->has_result_ && millis() - this->result_at_ >= RESULT_TTL_MS) {
    ESP_LOGW(TAG, "Plant settings from GbbOptimizer were not confirmed within %" PRIu32 " min; discarded",
             RESULT_TTL_MS / 60000);
    this->drop_result_();
    this->set_status_("expired");
  }
}

void CloudPairing::start(const std::string &input) {
  // The code never goes to the log: until it is used, it is worth the
  // plant's token.
  if (!this->available()) {
    ESP_LOGW(TAG, "Pairing is not available in this build (no CA certificate to verify GbbOptimizer with)");
    this->set_status_("unavailable");
    return;
  }
  if (this->job_ != nullptr && !this->discard_job_) {
    ESP_LOGW(TAG, "A pairing request is already running; the new code is ignored");
    return;
  }
  this->drop_result_();
  this->queued_code_.clear();
  std::string code;
  if (!normalize_pairing_code(input, code)) {
    ESP_LOGW(TAG, "That is not a GbbOptimizer pairing code (expected e.g. X-XXXX-XXXX)");
    this->set_status_("invalid_format");
    return;
  }
  if (!network::is_connected()) {
    ESP_LOGW(TAG, "Pairing needs a network connection");
    this->set_status_("network_error");
    return;
  }
  if (this->job_ != nullptr) {
    // A cancelled request is still on its way; this code goes out after it.
    this->queued_code_ = code;
    this->set_status_("working");
    return;
  }
  this->launch_(code);
}

void CloudPairing::launch_(const std::string &code) {
  auto *job = new PairingJob();  // NOLINT(cppcoreguidelines-owning-memory)
  job->url = this->url_;
  // normalize_pairing_code() leaves only A-Z, 0-9 and '-': nothing to escape.
  job->body = "{\"code\":\"" + code + "\"}";
  job->user_agent = this->user_agent_;
  job->ca_pem = this->ca_certificate_;
  if (xTaskCreate(pairing_task, "gbb_pairing", TASK_STACK_BYTES, job, TASK_PRIORITY, nullptr) != pdPASS) {
    delete job;  // NOLINT(cppcoreguidelines-owning-memory)
    ESP_LOGE(TAG, "Could not start the pairing request (out of memory)");
    this->set_status_("network_error");
    return;
  }
  this->job_ = job;
  this->discard_job_ = false;
  ESP_LOGI(TAG, "Asking GbbOptimizer for the plant's cloud settings");
  this->set_status_("working");
}

void CloudPairing::finish_job_() {
  std::unique_ptr<PairingJob> job(this->job_);
  this->job_ = nullptr;
  if (this->discard_job_) {
    this->discard_job_ = false;
    ESP_LOGD(TAG, "Answer to a cancelled pairing request ignored");
    if (!this->queued_code_.empty()) {
      const std::string code = std::move(this->queued_code_);
      this->queued_code_.clear();
      this->launch_(code);
    }
    return;
  }
  if (job->error != ESP_OK) {
    ESP_LOGW(TAG, "Pairing request failed: %s%s", esp_err_to_name(job->error),
             job->error == ESP_ERR_HTTP_CONNECT ? " (no internet access, DNS, or certificate not trusted)" : "");
    this->set_status_("network_error");
    return;
  }
  switch (job->status) {
    case 200:
      break;
    case 404:
      ESP_LOGW(TAG, "GbbOptimizer did not accept the pairing code (unknown, already used or expired)");
      this->set_status_("invalid_code");
      return;
    case 429:
      ESP_LOGW(TAG, "GbbOptimizer refused the pairing request: too many attempts, try again later");
      this->set_status_("rate_limited");
      return;
    default:
      ESP_LOGW(TAG, "Pairing failed: GbbOptimizer answered HTTP %d", job->status);
      this->set_status_("server_error");
      return;
  }
  if (job->too_large) {
    ESP_LOGW(TAG, "Pairing answer rejected: larger than %u B", (unsigned) MAX_RESPONSE_BYTES);
    this->set_status_("bad_response");
    return;
  }
  PairingResult result;
  const char *reason = parse_pairing_response(job->response, this->limits_, result);
  if (reason != nullptr) {
    ESP_LOGW(TAG, "Pairing answer rejected: %s", reason);
    this->set_status_("bad_response");
    return;
  }
  ESP_LOGI(TAG, "GbbOptimizer returned plant %s on %s:%u; waiting for confirmation", result.plant_id.c_str(),
           result.mqtt_server.c_str(), result.mqtt_port);
  this->result_ = std::move(result);
  this->has_result_ = true;
  this->result_at_ = millis();
  this->set_plant_(describe_pairing_result(this->result_));
  this->set_status_("confirm");
}

bool CloudPairing::take_result(PairingResult &out) {
  if (!this->has_result_)
    return false;
  out = std::move(this->result_);
  this->result_ = PairingResult{};
  this->has_result_ = false;
  this->set_status_("applied");
  return true;
}

void CloudPairing::cancel() {
  if (this->job_ != nullptr && !this->discard_job_) {
    this->discard_job_ = true;
    ESP_LOGI(TAG, "Pairing cancelled; the answer to the running request will be ignored");
  } else if (this->has_result_) {
    ESP_LOGI(TAG, "Pairing cancelled; the plant settings were not saved");
  }
  this->queued_code_.clear();
  this->drop_result_();
  this->set_status_(this->available() ? "idle" : "unavailable");
}

void CloudPairing::drop_result_() {
  this->has_result_ = false;
  this->result_ = PairingResult{};
  this->set_plant_("");
}

void CloudPairing::set_status_(const char *status) {
  // Published even when unchanged, so a repeated refusal still reaches the
  // panel as an event.
  if (this->status_sensor_ != nullptr)
    this->status_sensor_->publish_state(status);
}

void CloudPairing::set_plant_(const std::string &plant) {
  if (this->plant_sensor_ == nullptr)
    return;
  if (this->plant_sensor_->has_state() && this->plant_sensor_->state == plant)
    return;
  this->plant_sensor_->publish_state(plant);
}

}  // namespace gbb_dongle
}  // namespace esphome
