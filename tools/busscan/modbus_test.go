package main

import (
	"encoding/hex"
	"testing"
)

// Captured known-good frames from tools/cloud_roundtrip.py (CRC included).
func TestCRC16CapturedFrames(t *testing.T) {
	frames := []string{
		"01030204000345B2",
		"0103020A00032471",
		"0103020F00033470",
		"0103021900015475",
		"0103024B0002B5A5",
	}
	for _, f := range frames {
		b, err := hex.DecodeString(f)
		if err != nil {
			t.Fatal(err)
		}
		want := uint16(b[len(b)-2]) | uint16(b[len(b)-1])<<8
		if got := crc16(b[:len(b)-2]); got != want {
			t.Errorf("crc16(%s) = %04X, want %04X", f, got, want)
		}
	}
}

func TestBuildRead(t *testing.T) {
	// Rebuild a captured frame: addr 1, fn 3, start 0x0204, count 3.
	if got := buildRead(1, 0x0204, 3); got != "01030204000345B2" {
		t.Errorf("buildRead = %s", got)
	}
	// The ident frame used by the scanner: registers 0-7.
	f := buildRead(1, 0, 8)
	b, _ := hex.DecodeString(f)
	if len(b) != 8 || b[0] != 1 || b[1] != 3 || b[5] != 8 {
		t.Errorf("ident frame malformed: %s", f)
	}
}

// synthIdentResponse builds a valid response frame for registers 0-7.
func synthIdentResponse(addr byte, devType, modbusAddr, proto uint16, sn string) []byte {
	regs := []uint16{devType, modbusAddr, proto}
	b := []byte(sn)
	for len(b) < 10 {
		b = append(b, 0)
	}
	for i := 0; i < 10; i += 2 {
		regs = append(regs, uint16(b[i])<<8|uint16(b[i+1]))
	}
	f := []byte{addr, 0x03, 16}
	for _, r := range regs {
		f = append(f, byte(r>>8), byte(r))
	}
	c := crc16(f)
	return append(f, byte(c), byte(c>>8))
}

func TestParseIdent(t *testing.T) {
	f := synthIdentResponse(2, 0x0500, 2, 0x0102, "2299999999")
	id, err := parseIdent(f, 2)
	if err != nil {
		t.Fatal(err)
	}
	if !id.Present || !id.HasIdent || id.SN != "2299999999" || id.DeviceType != 0x0500 ||
		id.ModbusAddr != 2 || id.Proto != 0x0102 || id.Note != "" {
		t.Errorf("unexpected ident: %+v", id)
	}
}

func TestParseIdentException(t *testing.T) {
	f := []byte{0xAA, 0x83, 0x01}
	c := crc16(f)
	f = append(f, byte(c), byte(c>>8))
	id, err := parseIdent(f, 0xAA)
	if err != nil {
		t.Fatal(err)
	}
	if !id.Present || id.HasIdent || id.Exception != 0x01 {
		t.Errorf("unexpected ident: %+v", id)
	}
}

func TestParseIdentTruncated(t *testing.T) {
	if _, err := parseIdent([]byte{0x01, 0x03}, 1); err == nil {
		t.Error("expected error for truncated frame")
	}
}

func TestParseIdentBadCRC(t *testing.T) {
	f := synthIdentResponse(1, 0x0500, 1, 0x0102, "2299999999")
	f[len(f)-1] ^= 0xFF
	if _, err := parseIdent(f, 1); err == nil {
		t.Error("expected error for bad CRC")
	}
}

func TestParseIdentWrongAddress(t *testing.T) {
	f := synthIdentResponse(3, 0x0600, 3, 0x0102, "2299999998")
	id, err := parseIdent(f, 1)
	if err != nil {
		t.Fatal(err)
	}
	if !id.Present || id.Note == "" {
		t.Errorf("expected wrong-address note, got %+v", id)
	}
}

func TestParseSOC(t *testing.T) {
	f := []byte{0x01, 0x03, 0x02, 0x00, 0x55}
	c := crc16(f)
	f = append(f, byte(c), byte(c>>8))
	soc, err := parseSOC(f, 1)
	if err != nil || soc != 85 {
		t.Errorf("soc = %d, err = %v", soc, err)
	}
}
