// Host test for components/gbb_dongle/cloud_pairing.cpp. The ESP-IDF HTTP
// client is faked below and answers what each case sets in the g_* variables;
// the FreeRTOS task runs on a detached std::thread. Nothing here talks to
// GbbOptimizer: redeeming a real code would rotate a live plant's token.
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <esp_http_client.h>
#include <freertos/task.h>

#include "cloud_pairing.h"

namespace esphome {
uint32_t fake_millis = 0;
namespace network {
bool fake_connected = true;
}  // namespace network
}  // namespace esphome

// ---- Fake HTTP client and task ------------------------------------------------

struct fake_client {
  esp_http_client_config_t config;
  std::string url, user_agent;  // copied at init, as the real client does
  std::vector<std::pair<std::string, std::string>> headers;
  std::string written;
  size_t read_pos{0};
};

// The answer to the next exchange. Changed only while no exchange runs.
static esp_err_t g_open_err = ESP_OK;
static int64_t g_fetch_ret = -2;  // -2: the body's length
static int g_status = 200;
static std::string g_body;
static bool g_complete = true;
static int g_delay_ms = 0;  // spent in open(), to cancel a running exchange

// What the exchanges sent.
static fake_client g_last;  // the last client, copied at cleanup
static std::atomic<int> g_requests{0};

const char *esp_err_to_name(esp_err_t e) {
  switch (e) {
    case ESP_OK:
      return "ESP_OK";
    case ESP_FAIL:
      return "ESP_FAIL";
    case ESP_ERR_NO_MEM:
      return "ESP_ERR_NO_MEM";
    case ESP_ERR_TIMEOUT:
      return "ESP_ERR_TIMEOUT";
    case ESP_ERR_INVALID_SIZE:
      return "ESP_ERR_INVALID_SIZE";
    case ESP_ERR_HTTP_CONNECT:
      return "ESP_ERR_HTTP_CONNECT";
    default:
      return "?";
  }
}

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config) {
  auto *c = new fake_client();
  c->config = *config;
  c->url = config->url;
  c->user_agent = config->user_agent;
  return c;
}

esp_err_t esp_http_client_set_header(esp_http_client_handle_t c, const char *key, const char *value) {
  c->headers.emplace_back(key, value);
  return ESP_OK;
}

esp_err_t esp_http_client_open(esp_http_client_handle_t, int) {
  g_requests++;
  if (g_delay_ms > 0)
    std::this_thread::sleep_for(std::chrono::milliseconds(g_delay_ms));
  return g_open_err;
}

int esp_http_client_write(esp_http_client_handle_t c, const char *buffer, int len) {
  const int n = std::min(len, 5);  // partial writes, as over a slow link
  c->written.append(buffer, n);
  return n;
}

int64_t esp_http_client_fetch_headers(esp_http_client_handle_t) {
  return g_fetch_ret == -2 ? (int64_t) g_body.size() : g_fetch_ret;
}

int esp_http_client_get_status_code(esp_http_client_handle_t) { return g_status; }

int esp_http_client_read(esp_http_client_handle_t c, char *buffer, int len) {
  if (c->read_pos >= g_body.size())
    return 0;
  const int n = std::min({len, 7, (int) (g_body.size() - c->read_pos)});  // short reads
  memcpy(buffer, g_body.data() + c->read_pos, n);
  c->read_pos += n;
  return n;
}

bool esp_http_client_is_complete_data_received(esp_http_client_handle_t c) {
  return g_complete && c->read_pos == g_body.size();
}

esp_err_t esp_http_client_close(esp_http_client_handle_t) { return ESP_OK; }

esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c) {
  g_last = *c;
  delete c;
  return ESP_OK;
}

BaseType_t xTaskCreate(TaskFunction_t fn, const char *, uint32_t, void *param, UBaseType_t, TaskHandle_t *) {
  std::thread(fn, param).detach();
  return pdPASS;
}

void vTaskDelete(TaskHandle_t) {}

// ---- Helpers ------------------------------------------------------------------

using namespace esphome;
using namespace esphome::gbb_dongle;

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

static std::string norm(const std::string &in) {
  std::string out;
  return normalize_pairing_code(in, out) ? out : "<reject>";
}

// Tells whether an exchange is in flight.
class Pairing : public CloudPairing {
 public:
  bool in_flight() const { return this->job_ != nullptr; }
};

// Runs the main loop until no exchange is in flight (a code queued behind a
// cancelled exchange goes out and returns too). False after 5 s.
static bool settle(Pairing &p) {
  for (int i = 0; i < 1000 && p.in_flight(); i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    p.loop();
  }
  return !p.in_flight();
}

static const char *const CA = "-----BEGIN CERTIFICATE-----\nX\n-----END CERTIFICATE-----\n";
static const std::string GOOD = R"({"plantId":"G12345","plantName":"Dom","plantToken":"ab+c/d=",)"
                                R"("mqttServer":"gbboptimizer7-mqtt.gbbsoft.pl","mqttPort":8883})";

// GOOD with the first occurrence of `from` replaced by `to`.
static std::string good_with(const std::string &from, const std::string &to) {
  std::string body = GOOD;
  const size_t at = body.find(from);
  assert(at != std::string::npos);
  return body.replace(at, from.size(), to);
}

// The reason parse_pairing_response() refuses `body` for, or "ok".
static std::string refusal(const std::string &body, const PairingLimits &limits = PairingLimits{}) {
  PairingResult result;
  const char *why = parse_pairing_response(body, limits, result);
  return why != nullptr ? why : "ok";
}

// ---- Code normalization -------------------------------------------------------

static void test_normalization() {
  CHECK(norm("J-EEH7-ABCD") == "J-EEH7-ABCD");
  CHECK(norm("j-eeh7-abcd") == "J-EEH7-ABCD");
  CHECK(norm(" J EEH7 ABCD \n") == "J-EEH7-ABCD");
  CHECK(norm("JEEH7ABCD") == "J-EEH7-ABCD");
  CHECK(norm("JEEH-7ABCD") == "J-EEH7-ABCD");
  CHECK(norm("J\xE2\x80\x93" "EEH7\xE2\x80\x94" "ABCD") == "J-EEH7-ABCD");  // en and em dash
  CHECK(norm("J\xE2\x88\x92" "EEH7-ABCD") == "J-EEH7-ABCD");              // minus sign
  CHECK(norm("J\xC2\xA0" "EEH7\xC2\xA0" "ABCD") == "J-EEH7-ABCD");          // no-break space

  // A prefix of one or two letters, or none.
  CHECK(norm("AB-EEH7-ABCD") == "AB-EEH7-ABCD");
  CHECK(norm("ab eeh7 abcd") == "AB-EEH7-ABCD");
  CHECK(norm("aaeeh7abcd") == "AA-EEH7-ABCD");
  CHECK(norm("AAE-EH7A-BCD") == "AA-EEH7-ABCD");
  CHECK(norm("eeh7abcd") == "EEH7-ABCD");

  // Other lengths keep the dashes they were typed with.
  CHECK(norm("abc-eeh7-abcd") == "ABC-EEH7-ABCD");
  CHECK(norm("eeh7-abc") == "EEH7-ABC");
  CHECK(norm(std::string(32, 'A')) == std::string(32, 'A'));

  // Not a code: refused before any request.
  CHECK(norm("") == "<reject>");
  CHECK(norm("ABC") == "<reject>");
  CHECK(norm("- - -") == "<reject>");
  CHECK(norm("J-EEH7-ABC!") == "<reject>");
  CHECK(norm("J-EEH7-ABC\xC4\x84") == "<reject>");  // Ą
  CHECK(norm("\"},{\"x") == "<reject>");
  CHECK(norm(std::string(33, 'A')) == "<reject>");
}

// ---- Validation of the 200 answer -------------------------------------------

static void test_answer() {
  PairingResult r;
  CHECK(parse_pairing_response(GOOD, PairingLimits{}, r) == nullptr);
  CHECK(r.plant_id == "G12345" && r.plant_name == "Dom" && r.plant_token == "ab+c/d=" &&
        r.mqtt_server == "gbboptimizer7-mqtt.gbbsoft.pl" && r.mqtt_port == 8883);
  CHECK(describe_pairing_result(r) == "Dom · G12345 · gbboptimizer7-mqtt.gbbsoft.pl");

  CHECK(refusal("") == "answer is not a JSON object");
  CHECK(refusal("[1,2]") == "answer is not a JSON object");
  CHECK(refusal("{\"error\":\"invalid_code\"}") == "plantId missing");

  // Plant Id: a string, topic-safe, within the entity's max_length.
  CHECK(refusal(good_with("\"G12345\"", "12345")) == "plantId missing");
  CHECK(refusal(good_with("G12345", "G1/#")) == "plantId has characters not allowed in an MQTT topic");
  CHECK(refusal(good_with("G12345", "")) == "plantId empty or longer than the Plant Id entity accepts");
  CHECK(refusal(good_with("G12345", std::string(65, 'A'))) ==
        "plantId empty or longer than the Plant Id entity accepts");
  CHECK(refusal(good_with("G12345", std::string(64, 'A'))) == "ok");

  // Plant Token: printable ASCII without spaces.
  CHECK(refusal(good_with("ab+c/d=", "a b")) == "plantToken has characters other than printable ASCII");
  CHECK(refusal(good_with("ab+c/d=", std::string(65, 'x'))) ==
        "plantToken empty or longer than the Plant Token entity accepts");
  CHECK(refusal(good_with("\"plantToken\":\"ab+c/d=\",", "")) == "plantToken missing");

  // MQTT Server: a host name under .gbbsoft.pl, stored lower-case.
  const std::string host = "gbboptimizer7-mqtt.gbbsoft.pl";
  {
    PairingResult x;
    CHECK(parse_pairing_response(good_with(host, "GbbOptimizer7-MQTT.GbbSoft.PL"), PairingLimits{}, x) == nullptr);
    CHECK(x.mqtt_server == host);
  }
  const char *const foreign = "mqttServer is not a GbbOptimizer broker (*.gbbsoft.pl)";
  const char *const not_host = "mqttServer is not a host name";
  CHECK(refusal(good_with(host, "evil.example.com")) == foreign);
  CHECK(refusal(good_with(host, "gbbsoft.pl.evil.com")) == foreign);
  CHECK(refusal(good_with(host, "xgbbsoft.pl")) == foreign);
  CHECK(refusal(good_with(host, "gbbsoft.pl")) == foreign);
  CHECK(refusal(good_with(host, ".gbbsoft.pl")) == not_host);
  CHECK(refusal(good_with(host, "a..gbbsoft.pl")) == not_host);
  CHECK(refusal(good_with(host, "-a.gbbsoft.pl")) == not_host);
  CHECK(refusal(good_with(host, "a-.gbbsoft.pl")) == not_host);
  CHECK(refusal(good_with(host, "a_b.gbbsoft.pl")) == not_host);
  CHECK(refusal(good_with(host, host + ".")) == not_host);
  CHECK(refusal(good_with(host, "host.gbbsoft.pl:8883")) == not_host);
  CHECK(refusal(good_with(host, "mqtts://host.gbbsoft.pl")) == not_host);
  CHECK(refusal(good_with(host, std::string(64, 'a') + ".gbbsoft.pl")) == not_host);
  CHECK(refusal(good_with(host, std::string(63, 'a') + ".gbbsoft.pl")) == "ok");

  // MQTT Port: a whole number 1..65535.
  CHECK(refusal(good_with("8883", "\"8883\"")) == "mqttPort missing or not a whole number");
  CHECK(refusal(good_with("8883", "8883.5")) == "mqttPort missing or not a whole number");
  CHECK(refusal(good_with("8883", "0")) == "mqttPort out of range");
  CHECK(refusal(good_with("8883", "65536")) == "mqttPort out of range");

  // Plant name: optional, display only, cleaned and cut on a UTF-8 boundary.
  {
    PairingResult x;
    CHECK(parse_pairing_response(good_with("\"plantName\":\"Dom\",", ""), PairingLimits{}, x) == nullptr);
    CHECK(describe_pairing_result(x) == "G12345 · gbboptimizer7-mqtt.gbbsoft.pl");
    parse_pairing_response(good_with("\"Dom\"", "\"  Dom\\u0007 \\u202eLato\\u0085 \""), PairingLimits{}, x);
    CHECK(x.plant_name == "Dom Lato");
    parse_pairing_response(good_with("\"Dom\"", "\"Łąka " + std::string(70, 'z') + "\""), PairingLimits{}, x);
    CHECK(x.plant_name.size() <= PAIRING_PLANT_NAME_MAX && x.plant_name.rfind("Łąka ", 0) == 0);
    parse_pairing_response(good_with("\"Dom\"", "\"" + std::string(63, 'z') + "Ł\""), PairingLimits{}, x);
    CHECK(x.plant_name == std::string(63, 'z'));  // the 2-byte letter does not fit the last byte
    parse_pairing_response(good_with("\"Dom\"", "null"), PairingLimits{}, x);
    CHECK(x.plant_name.empty());
  }

  // The limits follow the entities' max_length.
  PairingLimits small;
  small.plant_token = 4;
  CHECK(refusal(GOOD, small) != "ok");
}

// ---- State machine ------------------------------------------------------------

static void test_state_machine() {
  text_sensor::TextSensor status, plant;
  Pairing p;
  p.set_status_text_sensor(&status);
  p.set_plant_text_sensor(&plant);

  // Without a CA nothing goes out.
  p.setup();
  CHECK(status.state == "unavailable");
  p.start("J-EEH7-ABCD");
  CHECK(status.state == "unavailable" && g_requests == 0);

  p.set_ca_certificate(CA);
  p.set_user_agent("GbbDongle/2.2.0");
  p.setup();
  CHECK(status.state == "idle" && plant.state.empty());

  // Not a code, or no network: refused without a request.
  p.start("hello!");
  CHECK(status.state == "invalid_format" && g_requests == 0);
  network::fake_connected = false;
  p.start("J-EEH7-ABCD");
  CHECK(status.state == "network_error" && g_requests == 0);
  network::fake_connected = true;

  // 404: what goes out, and how the refusal comes back.
  g_status = 404;
  g_body = "{\"error\":\"invalid_code\"}";
  p.start("j eeh7 abcd");
  CHECK(status.state == "working");
  CHECK(settle(p));
  CHECK(status.state == "invalid_code");
  CHECK(g_last.written == "{\"code\":\"J-EEH7-ABCD\"}");
  CHECK(g_last.url == DEFAULT_PAIRING_URL);
  CHECK(g_last.config.method == HTTP_METHOD_POST && g_last.config.disable_auto_redirect &&
        g_last.config.cert_pem == CA);
  CHECK(g_last.user_agent == "GbbDongle/2.2.0");
  CHECK(g_last.headers.size() == 2 && g_last.headers[0].first == "Content-Type" &&
        g_last.headers[0].second == "application/json");
  CHECK(g_last.read_pos == 0);  // a refusal's body is not read

  // The same refusal again is published again, so the panel sees a new event.
  const size_t published = status.history.size();
  p.start("ab eeh7 abcd");
  CHECK(settle(p));
  CHECK(status.state == "invalid_code" && status.history.size() == published + 2);
  CHECK(g_last.written == "{\"code\":\"AB-EEH7-ABCD\"}");

  // 429, other statuses (redirects are not followed).
  g_status = 429;
  p.start("J-EEH7-ABCD");
  CHECK(settle(p));
  CHECK(status.state == "rate_limited");
  g_status = 502;
  p.start("J-EEH7-ABCD");
  CHECK(settle(p));
  CHECK(status.state == "server_error");
  g_status = 301;
  p.start("J-EEH7-ABCD");
  CHECK(settle(p));
  CHECK(status.state == "server_error");

  // Connection failure and timeout.
  g_status = 200;
  g_open_err = ESP_ERR_HTTP_CONNECT;
  p.start("J-EEH7-ABCD");
  CHECK(settle(p));
  CHECK(status.state == "network_error");
  g_open_err = ESP_OK;
  g_fetch_ret = -ESP_ERR_HTTP_EAGAIN;
  p.start("J-EEH7-ABCD");
  CHECK(settle(p));
  CHECK(status.state == "network_error");
  g_fetch_ret = -2;

  // 200 that is cut short, too large, or fails validation.
  g_body = GOOD;
  g_complete = false;
  p.start("J-EEH7-ABCD");
  CHECK(settle(p));
  CHECK(status.state == "network_error");
  g_complete = true;
  g_body = std::string(3000, ' ') + GOOD;
  p.start("J-EEH7-ABCD");
  CHECK(settle(p));
  CHECK(status.state == "bad_response");
  g_body = good_with("gbboptimizer7-mqtt.gbbsoft.pl", "broker.example.com");
  p.start("J-EEH7-ABCD");
  CHECK(settle(p));
  CHECK(status.state == "bad_response" && plant.state.empty());

  // Success, then confirm.
  g_body = GOOD;
  p.start("J-EEH7-ABCD");
  CHECK(settle(p));
  CHECK(status.state == "confirm");
  CHECK(plant.state == "Dom · G12345 · gbboptimizer7-mqtt.gbbsoft.pl");
  PairingResult got;
  CHECK(p.take_result(got));
  CHECK(got.plant_token == "ab+c/d=" && got.mqtt_port == 8883);
  CHECK(status.state == "applied");
  CHECK(!p.take_result(got));

  // Success, then cancel.
  p.start("J-EEH7-ABCD");
  CHECK(settle(p));
  CHECK(status.state == "confirm");
  p.cancel();
  CHECK(status.state == "idle" && plant.state.empty() && !p.take_result(got));

  // Success, then nobody confirms for 10 minutes.
  fake_millis = 1000;
  p.start("J-EEH7-ABCD");
  CHECK(settle(p));
  CHECK(status.state == "confirm");
  fake_millis += 10 * 60 * 1000 - 1;
  p.loop();
  CHECK(status.state == "confirm");
  fake_millis += 1;
  p.loop();
  CHECK(status.state == "expired" && plant.state.empty() && !p.take_result(got));

  // A new code replaces a waiting result; one that is not a code drops it.
  p.start("J-EEH7-ABCD");
  CHECK(settle(p));
  CHECK(status.state == "confirm");
  p.start("bad!");
  CHECK(status.state == "invalid_format" && plant.state.empty() && !p.take_result(got));

  // While an exchange runs, another code is ignored.
  g_delay_ms = 100;
  int before = g_requests;
  p.start("J-EEH7-ABCD");
  p.start("K-EEH7-ABCD");
  CHECK(status.state == "working");
  CHECK(settle(p));
  CHECK(status.state == "confirm" && g_requests == before + 1);
  CHECK(g_last.written == "{\"code\":\"J-EEH7-ABCD\"}");
  p.cancel();

  // Cancelled while running: the answer is ignored.
  before = g_requests;
  p.start("J-EEH7-ABCD");
  p.cancel();
  CHECK(status.state == "idle");
  CHECK(settle(p));
  CHECK(status.state == "idle" && plant.state.empty() && !p.take_result(got));
  CHECK(g_requests == before + 1);

  // Cancelled while running, then a new code: it goes out once the first
  // exchange returns.
  before = g_requests;
  p.start("J-EEH7-ABCD");
  p.cancel();
  p.start("k-eeh7-wxyz");
  CHECK(status.state == "working");
  CHECK(settle(p));
  CHECK(status.state == "confirm");
  CHECK(g_last.written == "{\"code\":\"K-EEH7-WXYZ\"}");
  CHECK(g_requests == before + 2);
  g_delay_ms = 0;
}

int main() {
  test_normalization();
  test_answer();
  test_state_machine();
  std::printf("%s (%d failure(s))\n", failures ? "FAILED" : "OK", failures);
  return failures ? 1 : 0;
}
