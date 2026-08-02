package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strconv"
	"time"
)

// Entity names of the GbbDongle config entities (firmware/common/base.yaml).
// ESPHome's web_server matches REST URLs against the entity *name* (see
// UrlMatch::match_entity in web_server.cpp), so paths carry the URL-escaped
// name, not the sanitized object_id.
const (
	entMQTTServer = "MQTT Server"
	entMQTTPort   = "MQTT Port"
	entTLS        = "TLS"
	entCloud      = "Cloud Connection"
	entApplyBtn   = "Apply Settings (Restart)"
)

// dongleConfig holds the MQTT-related entity values we touch.
type dongleConfig struct {
	Server string
	Port   int
	TLS    bool
	Cloud  bool
}

func (c dongleConfig) String() string {
	return fmt.Sprintf("MQTT Server=%q, MQTT Port=%d, TLS=%v, Cloud Connection=%v",
		c.Server, c.Port, c.TLS, c.Cloud)
}

type dongleClient struct {
	base string // e.g. http://gbbdongle.local
	http *http.Client
}

func newDongleClient(host string) *dongleClient {
	return &dongleClient{
		base: "http://" + host,
		http: &http.Client{
			Timeout: 5 * time.Second,
			// The dongle reboots between applyConfig and the final restore; a
			// pooled keep-alive connection would be dead by then and Go does
			// not retry non-idempotent requests on it ("connection reset by
			// peer"). One fresh connection per request avoids that entirely.
			Transport: &http.Transport{DisableKeepAlives: true},
		},
	}
}

func (d *dongleClient) get(ctx context.Context, path string, out any) error {
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, d.base+path, nil)
	if err != nil {
		return err
	}
	resp, err := d.http.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("GET %s: HTTP %d", path, resp.StatusCode)
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	return json.Unmarshal(body, out)
}

// post retries transient transport errors: every request here is safe to
// repeat (set/turn_on/turn_off are idempotent, a second restart press while
// already restarting is a no-op) and restore must survive the dongle being
// mid-reboot.
func (d *dongleClient) post(ctx context.Context, path string) error {
	var lastErr error
	for attempt := 0; attempt < 3; attempt++ {
		if attempt > 0 {
			select {
			case <-time.After(time.Second):
			case <-ctx.Done():
				return ctx.Err()
			}
		}
		req, err := http.NewRequestWithContext(ctx, http.MethodPost, d.base+path, http.NoBody)
		if err != nil {
			return err
		}
		resp, err := d.http.Do(req)
		if err != nil {
			lastErr = err
			continue
		}
		resp.Body.Close()
		if resp.StatusCode != http.StatusOK {
			return fmt.Errorf("POST %s: HTTP %d", path, resp.StatusCode)
		}
		return nil
	}
	return fmt.Errorf("POST %s: %w", path, lastErr)
}

// fetchConfig reads the current MQTT-related entity values. The firmware
// serializes the number entity's "value" as a JSON string ("8883"), so port
// parsing accepts both forms.
func (d *dongleClient) fetchConfig(ctx context.Context) (dongleConfig, error) {
	var cfg dongleConfig
	var text struct {
		Value string `json:"value"`
	}
	if err := d.get(ctx, "/text/"+url.PathEscape(entMQTTServer), &text); err != nil {
		return cfg, fmt.Errorf("reading MQTT Server entity (is this a GbbDongle?): %w", err)
	}
	cfg.Server = text.Value
	var num struct {
		Value json.Number `json:"value"`
	}
	if err := d.get(ctx, "/number/"+url.PathEscape(entMQTTPort), &num); err != nil {
		return cfg, err
	}
	port, err := num.Value.Float64()
	if err != nil {
		return cfg, fmt.Errorf("unexpected MQTT Port value %q", num.Value)
	}
	cfg.Port = int(port)
	var sw struct {
		Value bool `json:"value"`
	}
	if err := d.get(ctx, "/switch/"+url.PathEscape(entTLS), &sw); err != nil {
		return cfg, err
	}
	cfg.TLS = sw.Value
	if err := d.get(ctx, "/switch/"+url.PathEscape(entCloud), &sw); err != nil {
		return cfg, err
	}
	cfg.Cloud = sw.Value
	return cfg, nil
}

// applyConfig writes the given values and presses "Apply Settings (Restart)",
// which persists them and restarts the dongle.
func (d *dongleClient) applyConfig(ctx context.Context, cfg dongleConfig) error {
	steps := []string{
		"/text/" + url.PathEscape(entMQTTServer) + "/set?value=" + url.QueryEscape(cfg.Server),
		"/number/" + url.PathEscape(entMQTTPort) + "/set?value=" + strconv.Itoa(cfg.Port),
		"/switch/" + url.PathEscape(entTLS) + "/turn_" + onOff(cfg.TLS),
		"/switch/" + url.PathEscape(entCloud) + "/turn_" + onOff(cfg.Cloud),
		"/button/" + url.PathEscape(entApplyBtn) + "/press",
	}
	for _, p := range steps {
		if err := d.post(ctx, p); err != nil {
			return err
		}
	}
	return nil
}

func onOff(v bool) string {
	if v {
		return "on"
	}
	return "off"
}
