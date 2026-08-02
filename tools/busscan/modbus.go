package main

import (
	"encoding/hex"
	"fmt"
	"strings"
)

// crc16 computes the Modbus RTU CRC (init 0xFFFF, poly 0xA001, reflected).
func crc16(data []byte) uint16 {
	crc := uint16(0xFFFF)
	for _, b := range data {
		crc ^= uint16(b)
		for i := 0; i < 8; i++ {
			if crc&1 != 0 {
				crc = (crc >> 1) ^ 0xA001
			} else {
				crc >>= 1
			}
		}
	}
	return crc
}

// buildRead builds an uppercase-hex "read holding registers" (0x03) RTU frame
// including the CRC (appended low byte first), as expected by the dongle.
func buildRead(addr byte, start, count uint16) string {
	f := []byte{addr, 0x03, byte(start >> 8), byte(start), byte(count >> 8), byte(count)}
	c := crc16(f)
	f = append(f, byte(c), byte(c>>8))
	return strings.ToUpper(hex.EncodeToString(f))
}

// deviceTypeName maps Deye register 0 to a human-readable device type.
func deviceTypeName(v uint16) string {
	switch v >> 8 {
	case 0x02:
		return "string inverter"
	case 0x03:
		return "1-phase hybrid"
	case 0x04:
		return "microinverter"
	case 0x05:
		return "LV 3-phase hybrid"
	case 0x06:
		return "HV 3-phase hybrid"
	default:
		return "unknown"
	}
}

// ident is the parsed result of probing one slave address.
type ident struct {
	Present    bool
	Exception  byte // Modbus exception code, 0 if none
	SN         string
	DeviceType uint16
	HasIdent   bool // DeviceType/ModbusAddr/Proto/SN are valid
	ModbusAddr uint16
	Proto      uint16
	Note       string
}

// parseIdent interprets the raw response frame to a read of registers 0-7
// (device type, modbus address, protocol version, serial number).
func parseIdent(frame []byte, probed byte) (ident, error) {
	if len(frame) < 5 {
		return ident{}, fmt.Errorf("response too short (%d bytes)", len(frame))
	}
	body := frame[:len(frame)-2]
	got := uint16(frame[len(frame)-2]) | uint16(frame[len(frame)-1])<<8
	if crc16(body) != got {
		return ident{}, fmt.Errorf("bad CRC in response %X", frame)
	}
	id := ident{Present: true}
	if frame[0] != probed {
		id.Note = fmt.Sprintf("reply from address %d", frame[0])
	}
	if frame[1]&0x80 != 0 {
		id.Exception = frame[2]
		id.Note = strings.TrimSpace(id.Note + fmt.Sprintf(" Modbus exception 0x%02X (device present)", frame[2]))
		return id, nil
	}
	if frame[1] != 0x03 {
		return ident{}, fmt.Errorf("unexpected function 0x%02X in response %X", frame[1], frame)
	}
	if frame[2] != 16 || len(frame) < 3+16+2 {
		return ident{}, fmt.Errorf("unexpected payload length %d in response %X", frame[2], frame)
	}
	regs := make([]uint16, 8)
	for i := range regs {
		regs[i] = uint16(frame[3+2*i])<<8 | uint16(frame[4+2*i])
	}
	id.HasIdent = true
	id.DeviceType = regs[0]
	id.ModbusAddr = regs[1]
	id.Proto = regs[2]
	sn := make([]byte, 0, 10)
	for _, r := range regs[3:8] {
		sn = append(sn, byte(r>>8), byte(r))
	}
	id.SN = sanitizeSN(sn)
	return id, nil
}

// sanitizeSN trims NUL/space padding and escapes non-printable characters.
func sanitizeSN(b []byte) string {
	s := strings.Trim(string(b), "\x00 ")
	var out strings.Builder
	for _, c := range []byte(s) {
		if c >= 0x20 && c < 0x7F {
			out.WriteByte(c)
		} else {
			fmt.Fprintf(&out, "\\x%02X", c)
		}
	}
	return out.String()
}

// parseSOC interprets the response to a single-register read of reg 588
// (Battery-1 SOC, percent). Returns -1 when the device answered with an
// exception.
func parseSOC(frame []byte, probed byte) (int, error) {
	if len(frame) < 5 {
		return -1, fmt.Errorf("response too short (%d bytes)", len(frame))
	}
	body := frame[:len(frame)-2]
	got := uint16(frame[len(frame)-2]) | uint16(frame[len(frame)-1])<<8
	if crc16(body) != got {
		return -1, fmt.Errorf("bad CRC in response %X", frame)
	}
	if frame[1]&0x80 != 0 {
		return -1, nil
	}
	if frame[1] != 0x03 || frame[2] != 2 || len(frame) < 7 {
		return -1, fmt.Errorf("unexpected SOC response %X", frame)
	}
	return int(uint16(frame[3])<<8 | uint16(frame[4])), nil
}
