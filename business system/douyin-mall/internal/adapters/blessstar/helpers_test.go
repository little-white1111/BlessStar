package adapter_blessstar

import (
	"testing"
	"time"
)

func TestToInt32(t *testing.T) {
	tests := []struct {
		name    string
		input   interface{}
		want    int32
		wantOK  bool
	}{
		{"int32 direct", int32(42), 42, true},
		{"int", int(42), 42, true},
		{"int64", int64(42), 42, true},
		{"float64", float64(42.0), 42, true},
		{"float64 truncated", float64(42.7), 42, true},
		{"string fails", "42", 0, false},
		{"bool fails", true, 0, false},
		{"nil fails", nil, 0, false},
		{"negative int", int(-5), -5, true},
		{"zero", int(0), 0, true},
		{"int32 max", int32(2147483647), 2147483647, true},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, ok := toInt32(tt.input)
			if ok != tt.wantOK {
				t.Errorf("toInt32() ok = %v, wantOK %v", ok, tt.wantOK)
			}
			if got != tt.want {
				t.Errorf("toInt32() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestToBool(t *testing.T) {
	tests := []struct {
		name   string
		input  interface{}
		want   bool
		wantOK bool
	}{
		{"true", true, true, true},
		{"false", false, false, true},
		{"int fails", int(1), false, false},
		{"string fails", "true", false, false},
		{"nil fails", nil, false, false},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, ok := toBool(tt.input)
			if ok != tt.wantOK {
				t.Errorf("toBool() ok = %v, wantOK %v", ok, tt.wantOK)
			}
			if got != tt.want {
				t.Errorf("toBool() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestToString(t *testing.T) {
	tests := []struct {
		name   string
		input  interface{}
		want   string
		wantOK bool
	}{
		{"string direct", "hello", "hello", true},
		{"empty string", "", "", true},
		{"int fallback", int(42), "42", false},
		{"bool fallback", true, "true", false},
		{"nil fallback", nil, "<nil>", false},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, ok := toString(tt.input)
			if ok != tt.wantOK {
				t.Errorf("toString() ok = %v, wantOK %v", ok, tt.wantOK)
			}
			if got != tt.want {
				t.Errorf("toString() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestToStringSlice(t *testing.T) {
	tests := []struct {
		name   string
		input  interface{}
		want   []string
		wantOK bool
	}{
		{"[]string direct", []string{"a", "b"}, []string{"a", "b"}, true},
		{"[]interface{} conversion", []interface{}{"x", "y"}, []string{"x", "y"}, true},
		{"empty slice", []string{}, []string{}, true},
		{"single element", []string{"*"}, []string{"*"}, true},
		{"int fails", int(42), nil, false},
		{"nil fails", nil, nil, false},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, ok := toStringSlice(tt.input)
			if ok != tt.wantOK {
				t.Errorf("toStringSlice() ok = %v, wantOK %v", ok, tt.wantOK)
			}
			if !equalStringSlice(got, tt.want) {
				t.Errorf("toStringSlice() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestToDurationSeconds(t *testing.T) {
	tests := []struct {
		name   string
		input  interface{}
		want   time.Duration
		wantOK bool
	}{
		{"int 86400", int(86400), 86400 * time.Second, true},
		{"int32 3600", int32(3600), 3600 * time.Second, true},
		{"int64 1800", int64(1800), 1800 * time.Second, true},
		{"float64 60.0", float64(60.0), 60 * time.Second, true},
		{"time.Duration direct", 30 * time.Minute, 30 * time.Minute, true},
		{"string fails", "86400", 0, false},
		{"nil fails", nil, 0, false},
		{"zero", int(0), 0, true},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, ok := toDurationSeconds(tt.input)
			if ok != tt.wantOK {
				t.Errorf("toDurationSeconds() ok = %v, wantOK %v", ok, tt.wantOK)
			}
			if got != tt.want {
				t.Errorf("toDurationSeconds() = %v, want %v", got, tt.want)
			}
		})
	}
}

func equalStringSlice(a, b []string) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}
