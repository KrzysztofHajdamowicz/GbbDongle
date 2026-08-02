package main

import (
	"fmt"
	"net"
	"os"
	"text/tabwriter"
)

type candidate struct {
	Iface string
	IP    string
}

// candidateIPs lists private (RFC1918) IPv4 addresses on up, non-loopback
// interfaces — the addresses a dongle on the LAN could plausibly reach.
func candidateIPs() []candidate {
	var out []candidate
	ifaces, err := net.Interfaces()
	if err != nil {
		return out
	}
	for _, ifc := range ifaces {
		if ifc.Flags&net.FlagUp == 0 || ifc.Flags&net.FlagLoopback != 0 {
			continue
		}
		addrs, err := ifc.Addrs()
		if err != nil {
			continue
		}
		for _, a := range addrs {
			ipnet, ok := a.(*net.IPNet)
			if !ok {
				continue
			}
			ip := ipnet.IP.To4()
			if ip == nil || !ip.IsPrivate() {
				continue
			}
			out = append(out, candidate{Iface: ifc.Name, IP: ip.String()})
		}
	}
	return out
}

func printCandidates(cands []candidate, port int) {
	w := tabwriter.NewWriter(os.Stdout, 2, 4, 2, ' ', 0)
	fmt.Fprintln(w, "INTERFACE\tIP")
	for _, c := range cands {
		fmt.Fprintf(w, "%s\t%s\n", c.Iface, c.IP)
	}
	w.Flush()
	fmt.Printf("\nPoint the GbbDongle at one of the IPs above (the one on the same network\n"+
		"as the dongle), via its web UI:\n"+
		"  MQTT Server = <IP>\n"+
		"  MQTT Port   = %d\n"+
		"  TLS         = off\n"+
		"  Cloud Connection = on\n"+
		"then press \"Apply Settings (Restart)\".\n\n", port)
}

// advertiseIP picks the local IP the OS would use to reach the dongle: the
// UDP "connect" performs no I/O, it only consults the routing table.
func advertiseIP(dongleHost string) (string, error) {
	conn, err := net.Dial("udp4", net.JoinHostPort(dongleHost, "80"))
	if err != nil {
		return "", err
	}
	defer conn.Close()
	addr, ok := conn.LocalAddr().(*net.UDPAddr)
	if !ok {
		return "", fmt.Errorf("unexpected local address type %T", conn.LocalAddr())
	}
	return addr.IP.String(), nil
}
