/*
  sine_pwm.cpp

  Generate a sine-like analog output using PWM + a software sample ticker.
  Compatible with Arduino (Nano 33 BLE / Mbed-based cores) in VS Code.
  You can change frequency and amplitude from the Serial Monitor by typing:
    <frequency_hz> <amplitude_v>
  Example:
    500 3.3
  This will set frequency=500 Hz and amplitude=3.3 V (clamped to VCC).

  Notes / changes made:
  - Use explicit mbed::PwmOut and mbed::Ticker while keeping Arduino Serial for VS Code Arduino workflow.
  - Replace digitalPinToPinName(...) usage with the Arduino D9 pin-name symbol (PIN -> PinName).
  - Add safety checks (tableSize > 0) and guard against divide-by-zero.
  - Use modulo for index wrap instead of some fragile branch logic.
  - Ensure ticker is detached while rebuilding table to avoid concurrent reads/writes.
  - Improved parsing & user feedback; parsing supports "freq amp" (e.g. "500 3.3").
  - Kept original design: volatile shared variables, minimal ISR work, linear interpolation.
*/

#include <Arduino.h>
#include "mbed.h"

// PWM output pin (use Arduino pin name that maps to an mbed PinName)

#define PWM_PIN 12
#define MAX_TABLE_SIZE 200  // allowed max table size
int TABLE_SIZE = 100;      // default table entries

// mbed objects (explicit namespace to avoid name collisions)
mbed::PwmOut pwmOut(digitalPinToPinName(PWM_PIN));
mbed::Ticker sampleTicker;

// Sine lookup table (0..1)
float sineTable[MAX_TABLE_SIZE];

// Shared/ISR variables (volatile)
volatile float phase = 0.0f;             // phase in cycles [0.0 .. 1.0)
volatile int currentTableSize = 100;     // number of samples per cycle
volatile bool tickerAttached = false;
volatile float amplitude_v = 3.3f;       // desired peak voltage (0 .. VCC)
volatile float amplitude_scale = 1.0f;   // amplitude_v / VCC

// Non-volatile control variables (main context)
float frequency = 0.5f; // default 0.5 Hz
unsigned int currentIntervalUs = 0; // microseconds between samples

const float PI_F = 3.14159265358979323846f;
const float VCC = 3.3f; // approximate supply rail of Nano 33 BLE (adjust if needed)

// Minimal ISR: compute PWM duty and write
void onSampleTick() {
    // Read volatile copies once to reduce repeated volatile reads
    int tableSize = currentTableSize;  // volatile -> read once
    if (tableSize <= 0) return;        // safety
    float p = phase;
    float a_scale = amplitude_scale;

    // Map phase to fractional table position
    float pos = p * (float)tableSize;
    int idx0 = (int)floorf(pos);
    // safe wrap using modulo (in case idx0 == tableSize due to float rounding)
    if (idx0 >= tableSize) idx0 = idx0 % tableSize;
    int idx1 = idx0 + 1;
    if (idx1 >= tableSize) idx1 -= tableSize;
    float frac = pos - (float)idx0;

    // Linear interpolation between sineTable[idx0] and sineTable[idx1]
    float s0 = sineTable[idx0];
    float s1 = sineTable[idx1];
    float sample = s0 + (s1 - s0) * frac; // value in 0..1

    // Duty is scaled so low = 0V, high = amplitude_v
    float duty = a_scale * sample; // duty range 0 .. 1
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    pwmOut.write(duty);

    // Advance phase by one table step (1/tableSize)
    float step = 1.0f / (float)tableSize;
    p += step;
    if (p >= 1.0f) p -= 1.0f;
    phase = p; // write back
}

// Build sine table (values 0..1). Call only when ticker is detached.
void buildSineTable(int tableSize) {
    if (tableSize <= 0) return;
    if (tableSize > MAX_TABLE_SIZE) tableSize = MAX_TABLE_SIZE;
    for (int i = 0; i < tableSize; i++) {
        sineTable[i] = (sinf(2.0f * PI_F * (float)i / (float)tableSize) + 1.0f) * 0.5f;
    }
}

// Attach ticker (ensure detached first)
void attachTickerUs(unsigned int intervalUs) {
    // detach if previously attached
    if (tickerAttached) {
        sampleTicker.detach();
        tickerAttached = false;
    }
    // attach_us expects a callable; use mbed::callback
    sampleTicker.attach_us(mbed::callback(onSampleTick), intervalUs);
    tickerAttached = true;
}

void updateForFrequencyAndTable(float freq, int newTableSize) {
    if (freq <= 0.0f) return;
    frequency = freq;

    if (newTableSize <= 0) newTableSize = TABLE_SIZE;
    if (newTableSize > MAX_TABLE_SIZE) newTableSize = MAX_TABLE_SIZE;

    // detach ticker while rebuilding the table to avoid concurrent reads
    if (tickerAttached) {
        sampleTicker.detach();
        tickerAttached = false;
    }

    // Keep phase as-is to preserve continuity.
    currentTableSize = newTableSize;
    buildSineTable(currentTableSize);

    // Compute ISR interval (us) for desired frequency such that we produce newTableSize samples per cycle:
    // interval_us = 1e6 / (frequency * samples_per_cycle)
    float denom = frequency * (float)currentTableSize;
    if (denom <= 0.0f) denom = 1.0f; // safety
    float interval_f = 1e6f / denom;
    if (interval_f < 6.0f) interval_f = 6.0f; // prevent too-fast ISR
    currentIntervalUs = (unsigned int)(interval_f + 0.5f);

    // Re-attach ticker with the new interval; phase not reset to preserve continuity
    attachTickerUs(currentIntervalUs);
}

void applyAmplitude(float amp) {
    if (amp < 0.0f) amp = 0.0f;
    if (amp > VCC) amp = VCC;
    amplitude_v = amp;
    amplitude_scale = amplitude_v / VCC;
    if (amplitude_scale > 1.0f) amplitude_scale = 1.0f;
    if (amplitude_scale < 0.0f) amplitude_scale = 0.0f;
}

void setup() {
    // Initialize Serial (Arduino)
    Serial.begin(115200);
    while (!Serial) { delay(1); } // wait for serial USB (optional)

    // Setup PWM carrier frequency (period_us) for better RC smoothing
    pwmOut.period_us(17); // ~66.7 kHz carrier (tweak if needed)

    // start with 0V until configured
    pwmOut.write(0.0f);

    // Table & defaults
    if (TABLE_SIZE > MAX_TABLE_SIZE) TABLE_SIZE = MAX_TABLE_SIZE;
    currentTableSize = TABLE_SIZE;

    if (tickerAttached) {
        sampleTicker.detach();
        tickerAttached = false;
    }

    buildSineTable(currentTableSize);

    // start phase at 0.0
    phase = 0.0f;

    // Apply defaults
    applyAmplitude(amplitude_v);

    // Choose adaptive table-size for the default frequency
    // Choose adaptive table-size (keeps ISR <= ~30kHz)
    int defaultTableSize;
    float f = frequency;
    if (f <= 150.0f)      defaultTableSize = 200;
    else if (f <= 300.0f) defaultTableSize = 100;
    else if (f <= 600.0f) defaultTableSize = 50;
    else if (f <= 1200.0f) defaultTableSize = 30;
    else                   defaultTableSize = 20;
    if (defaultTableSize > MAX_TABLE_SIZE) defaultTableSize = MAX_TABLE_SIZE;

    updateForFrequencyAndTable(frequency, defaultTableSize);

    // Ready marker
    Serial.println("ARDUINO_READY");
    Serial.flush();
}

// Process one line of input. Accepts either "<freq> <amp>" or more descriptive text:
void processSerialLine(const char *line) {
    float freq = -1.0f;
    float amp = -1.0f;

    // Try simple "freq amp" first
    int n = sscanf(line, "%f %f", &freq, &amp);

    // If not two numbers, try to find keywords "frequency" and "amplitude" in free text
    if (n < 2) {
        const char *pf = strstr(line, "frequency");
        if (pf) {
            float ftmp;
            if (sscanf(pf, "%*[^0-9-]%f", &ftmp) == 1) freq = ftmp;
        }
        const char *pa = strstr(line, "amplitude");
        if (pa) {
            float atmp;
            if (sscanf(pa, "%*[^0-9-.]%f", &atmp) == 1) amp = atmp;
        }
    }

    // If we have both values, apply them
    if (freq > 0.0f && amp >= 0.0f) {
        // Choose table size heuristically based on freq
        int newTableSize;
      // Adaptive table-size (keeps ISR <= ~30kHz)
if      (freq <= 150.0f)  newTableSize = 200; // 1..150 Hz
else if (freq <= 300.0f)  newTableSize = 100; // 150..300 Hz
else if (freq <= 600.0f)  newTableSize = 50;  // 300..600 Hz
else if (freq <= 1200.0f) newTableSize = 30;  // 600..1200 Hz
else                      newTableSize = 20;  // 1200..1600 Hz

        if (newTableSize > MAX_TABLE_SIZE) newTableSize = MAX_TABLE_SIZE;

        // Clamp amplitude to VCC
        if (amp > VCC) amp = VCC;

        // Apply amplitude & frequency (these keep phase intact)
        applyAmplitude(amp);
        updateForFrequencyAndTable(freq, newTableSize);

        Serial.println("ACK");
        Serial.print("APPLIED freq=");
        Serial.print(freq, 3);
        Serial.print(" Hz amp=");
        Serial.print(amp, 3);
        Serial.print(" V tableSize=");
        Serial.print(currentTableSize);
        Serial.print(" interval_us=");
        Serial.println(currentIntervalUs);
        Serial.flush();
    } else {
        Serial.println("NACK");
        Serial.println("PARSE_ERROR - expected format: \"<frequency_hz> <amplitude_v>\" e.g. \"500 3.3\"");
        Serial.flush();
    }
}

void loop() {
    static char buf[200];
    static int bufpos = 0;

    while (Serial.available() > 0) {
        int cInt = Serial.read();
        if (cInt < 0) break;
        char c = (char)cInt;
        if (c == '\r') continue;
        if (c == '\n' || bufpos >= (int)sizeof(buf) - 1) {
            if (bufpos > 0) {
                buf[bufpos] = '\0';
                processSerialLine(buf);
            }
            bufpos = 0;
        } else {
            buf[bufpos++] = c;
        }
    }

    // yield CPU; ISR handles waveform generation
    delay(1);
}

