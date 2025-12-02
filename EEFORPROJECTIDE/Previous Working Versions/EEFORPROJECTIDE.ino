// File: sine_pwm_control_arduino_33_ble.ino
// Updated for continuous, 0V-bottom true sine output with minimal stepping.
// - Low of sine is always 0V and high is the commanded amplitude (0..VCC).
// - Uses a phase accumulator + linear interpolation to eliminate discrete-step jumps
//   and preserve phase continuity when changing frequency/table size/amplitude.
// - Keeps high-frequency PWM carrier (period_us ~ 15) for best analog smoothing with an RC filter.
// - Does NOT detach/reset phase when updating frequency/table, so waveform remains continuous.
//
// Notes for achieving the "best" sine:
// - The MCU produces a PWM duty that, after a low-pass filter (RC), yields the analog waveform.
//   Hardware filtering (LC/RC) and PCB layout critically affect noise. Software alone cannot fully
//   eliminate noise; use a good low-pass filter and small RC cutoff far below PWM carrier but above
//   the highest sine harmonic you care about.
// - Larger table sizes and higher PWM carrier reduce quantization noise; interpolation reduces stepping.
// - Use pwmPin.period_us(10..20) as a tradeoff between attainable resolution and CPU/timer constraints.

#include "mbed.h"
using namespace mbed;

#define PWM_PIN 9
#define MAX_TABLE_SIZE 200  // increased safe cap to allow larger tables if CPU can handle it
int TABLE_SIZE = 100; // default table entries (larger value -> smoother analog sine after filtering)

PwmOut pwmPin(digitalPinToPinName(PWM_PIN));
Ticker sampleTicker;

float sineTable[MAX_TABLE_SIZE];

// Shared/ISR variables (volatile)
volatile float phase = 0.0f;             // phase in cycles [0.0 .. 1.0)
volatile int currentTableSize = 100;     // number of samples per cycle (table resolution)
volatile bool tickerAttached = false;
volatile float amplitude_v = 3.3f;       // desired peak voltage (0 .. VCC)
volatile float amplitude_scale = 1.0f;   // amplitude_v / VCC, applied to 0..1 sine

float frequency = 0.5f; // default 0.5 Hz
unsigned int currentIntervalUs = 0; // microseconds between samples

const float PI_F = 3.14159265358979323846f;
const float VCC = 3.3f; // approx supply rail of Nano 33 BLE

// Minimal ISR: compute PWM duty and write
// Uses linear interpolation between table points for smooth output and to avoid visible stepping.
void onSampleTick() {
    // Read volatile copies once to reduce repeated volatile reads
    int tableSize = currentTableSize;           // small cost to read volatile
    float p = phase;                            // 0..1
    float a_scale = amplitude_scale;            // 0..1

    // Map phase to table position (fractional index)
    float pos = p * (float)tableSize;
    int idx0 = (int)floorf(pos);
    if (idx0 >= tableSize) idx0 = 0;
    int idx1 = idx0 + 1;
    if (idx1 >= tableSize) idx1 = 0;
    float frac = pos - (float)idx0;

    // Linear interpolation between sineTable[idx0] and sineTable[idx1]
    float s0 = sineTable[idx0];
    float s1 = sineTable[idx1];
    float sample = s0 + (s1 - s0) * frac; // value in 0..1

    // Duty is scaled so low = 0V, high = amplitude_v
    float duty = a_scale * sample; // duty range 0 .. amplitude_v/VCC
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    pwmPin.write(duty);

    // Advance phase by a fixed step of 1/tableSize cycles per sample (keeps one cycle = tableSize ticks)
    // Using phase (0..1) rather than integer index keeps continuity across updates.
    float step = 1.0f / (float)tableSize;
    p += step;
    if (p >= 1.0f) p -= 1.0f;
    phase = p; // write back volatile
}

// Build sine table (values 0..1). Call only when ticker is detached.
void buildSineTable(int tableSize) {
    if (tableSize <= 0) return;
    if (tableSize > MAX_TABLE_SIZE) tableSize = MAX_TABLE_SIZE;
    for (int i = 0; i < tableSize; i++) {
        sineTable[i] = (sinf(2.0f * PI_F * (float)i / (float)tableSize) + 1.0f) * 0.5f;
    }
}

void attachTickerUs(unsigned int intervalUs) {
    if (tickerAttached) {
        sampleTicker.detach();
        tickerAttached = false;
    }
    sampleTicker.attach_us(callback(&onSampleTick), intervalUs);
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

    // Keep phase as-is to preserve continuity. Do NOT reset phase to 0.
    // Phase meaning: current position inside the waveform cycle [0..1).
    // Changing table size maps phase to new table smoothly (pos = phase * newTableSize).
    currentTableSize = newTableSize;
    buildSineTable(currentTableSize);

    // Compute ISR interval (us) for desired frequency such that we produce newTableSize samples per cycle:
    float interval_f = 1e6f / (frequency * (float)currentTableSize);
    if (interval_f < 6.0f) interval_f = 6.0f; // prevent too-fast ISR (tune as needed)
    currentIntervalUs = (unsigned int)(interval_f + 0.5f);

    // Re-attach ticker with the new interval; phase not reset to preserve continuity
    attachTickerUs(currentIntervalUs);
}

void applyAmplitude(float amp) {
    if (amp < 0.0f) amp = 0.0f;
    if (amp > VCC) amp = VCC;
    amplitude_v = amp;
    // amplitude_scale maps 0..1 -> 0..VCC. We scale PWM duty by amplitude_scale so low is 0V.
    amplitude_scale = amplitude_v / VCC;
    if (amplitude_scale > 1.0f) amplitude_scale = 1.0f;
    if (amplitude_scale < 0.0f) amplitude_scale = 0.0f;
}

void setup() {
    Serial.begin(115200);

    // High-frequency PWM carrier for better smoothing by RC filter (10-20 us recommended)
    pwmPin.period_us(15);

    // output 0V until configured (lowest possible output)
    pwmPin.write(0.0f);

    // Table & defaults
    if (TABLE_SIZE > MAX_TABLE_SIZE) TABLE_SIZE = MAX_TABLE_SIZE;
    currentTableSize = TABLE_SIZE;

    if (tickerAttached) {
        sampleTicker.detach();
        tickerAttached = false;
    }

    buildSineTable(currentTableSize);

    // phase starts at 0 (arbitrary). Using a non-zero start won't change continuity properties.
    phase = 0.0f;

    // Apply default amplitude and frequency
    applyAmplitude(amplitude_v);

    // Choose adaptive table-size for the default frequency (tune sizes to preference)
    int defaultTableSize;
    float f = frequency;
    if (f <= 250.0f) defaultTableSize = 200;
    else if (f <= 500.0f) defaultTableSize = 150;
    else if (f <= 750.0f) defaultTableSize = 100;
    else if (f <= 1000.0f) defaultTableSize = 75;
    else if (f <= 1250.0f) defaultTableSize = 50;
    else defaultTableSize = 30;
    if (defaultTableSize > MAX_TABLE_SIZE) defaultTableSize = MAX_TABLE_SIZE;

    updateForFrequencyAndTable(frequency, defaultTableSize);

    // Print ready marker for host (non-blocking)
    Serial.println("ARDUINO_READY");
    Serial.flush();
}

void processSerialLine(const char *line) {
    float freq = -1.0f;
    float amp = -1.0f;

    int n = sscanf(line, "%f %f", &freq, &amp);
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

    if (freq > 0.0f && amp >= 0.0f) {
        int newTableSize;
        if (freq <= 250.0f) newTableSize = 200;
        else if (freq <= 500.0f) newTableSize = 150;
        else if (freq <= 750.0f) newTableSize = 100;
        else if (freq <= 1000.0f) newTableSize = 75;
        else if (freq <= 1250.0f) newTableSize = 50;
        else newTableSize = 30;
        if (newTableSize > MAX_TABLE_SIZE) newTableSize = MAX_TABLE_SIZE;

        // Apply amplitude & frequency. These functions keep phase intact for continuity.
        applyAmplitude(amp);
        updateForFrequencyAndTable(freq, newTableSize);

        Serial.println("ACK");
        String msg = "APPLIED freq=" + String(freq, 3) + " Hz amp=" + String(amp, 3) +
                     " V (0.." + String(amp,3) + " V) tableSize=" + String(currentTableSize) +
                     " interval_us=" + String(currentIntervalUs);
        Serial.println(msg);
        Serial.flush();
    } else {
        Serial.println("NACK");
        Serial.println("PARSE_ERROR");
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

    // Short sleep to yield CPU; ISR does the waveform generation
    delay(1);
}