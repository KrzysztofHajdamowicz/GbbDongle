// busscan enumerates Modbus RTU devices (Deye inverters) on the RS485 bus
// behind a GbbDongle. It runs a local MQTT broker that accepts any
// credentials; once the dongle is pointed at it (manually via the web UI, or
// automatically with --dongle), it probes consecutive slave addresses with a
// "read registers 0-7" frame and prints the serial number and device type of
// everything that answers.
package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"sync"
	"syscall"
	"text/tabwriter"
	"time"
)

func main() {
	os.Exit(run())
}

func run() int {
	var (
		dongleHost = flag.String("dongle", "", "dongle hostname or IP (e.g. gbbdongle.local); enables automatic reconfiguration and restore")
		port       = flag.Int("port", 1883, "TCP port for the local MQTT broker")
		ip         = flag.String("ip", "", "local IP to advertise to the dongle (default: auto-detect)")
		full       = flag.Bool("full", false, "sweep all addresses 1-247 instead of stopping at the first silent one")
		readSOC    = flag.Bool("soc", false, "also read Battery-1 SOC (register 588) from each device")
		timeout    = flag.Duration("timeout", 5*time.Second, "per-request timeout waiting for the dongle's MQTT response")
		wait       = flag.Duration("wait", 120*time.Second, "how long to wait for the dongle to connect")
		verbose    = flag.Bool("v", false, "verbose logging (MQTT payloads, client connects)")
	)
	flag.Parse()
	log.SetFlags(log.Ltime)

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	b, err := startBroker(*port, *verbose)
	if err != nil {
		log.Printf("error: %v", err)
		return 2
	}
	defer b.close()
	fmt.Printf("MQTT broker listening on port %d (any credentials accepted)\n\n", *port)

	// restore is set up by the auto-reconfigure path and must run exactly
	// once, on any exit path (incl. Ctrl-C), with a fresh context.
	var restoreOnce sync.Once
	restore := func() {}
	defer func() { restoreOnce.Do(restore) }()

	if *dongleHost != "" {
		saved, restoreFn, err := autoReconfigure(ctx, *dongleHost, *ip, *port)
		if err != nil {
			log.Printf("error: %v", err)
			return 2
		}
		restore = restoreFn
		fmt.Printf("Saved dongle configuration (will be restored on exit):\n  %s\n\n", saved)
	} else {
		cands := candidateIPs()
		if len(cands) == 0 {
			fmt.Println("No private IPv4 addresses found — check your network connection.")
		}
		printCandidates(cands, *port)
	}

	fmt.Printf("Waiting up to %s for the dongle to connect...\n", *wait)
	waitCtx, cancel := context.WithTimeout(ctx, *wait)
	s := &scanner{broker: b, timeout: *timeout, wait: *wait, verbose: *verbose}
	_, err = b.tracker.waitReady(waitCtx)
	cancel()
	if err != nil {
		if ctx.Err() != nil {
			log.Printf("interrupted")
		} else {
			log.Printf("dongle did not connect within %s", *wait)
		}
		return 2
	}

	fmt.Println()
	results, scanErr := s.scan(ctx, *full, *readSOC)
	fmt.Println()
	printResults(results, *readSOC)

	// Restore the original configuration before reporting the outcome so the
	// tester sees restore errors even on a successful scan.
	restoreOnce.Do(restore)

	if scanErr != nil {
		log.Printf("scan aborted: %v", scanErr)
		return 2
	}
	if *dongleHost == "" {
		fmt.Println("\nRemember to restore the original MQTT settings in the dongle's web UI.")
	}
	if len(results) == 0 {
		return 1
	}
	return 0
}

// autoReconfigure saves the dongle's MQTT config, points it at this tool and
// returns a restore func that puts the original values back.
func autoReconfigure(ctx context.Context, host, ipOverride string, port int) (dongleConfig, func(), error) {
	d := newDongleClient(host)
	saved, err := d.fetchConfig(ctx)
	if err != nil {
		return dongleConfig{}, nil, err
	}

	localIP := ipOverride
	if localIP == "" {
		localIP, err = advertiseIP(host)
		if err != nil {
			return dongleConfig{}, nil, fmt.Errorf(
				"cannot auto-detect the local IP routed to %s: %w (pass it with --ip)", host, err)
		}
	}

	fmt.Printf("Reconfiguring dongle %s -> MQTT %s:%d, TLS off, cloud on (restarts the dongle)\n",
		host, localIP, port)
	err = d.applyConfig(ctx, dongleConfig{Server: localIP, Port: port, TLS: false, Cloud: true})
	if err != nil {
		return dongleConfig{}, nil, fmt.Errorf("reconfiguring dongle: %w", err)
	}

	restore := func() {
		// Deliberately not the run context: restore must work after Ctrl-C.
		// Generous timeout: each POST may retry for a few seconds.
		rctx, cancel := context.WithTimeout(context.Background(), 60*time.Second)
		defer cancel()
		fmt.Printf("\nRestoring dongle configuration: %s\n", saved)
		if err := d.applyConfig(rctx, saved); err != nil {
			fmt.Printf("FAILED to restore the dongle configuration: %v\n", err)
			fmt.Printf("Restore it manually in the web UI (http://%s):\n  %s\n", host, saved)
			return
		}
		fmt.Println("Dongle configuration restored.")
	}
	return saved, restore, nil
}

func printResults(results []result, withSOC bool) {
	if len(results) == 0 {
		fmt.Println("No Modbus devices found.")
		return
	}
	seen := map[string]bool{}
	w := tabwriter.NewWriter(os.Stdout, 2, 4, 2, ' ', 0)
	if withSOC {
		fmt.Fprintln(w, "ADDR\tSERIAL\tTYPE\tPROTO\tSOC\tNOTE")
	} else {
		fmt.Fprintln(w, "ADDR\tSERIAL\tTYPE\tPROTO\tNOTE")
	}
	for _, r := range results {
		sn, typ, proto := "-", "-", "-"
		note := r.Note
		if r.HasIdent {
			sn = r.SN
			typ = fmt.Sprintf("%s (0x%04X)", deviceTypeName(r.DeviceType), r.DeviceType)
			proto = fmt.Sprintf("0x%04X", r.Proto)
			if seen[r.SN] {
				note = trimJoin(note, "DUPLICATE SN")
			}
			seen[r.SN] = true
			if int(r.ModbusAddr) != int(r.Addr) {
				note = trimJoin(note, fmt.Sprintf("reg1 reports address %d", r.ModbusAddr))
			}
		}
		if withSOC {
			soc := "-"
			if r.SOC >= 0 {
				soc = fmt.Sprintf("%d%%", r.SOC)
			}
			fmt.Fprintf(w, "%d\t%s\t%s\t%s\t%s\t%s\n", r.Addr, sn, typ, proto, soc, note)
		} else {
			fmt.Fprintf(w, "%d\t%s\t%s\t%s\t%s\n", r.Addr, sn, typ, proto, note)
		}
	}
	w.Flush()
	fmt.Printf("\n%d device(s) found.\n", len(results))
}

func trimJoin(a, b string) string {
	if a == "" {
		return b
	}
	return a + "; " + b
}
