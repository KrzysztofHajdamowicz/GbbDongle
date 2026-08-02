package main

import (
	"bytes"
	"context"
	"fmt"
	"io"
	"log"
	"log/slog"
	"strings"
	"sync"

	mqtt "github.com/mochi-mqtt/server/v2"
	"github.com/mochi-mqtt/server/v2/hooks/auth"
	"github.com/mochi-mqtt/server/v2/listeners"
	"github.com/mochi-mqtt/server/v2/packets"
)

const toDeviceSuffix = "/ModbusInMqtt/toDevice"

// tracker follows the dongle we lock onto: which client, which PlantId, and
// whether its toDevice subscription is currently live. waitReady blocks until
// the dongle is subscribed (again), so the scan survives a mid-scan restart.
type tracker struct {
	mu       sync.Mutex
	plantID  string
	clientID string
	ready    chan struct{} // closed while the dongle is subscribed
	verbose  bool
}

func newTracker(verbose bool) *tracker {
	return &tracker{ready: make(chan struct{}), verbose: verbose}
}

func (t *tracker) subscribed(clientID, plantID, remote string) {
	t.mu.Lock()
	defer t.mu.Unlock()
	switch {
	case t.clientID == "":
		t.clientID, t.plantID = clientID, plantID
		log.Printf("dongle connected: client %q (Plant Id %q) from %s", clientID, plantID, remote)
		close(t.ready)
	case clientID == t.clientID:
		select {
		case <-t.ready:
		default:
			log.Printf("dongle re-subscribed: client %q", clientID)
			close(t.ready)
		}
	case plantID == t.plantID:
		log.Printf("WARNING: second client %q subscribed to the same Plant Id %q — "+
			"two devices would execute the same frames; results may be unreliable", clientID, plantID)
	default:
		log.Printf("ignoring additional client %q (Plant Id %q); already locked on %q", clientID, plantID, t.clientID)
	}
}

func (t *tracker) disconnected(clientID string) {
	t.mu.Lock()
	defer t.mu.Unlock()
	if clientID != t.clientID {
		return
	}
	select {
	case <-t.ready:
		log.Printf("dongle disconnected; waiting for it to come back")
		t.ready = make(chan struct{})
	default:
	}
}

// waitReady blocks until the locked dongle (or the first dongle ever) has a
// live toDevice subscription, and returns its PlantId.
func (t *tracker) waitReady(ctx context.Context) (string, error) {
	for {
		t.mu.Lock()
		ready := t.ready
		plantID := t.plantID
		t.mu.Unlock()
		select {
		case <-ready:
			if plantID == "" {
				// Locked between ready-close and plantID set is impossible
				// (both happen under the lock), so this is just defensive.
				continue
			}
			return plantID, nil
		case <-ctx.Done():
			return "", ctx.Err()
		}
	}
}

// gbbHook feeds connection lifecycle events into the tracker.
type gbbHook struct {
	mqtt.HookBase
	tracker *tracker
}

func (h *gbbHook) ID() string { return "busscan" }

func (h *gbbHook) Provides(b byte) bool {
	return bytes.Contains([]byte{
		mqtt.OnSessionEstablished,
		mqtt.OnSubscribed,
		mqtt.OnDisconnect,
	}, []byte{b})
}

func (h *gbbHook) OnSessionEstablished(cl *mqtt.Client, pk packets.Packet) {
	if h.tracker.verbose {
		log.Printf("client connected: id %q, username %q, from %s",
			cl.ID, string(cl.Properties.Username), cl.Net.Remote)
	}
}

func (h *gbbHook) OnSubscribed(cl *mqtt.Client, pk packets.Packet, reasonCodes []byte) {
	for _, f := range pk.Filters {
		if strings.HasSuffix(f.Filter, toDeviceSuffix) {
			plantID := strings.TrimSuffix(f.Filter, toDeviceSuffix)
			h.tracker.subscribed(cl.ID, plantID, cl.Net.Remote)
		}
	}
}

func (h *gbbHook) OnDisconnect(cl *mqtt.Client, err error, expire bool) {
	h.tracker.disconnected(cl.ID)
}

// broker wraps the embedded mochi-mqtt server.
type broker struct {
	server    *mqtt.Server
	tracker   *tracker
	responses chan []byte
}

func startBroker(port int, verbose bool) (*broker, error) {
	t := newTracker(verbose)
	server := mqtt.New(&mqtt.Options{InlineClient: true})
	if !verbose {
		server.Log = discardLogger()
	}
	if err := server.AddHook(new(auth.AllowHook), nil); err != nil {
		return nil, err
	}
	if err := server.AddHook(&gbbHook{tracker: t}, nil); err != nil {
		return nil, err
	}
	if err := server.AddListener(listeners.NewTCP(listeners.Config{
		ID:      "tcp",
		Address: fmt.Sprintf(":%d", port),
	})); err != nil {
		return nil, fmt.Errorf("cannot listen on port %d (already in use?): %w", port, err)
	}

	b := &broker{server: server, tracker: t, responses: make(chan []byte, 8)}
	err := server.Subscribe("+/ModbusInMqtt/fromDevice", 1,
		func(cl *mqtt.Client, sub packets.Subscription, pk packets.Packet) {
			payload := append([]byte(nil), pk.Payload...)
			select {
			case b.responses <- payload:
			default:
				log.Printf("dropping unexpected fromDevice payload (%d bytes)", len(pk.Payload))
			}
		})
	if err != nil {
		return nil, err
	}
	if verbose {
		err = server.Subscribe("+/keepalive", 1,
			func(cl *mqtt.Client, sub packets.Subscription, pk packets.Packet) {
				log.Printf("keepalive on %s", pk.TopicName)
			})
		if err != nil {
			return nil, err
		}
	}

	go func() {
		if err := server.Serve(); err != nil {
			log.Fatalf("broker failed: %v", err)
		}
	}()
	return b, nil
}

func (b *broker) publishRequest(plantID string, payload []byte) error {
	return b.server.Publish(plantID+toDeviceSuffix, payload, false, 1)
}

func (b *broker) close() {
	_ = b.server.Close()
}

func discardLogger() *slog.Logger {
	return slog.New(slog.NewTextHandler(io.Discard, nil))
}
