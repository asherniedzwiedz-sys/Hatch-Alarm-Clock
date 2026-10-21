// Host tests for src/Logic.h:  pio test -e native
#include <unity.h>

#include "Logic.h"
#include "SleepLogic.h"
#include "SoundSynth.h"

#include <stdio.h>

// 2026-10-19 is a Monday. Tests use UTC as "local" time.
static const int64_t MON_0700 = (int64_t)daysFromCivil(2026, 10, 19) * DAY_SEC + 7 * 3600;
static const uint8_t WEEKDAYS = 0x3E;  // Mon-Fri

static int wdayOf(int64_t t) { return (int)((t / DAY_SEC + 4) % 7); }  // 1970-01-01 was a Thursday
static int32_t secOf(int64_t t) { return (int32_t)(t % DAY_SEC); }
static void tickAt(AlarmMachine& m, int64_t t, Alarm* al, int n) { m.tick(t, wdayOf(t), secOf(t), al, n); }
static void assertPhase(Phase want, const AlarmMachine& m) { TEST_ASSERT_EQUAL_INT((int)want, (int)m.phase); }

void setUp() {}
void tearDown() {}

void test_sunrise_curve() {
  TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.2176f, sunriseLevel(15.0f / 30.0f));  // t = 15 min -> ~22 %
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sunriseLevel(0.0f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, sunriseLevel(1.0f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, sunriseLevel(1.5f));
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 1700.0f, sunriseKelvin(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 2450.0f, sunriseKelvin(0.5f));
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 3200.0f, sunriseKelvin(1.0f));
}

void test_kelvin_to_rgb() {
  RGB c = kelvinToRGB(6600);
  TEST_ASSERT_EQUAL_UINT8(255, c.r);
  TEST_ASSERT_EQUAL_UINT8(255, c.g);
  TEST_ASSERT_EQUAL_UINT8(255, c.b);
  c = kelvinToRGB(1700);  // candle: red + some green, no blue
  TEST_ASSERT_EQUAL_UINT8(255, c.r);
  TEST_ASSERT_UINT8_WITHIN(2, 121, c.g);
  TEST_ASSERT_EQUAL_UINT8(0, c.b);
  c = kelvinToRGB(2700);  // incandescent warm white
  TEST_ASSERT_UINT8_WITHIN(2, 167, c.g);
  TEST_ASSERT_UINT8_WITHIN(2, 87, c.b);
  uint8_t lastB = 0;
  for (int k = 2000; k <= 6500; k += 100) {  // cooler = bluer, monotonically
    TEST_ASSERT_TRUE(kelvinToRGB((float)k).b >= lastB);
    lastB = kelvinToRGB((float)k).b;
  }
}

void test_sunrise_ring_snooze_dismiss() {
  Alarm al[1] = {{7, 0, WEEKDAYS, true}};
  AlarmMachine m;
  tickAt(m, MON_0700 - 31 * 60, al, 1);
  assertPhase(Phase::Idle, m);
  tickAt(m, MON_0700 - 15 * 60, al, 1);
  assertPhase(Phase::Sunrise, m);
  TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.2176f, sunriseLevel(m.progress));
  tickAt(m, MON_0700, al, 1);
  assertPhase(Phase::Ringing, m);
  m.snooze(MON_0700 + 60);
  assertPhase(Phase::Snoozed, m);
  tickAt(m, MON_0700 + 60 + 9 * 60 - 1, al, 1);
  assertPhase(Phase::Snoozed, m);
  tickAt(m, MON_0700 + 60 + 9 * 60, al, 1);
  assertPhase(Phase::Ringing, m);
  m.dismiss(al, 1);
  assertPhase(Phase::Idle, m);
  TEST_ASSERT_FALSE(m.timedOut);
  tickAt(m, MON_0700 + 11 * 60, al, 1);  // still inside the ring window: must not come back
  assertPhase(Phase::Idle, m);
  TEST_ASSERT_TRUE(al[0].enabled);  // repeating alarms stay on
  tickAt(m, MON_0700 + DAY_SEC - 10 * 60, al, 1);  // Tuesday's sunrise
  assertPhase(Phase::Sunrise, m);
}

void test_auto_fade_out_and_weekend() {
  Alarm al[1] = {{7, 0, WEEKDAYS, true}};
  AlarmMachine m;
  tickAt(m, MON_0700 + 5, al, 1);
  assertPhase(Phase::Ringing, m);
  tickAt(m, MON_0700 + 20 * 60 - 1, al, 1);
  assertPhase(Phase::Ringing, m);
  tickAt(m, MON_0700 + 20 * 60, al, 1);
  assertPhase(Phase::Idle, m);
  TEST_ASSERT_TRUE(m.timedOut);
  const int64_t sat = MON_0700 + 5 * DAY_SEC;
  tickAt(m, sat - 10 * 60, al, 1);
  assertPhase(Phase::Idle, m);
  tickAt(m, sat, al, 1);
  assertPhase(Phase::Idle, m);
}

void test_power_loss_resume_and_one_shot() {
  Alarm al[1] = {{7, 0, 0, true}};  // one-shot
  AlarmMachine m;                    // fresh boot, handledUpTo read back as 0
  tickAt(m, MON_0700 + 5 * 60, al, 1);
  assertPhase(Phase::Ringing, m);  // power came back mid-alarm: keep ringing
  TEST_ASSERT_TRUE(m.until == MON_0700 + 20 * 60);  // auto fade-out keeps the original schedule
  m.dismiss(al, 1);
  TEST_ASSERT_FALSE(al[0].enabled);  // one-shot disables itself
  TEST_ASSERT_TRUE(m.dirty);
  tickAt(m, MON_0700 + DAY_SEC, al, 1);
  assertPhase(Phase::Idle, m);
}

void test_edit_does_not_fire_past_alarm() {
  Alarm al[1] = {{7, 0, 0x7F, true}};
  AlarmMachine m;
  m.edited(MON_0700 + 5 * 60);  // saved at 07:05
  tickAt(m, MON_0700 + 5 * 60, al, 1);
  assertPhase(Phase::Idle, m);
  AlarmMachine m2;
  m2.edited(MON_0700 - 10 * 60);  // saved at 06:50: already inside the sunrise window
  tickAt(m2, MON_0700 - 10 * 60, al, 1);
  assertPhase(Phase::Sunrise, m2);
}

void test_dismiss_during_sunrise_cancels_ring() {
  Alarm al[2] = {{7, 0, 0x7F, true}, {7, 10, 0x7F, true}};
  AlarmMachine m;
  tickAt(m, MON_0700 - 60, al, 2);
  assertPhase(Phase::Sunrise, m);
  TEST_ASSERT_EQUAL_INT(0, m.index);
  m.dismiss(al, 2);
  tickAt(m, MON_0700, al, 2);  // 07:00 cancelled, 07:10 now in its sunrise
  assertPhase(Phase::Sunrise, m);
  TEST_ASSERT_EQUAL_INT(1, m.index);
}

void test_next_alarm() {
  Alarm al[3] = {{7, 0, WEEKDAYS, true}, {9, 30, 0x41, true}, {6, 0, 0x7F, false}};  // 0x41 = Sat+Sun
  int32_t off = 0;
  const int64_t sun2300 = MON_0700 - 8 * 3600;
  TEST_ASSERT_EQUAL_INT(0, findAlarm(al, 3, wdayOf(sun2300), secOf(sun2300), 0, WEEK_SEC, true, off));
  TEST_ASSERT_EQUAL_INT32(8 * 3600, off);
  TEST_ASSERT_EQUAL_INT(2, findAlarm(al, 3, wdayOf(sun2300), secOf(sun2300), 0, WEEK_SEC, false, off));
  const int64_t fri0800 = MON_0700 + 4 * DAY_SEC + 3600;
  TEST_ASSERT_EQUAL_INT(1, findAlarm(al, 3, wdayOf(fri0800), secOf(fri0800), 0, WEEK_SEC, true, off));
  TEST_ASSERT_EQUAL_INT32(DAY_SEC + 90 * 60, off);  // Saturday 09:30
}

void test_alarm_json() {
  Alarm in[2] = {{6, 45, WEEKDAYS, true}, {9, 0, 0, false}}, out[MAX_ALARMS];
  JsonDocument doc;
  alarmsToJson(in, 2, doc.to<JsonArray>());
  TEST_ASSERT_EQUAL_INT(2, alarmsFromJson(doc.as<JsonArrayConst>(), out, MAX_ALARMS));
  TEST_ASSERT_EQUAL_UINT8(45, out[0].minute);
  TEST_ASSERT_EQUAL_UINT8(WEEKDAYS, out[0].days);
  TEST_ASSERT_FALSE(out[1].enabled);

  const char* bad[] = {
      "[{\"hour\":24,\"minute\":0}]",        "[{\"hour\":7,\"minute\":60}]",
      "[{\"hour\":7,\"minute\":0,\"days\":128}]", "[{\"hour\":\"7\",\"minute\":0}]",
      "[{\"hour\":7,\"minute\":0,\"enabled\":1}]", "[7]",
      "{\"hour\":7}",                        "[{},{},{},{},{},{},{},{},{},{},{}]",
  };
  for (const char* s : bad) {
    TEST_ASSERT_FALSE(deserializeJson(doc, s));
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, alarmsFromJson(doc.as<JsonArrayConst>(), out, MAX_ALARMS), s);
  }
  TEST_ASSERT_FALSE(deserializeJson(doc, "[{\"hour\":7,\"minute\":5}]"));
  TEST_ASSERT_EQUAL_INT(1, alarmsFromJson(doc.as<JsonArrayConst>(), out, MAX_ALARMS));
  TEST_ASSERT_TRUE(out[0].enabled);  // defaults: enabled, one-shot
  TEST_ASSERT_EQUAL_UINT8(0, out[0].days);
}

void test_days_from_civil() {
  TEST_ASSERT_EQUAL_INT32(0, daysFromCivil(1970, 1, 1));
  TEST_ASSERT_EQUAL_INT32(19723, daysFromCivil(2024, 1, 1));  // 1704067200 / 86400
  TEST_ASSERT_EQUAL_INT32(19782, daysFromCivil(2024, 2, 29));
  TEST_ASSERT_EQUAL_INT32(19783, daysFromCivil(2024, 3, 1));
  TEST_ASSERT_EQUAL_INT(1, wdayOf(MON_0700));
}

void test_render_clock() {
  uint16_t rows[8];
  renderClock(7, 5, false, rows);  // " 7:05"
  TEST_ASSERT_EQUAL_HEX16(0x7770, rows[1]);  // top row: blank, 7, 0, 5
  TEST_ASSERT_EQUAL_HEX16(0x15C0, rows[2]);  // second row, colon in column 7
  TEST_ASSERT_EQUAL_HEX16(0, rows[0] | rows[6] | rows[7]);
  renderClock(0, 0, false, rows);  // midnight shows 12:00
  TEST_ASSERT_EQUAL_HEX16(0x0002, rows[1] & 0x7);
  renderClock(13, 0, false, rows);  // 1:00 pm: no leading digit
  TEST_ASSERT_EQUAL_HEX16(0, rows[1] & 0x7);
  renderClock(9, 30, true, rows);  // 24 h keeps the leading zero
  TEST_ASSERT_EQUAL_HEX16(0x7, rows[1] & 0x7);
  renderClock(-1, 0, true, rows);  // no valid time: --:--
  TEST_ASSERT_EQUAL_HEX16(0x7777, rows[3]);
  TEST_ASSERT_EQUAL_HEX16(0, rows[1]);
}

static int feedAll(Ld2410Parser& p, const uint8_t* b, size_t n, RadarReport& r) {
  int frames = 0;
  for (size_t i = 0; i < n; i++) frames += p.feed(b[i], r);
  return frames;
}

void test_ld2410_parser() {
  // basic frame: moving + still target, move 120 cm / energy 42, still 150 cm / energy 5, detect 300 cm
  const uint8_t frame[] = {0xF4, 0xF3, 0xF2, 0xF1, 0x0D, 0x00, 0x02, 0xAA, 0x03, 0x78, 0x00, 0x2A,
                           0x96, 0x00, 0x05, 0x2C, 0x01, 0x55, 0x00, 0xF8, 0xF7, 0xF6, 0xF5};
  const uint8_t noise[] = {0x00, 0xF4, 0xF3, 0x11, 0xF4};  // garbage and a false start
  Ld2410Parser p;
  RadarReport r = {};
  TEST_ASSERT_EQUAL_INT(0, feedAll(p, noise, sizeof noise, r));
  TEST_ASSERT_EQUAL_INT(1, feedAll(p, frame, sizeof frame, r));
  TEST_ASSERT_EQUAL_UINT8(3, r.state);
  TEST_ASSERT_EQUAL_UINT16(120, r.moveCm);
  TEST_ASSERT_EQUAL_UINT8(42, r.moveEnergy);
  TEST_ASSERT_EQUAL_UINT16(150, r.stillCm);
  TEST_ASSERT_EQUAL_UINT8(5, r.stillEnergy);
  TEST_ASSERT_TRUE(inBed(r, 200));
  TEST_ASSERT_FALSE(inBed(r, 100));

  uint8_t bad[sizeof frame];
  memcpy(bad, frame, sizeof frame);
  bad[sizeof bad - 1] = 0x00;  // broken footer
  TEST_ASSERT_EQUAL_INT(0, feedAll(p, bad, sizeof bad, r));
  TEST_ASSERT_EQUAL_INT(1, feedAll(p, frame, sizeof frame, r));  // and recovers

  uint8_t eng[6 + 35 + 4] = {0xF4, 0xF3, 0xF2, 0xF1, 35, 0, 0x01, 0xAA, 0x02, 0, 0, 0, 0x64, 0x00, 0x30};
  eng[6 + 33] = 0x55;
  memcpy(eng + 6 + 35, frame + 19, 4);  // footer
  TEST_ASSERT_EQUAL_INT(1, feedAll(p, eng, sizeof eng, r));  // engineering mode
  TEST_ASSERT_EQUAL_UINT8(2, r.state);
  TEST_ASSERT_EQUAL_UINT16(100, r.stillCm);
}

struct NightSim {
  SleepTracker t;
  int64_t now;
  int nights = 0;
  Night last = {};
  void run(int minutes, bool present, uint8_t motion) {
    for (int i = 0; i < minutes * 60; i++, now++)
      if (t.second(now, (int)(now % DAY_SEC / 3600), present, motion)) {
        nights++;
        last = t.night;
      }
  }
};

void test_sleep_night() {
  NightSim s;
  s.now = MON_0700 + 14 * 3600;  // Monday 21:00
  s.run(60, false, 0);           // room empty
  s.run(15, true, 40);           // 22:00 in bed, reading
  TEST_ASSERT_TRUE(s.t.inSession);
  s.run(225, true, 3);  // 22:15 still -> asleep
  s.run(10, true, 50);  // 02:00 restless 10 min (awake)
  s.run(260, true, 3);  // 02:10 back to sleep
  s.run(20, false, 0);  // 06:30 up
  TEST_ASSERT_EQUAL_INT(1, s.nights);
  const Night& n = s.last;
  TEST_ASSERT_TRUE(n.start == MON_0700 + 15 * 3600);       // backdated to 22:00
  TEST_ASSERT_TRUE(n.end == MON_0700 + 23 * 3600 + 1800);  // 06:30, not when the 15 min away ran out
  TEST_ASSERT_EQUAL_UINT16(510, n.inBedMin);
  TEST_ASSERT_EQUAL_UINT16(19, n.latencyMin);
  TEST_ASSERT_EQUAL_UINT16(478, n.asleepMin);
  TEST_ASSERT_EQUAL_UINT16(32, n.awakeMin);
  TEST_ASSERT_EQUAL_UINT8(1, n.wakeups);  // getting up at 06:30 is not a wake-up
  TEST_ASSERT_EQUAL_UINT8(102, n.buckets);
  TEST_ASSERT_EQUAL_UINT8(SLEEP_AWAKE, n.stage[0]);
  TEST_ASSERT_EQUAL_UINT8(SLEEP_ASLEEP, n.stage[10]);
  TEST_ASSERT_EQUAL_UINT8(SLEEP_ASLEEP, n.stage[101]);
  JsonDocument doc;
  nightToJson(n, doc.to<JsonObject>());
  TEST_ASSERT_EQUAL_INT(102, (int)strlen(doc["stages"] | ""));
  TEST_ASSERT_EQUAL_INT(478, doc["asleep"] | 0);
}

void test_sleep_ignores_naps_and_short_sessions() {
  NightSim s;
  s.now = MON_0700 + DAY_SEC + 7 * 3600;  // Tuesday 14:00
  s.run(90, true, 3);                     // afternoon nap: outside the bedtime window
  TEST_ASSERT_FALSE(s.t.inSession);
  s.run(9 * 60, false, 0);  // 15:30 -> 00:30
  s.run(40, true, 3);       // in bed 40 min, then gone
  s.run(20, false, 0);
  TEST_ASSERT_FALSE(s.t.inSession);
  TEST_ASSERT_EQUAL_INT(0, s.nights);  // too short to keep
}

void test_sound_synth() {
  TEST_ASSERT_TRUE(synthByName("rain") == SynthSound::Rain);
  TEST_ASSERT_TRUE(synthByName("rain.wav") == SynthSound::None);  // SD files stay files
  for (int k = 1; k <= 5; k++) {
    NoiseSynth n;
    n.start((SynthSound)k);
    double sum = 0, sq = 0;
    long clipped = 0;
    const long N = 44100L * 30;  // 30 s: several ocean waves
    for (long i = 0; i < N; i++) {
      const int v = n.next();
      sum += v;
      sq += (double)v * v;
      clipped += v >= 32000 || v <= -32000;
    }
    const double rms = sqrt(sq / N) / 32767.0, mean = sum / N / 32767.0;
    char msg[96];
    snprintf(msg, sizeof msg, "%s: rms %.3f (%.1f dBFS) mean %.4f clipped %ld", SYNTH_NAMES[k - 1], rms, 20 * log10(rms), mean, clipped);
    TEST_MESSAGE(msg);
    TEST_ASSERT_TRUE_MESSAGE(rms > 0.08 && rms < 0.125, msg);  // -20 dBFS +-2 dB each, like the WAVs
    TEST_ASSERT_TRUE_MESSAGE(fabs(mean) < 0.02, msg);         // no DC offset
    TEST_ASSERT_TRUE_MESSAGE(clipped < N / 1000, msg);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sunrise_curve);
  RUN_TEST(test_kelvin_to_rgb);
  RUN_TEST(test_sunrise_ring_snooze_dismiss);
  RUN_TEST(test_auto_fade_out_and_weekend);
  RUN_TEST(test_power_loss_resume_and_one_shot);
  RUN_TEST(test_edit_does_not_fire_past_alarm);
  RUN_TEST(test_dismiss_during_sunrise_cancels_ring);
  RUN_TEST(test_next_alarm);
  RUN_TEST(test_alarm_json);
  RUN_TEST(test_days_from_civil);
  RUN_TEST(test_render_clock);
  RUN_TEST(test_ld2410_parser);
  RUN_TEST(test_sleep_night);
  RUN_TEST(test_sleep_ignores_naps_and_short_sessions);
  RUN_TEST(test_sound_synth);
  return UNITY_END();
}
