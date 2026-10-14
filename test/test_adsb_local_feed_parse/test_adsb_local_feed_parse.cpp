#include <unity.h>

#include <ArduinoJson.h>

#include <cstdio>
#include <cstring>

// Mirrors services::adsb's filter + array-key fallback against a real capture from a
// local readsb-based feeder (an ADSBExchange feeder image's aircraft.json). Unlike
// adsb.fi, a raw readsb/dump1090-fa/tar1090 feed nests the array under "aircraft",
// not "ac" -- everything else (field names) is readsb-native on both sides.

static void buildAircraftFilter(JsonDocument& filter) {
  static const char* kKeepKeys[] = {
      "lat",          "lon",      "true_heading", "mag_heading", "track",
      "dir",          "gs",       "tas",          "ias",         "baro_rate",
      "geom_rate",    "alt_baro", "alt_geom",     "flight",      "hex",
      "t",            "r",        "dbFlags",      "category",    "squawk"};
  static const char* kArrayKeys[] = {"ac", "aircraft"};
  for (const char* array_key : kArrayKeys) {
    JsonObject el = filter[array_key].add<JsonObject>();
    for (const char* key : kKeepKeys) {
      el[key] = true;
    }
  }
}

// Mirrors parseAircraftDoc()'s "ac" then "aircraft" fallback.
static JsonArray pickAircraftArray(JsonDocument& doc) {
  JsonArray ac = doc["ac"].as<JsonArray>();
  if (ac.isNull()) {
    ac = doc["aircraft"].as<JsonArray>();
  }
  return ac;
}

static bool readJsonFloat(JsonObjectConst obj, const char* key, float* out) {
  if (obj[key].is<float>() || obj[key].is<double>() || obj[key].is<int>()) {
    *out = obj[key].as<float>();
    return true;
  }
  return false;
}

// Trimmed capture from a real ADSBExchange feeder image (readsb) aircraft.json --
// same "now"/"messages"/"aircraft" envelope every readsb/dump1090-fa/tar1090 install
// serves locally.
static const char kLocalFeedSample[] = R"({
  "now": 1785499880.294,
  "messages": 23800927,
  "aircraft": [
    {"hex":"744825","flight":"RJA113  ","alt_baro":33975,"gs":424.5,"track":291.85,
     "mag_heading":281.78,"true_heading":285.68,"baro_rate":64,"squawk":"3556",
     "category":"A3","lat":49.960094,"lon":8.50906},
    {"hex":"4bb143","flight":"THY3MJ  ","alt_baro":37000,"gs":492.9,"track":115.99,
     "mag_heading":120.59,"true_heading":124.49,"baro_rate":0,"squawk":"4755",
     "category":"A5","lat":49.305588,"lon":8.726525},
    {"hex":"39bdb8","flight":"AFR19DE ","alt_baro":35000,"gs":460.8,"track":115.17,
     "mag_heading":120.94,"true_heading":124.9,"baro_rate":32,"squawk":"1000",
     "category":"A3","lat":49.671936,"lon":8.894734}
  ]
})";

void test_local_feed_uses_aircraft_key(void) {
  JsonDocument filter;
  buildAircraftFilter(filter);
  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, kLocalFeedSample, DeserializationOption::Filter(filter));
  TEST_ASSERT_FALSE(err);

  // adsb.fi's key is absent; the local feed must fall back to "aircraft".
  TEST_ASSERT_TRUE(doc["ac"].isNull());

  JsonArray ac = pickAircraftArray(doc);
  TEST_ASSERT_FALSE(ac.isNull());
  TEST_ASSERT_EQUAL(3, ac.size());

  bool found_rja113 = false;
  for (JsonObjectConst plane : ac) {
    float lat = 0, lon = 0;
    TEST_ASSERT_TRUE(readJsonFloat(plane, "lat", &lat));
    TEST_ASSERT_TRUE(readJsonFloat(plane, "lon", &lon));
    const char* flight = plane["flight"].as<const char*>();
    TEST_ASSERT_NOT_NULL(flight);
    if (strncmp(flight, "RJA113", 6) == 0) {
      found_rja113 = true;
      float alt = 0;
      TEST_ASSERT_TRUE(readJsonFloat(plane, "alt_baro", &alt));
      TEST_ASSERT_EQUAL_FLOAT(33975.0f, alt);
      TEST_ASSERT_EQUAL_STRING("3556", plane["squawk"].as<const char*>());
    }
  }
  TEST_ASSERT_TRUE(found_rja113);
}

// A cloud (adsb.fi) response still parses the same way -- "ac" wins and "aircraft"
// is never consulted.
void test_cloud_feed_still_uses_ac_key(void) {
  static const char kCloudSample[] =
      R"({"ac":[{"hex":"aaa111","flight":"AIR1","lat":33.95,"lon":-118.40,"alt_baro":3500}]})";
  JsonDocument filter;
  buildAircraftFilter(filter);
  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, kCloudSample, DeserializationOption::Filter(filter));
  TEST_ASSERT_FALSE(err);

  JsonArray ac = pickAircraftArray(doc);
  TEST_ASSERT_FALSE(ac.isNull());
  TEST_ASSERT_EQUAL(1, ac.size());
  TEST_ASSERT_EQUAL_STRING("AIR1", ac[0]["flight"].as<const char*>());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_local_feed_uses_aircraft_key);
  RUN_TEST(test_cloud_feed_still_uses_ac_key);
  return UNITY_END();
}
