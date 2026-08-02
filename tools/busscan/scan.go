package main

import (
	"context"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"log"
	"sync/atomic"
	"time"
)

// header mirrors the GbbOptimizer cloud protocol payload (docs/protocol.md).
type header struct {
	OrderId     string `json:"OrderId"`
	SendLastLog int    `json:"SendLastLog"`
	Lines       []line `json:"Lines"`
}

type line struct {
	LineNo    int    `json:"LineNo"`
	Tag       string `json:"Tag,omitempty"`
	Timestamp int64  `json:"Timestamp"`
	Modbus    string `json:"Modbus,omitempty"`
	Error     string `json:"Error,omitempty"`
}

var orderCounter uint64

// errDongleTimeout means the dongle reported "Response timeout" for the frame:
// nothing answered on the RS485 bus at that address.
type dongleError struct{ msg string }

func (e dongleError) Error() string { return e.msg }

type scanner struct {
	broker  *broker
	timeout time.Duration
	wait    time.Duration
	verbose bool
}

// exchange sends one Modbus frame through the dongle and returns the raw
// response frame. A dongleError return means the dongle executed the request
// but the bus transaction failed (timeout, bad response CRC, ...).
func (s *scanner) exchange(ctx context.Context, frameHex string) ([]byte, error) {
	// Block until the dongle has a live toDevice subscription; survives
	// mid-scan restarts.
	readyCtx, cancel := context.WithTimeout(ctx, s.wait)
	plantID, err := s.broker.tracker.waitReady(readyCtx)
	cancel()
	if err != nil {
		return nil, fmt.Errorf("dongle not connected: %w", err)
	}

	// Drain stale responses from an earlier attempt.
	for {
		select {
		case <-s.broker.responses:
			continue
		default:
		}
		break
	}

	orderID := fmt.Sprintf("busscan-%d", atomic.AddUint64(&orderCounter, 1))
	payload, err := json.Marshal(header{
		OrderId:     orderID,
		SendLastLog: 0,
		Lines: []line{{
			LineNo:    0,
			Tag:       "busscan",
			Timestamp: time.Now().Unix(),
			Modbus:    frameHex,
		}},
	})
	if err != nil {
		return nil, err
	}
	if s.verbose {
		log.Printf("-> %s: %s", plantID+toDeviceSuffix, payload)
	}
	if err := s.broker.publishRequest(plantID, payload); err != nil {
		return nil, err
	}

	timer := time.NewTimer(s.timeout)
	defer timer.Stop()
	for {
		select {
		case raw := <-s.broker.responses:
			if s.verbose {
				log.Printf("<- fromDevice: %s", raw)
			}
			var resp header
			if err := json.Unmarshal(raw, &resp); err != nil {
				log.Printf("ignoring unparseable fromDevice payload: %v", err)
				continue
			}
			if resp.OrderId != orderID || len(resp.Lines) == 0 {
				continue // stale QoS retransmit or foreign traffic
			}
			l := resp.Lines[0]
			if l.Error != "" {
				return nil, dongleError{l.Error}
			}
			frame, err := hex.DecodeString(l.Modbus)
			if err != nil {
				return nil, fmt.Errorf("bad hex in response: %w", err)
			}
			return frame, nil
		case <-timer.C:
			return nil, fmt.Errorf("no response from dongle within %s", s.timeout)
		case <-ctx.Done():
			return nil, ctx.Err()
		}
	}
}

// probe checks one slave address. ident.Present == false means the address is
// silent (dongle-reported bus timeout after retries).
func (s *scanner) probe(ctx context.Context, addr byte) (ident, error) {
	frameHex := buildRead(addr, 0, 8)
	var lastNote string
	for attempt := 1; attempt <= 2; attempt++ {
		frame, err := s.exchange(ctx, frameHex)
		switch e := err.(type) {
		case nil:
			id, perr := parseIdent(frame, addr)
			if perr != nil {
				// Something answered but the frame is unusable: retry, then
				// record the address as present-but-garbled.
				lastNote = perr.Error()
				continue
			}
			return id, nil
		case dongleError:
			if e.msg == "Response timeout" {
				if attempt == 1 {
					continue // one retry against transient bus noise
				}
				return ident{Present: false}, nil
			}
			// "Invalid CRC in response" etc.: something is answering.
			lastNote = e.msg
			continue
		default:
			if ctx.Err() != nil {
				return ident{}, ctx.Err()
			}
			if attempt == 1 {
				continue // tool-side MQTT timeout: retry once
			}
			return ident{}, err
		}
	}
	return ident{Present: true, Note: "garbled response: " + lastNote}, nil
}

// probeSOC reads Battery-1 SOC (register 588); returns -1 when unavailable.
func (s *scanner) probeSOC(ctx context.Context, addr byte) int {
	frame, err := s.exchange(ctx, buildRead(addr, 588, 1))
	if err != nil {
		return -1
	}
	soc, err := parseSOC(frame, addr)
	if err != nil {
		return -1
	}
	return soc
}

type result struct {
	Addr byte
	ident
	SOC int // -1 = not read
}

// scan probes addresses 1..247. In default mode it stops at the first silent
// address; with full=true it sweeps the whole range.
func (s *scanner) scan(ctx context.Context, full, readSOC bool) ([]result, error) {
	var results []result
	last := byte(247)
	for addr := byte(1); addr <= last; addr++ {
		fmt.Printf("probing address %3d... ", addr)
		id, err := s.probe(ctx, addr)
		if err != nil {
			fmt.Println()
			return results, err
		}
		if !id.Present {
			fmt.Println("no answer")
			if !full {
				fmt.Println("\nStopped at the first silent address. Devices with a gap in their")
				fmt.Println("addressing (e.g. 1, 2, 5) are missed — re-run with --full to sweep 1-247.")
				break
			}
			continue
		}
		r := result{Addr: addr, ident: id, SOC: -1}
		if id.HasIdent {
			fmt.Printf("found: SN %s (%s)\n", id.SN, deviceTypeName(id.DeviceType))
		} else {
			fmt.Printf("found: %s\n", id.Note)
		}
		if readSOC && id.HasIdent {
			r.SOC = s.probeSOC(ctx, addr)
		}
		results = append(results, r)
	}
	return results, nil
}
