/**
 * Constants for use with smartWatering.ino
 */

// Capacitive moisture constants (based on this sensor, others may vary)
const int AIR = 720;
const int WATER = 285;

// Data collection element
struct sensVals {
  int temp;
  int humid;
  int soil;
};
