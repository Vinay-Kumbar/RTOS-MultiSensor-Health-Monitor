# RTOS-based Multi-Sensor Task Scheduler

## What this project is

This is a simulated embedded system built on an ESP32, using FreeRTOS, that
reads multiple sensors at the same time, shows live status on a small OLED
screen, and reacts to an emergency button press instantly, no matter what
else is running. I built it entirely in Wokwi (a browser-based simulator)
since I didn't have physical hardware available.

The main idea was to actually implement the core concepts of RTOS-based
scheduling instead of just reading about them - task priorities, interrupts,
mutexes, and queues - and see them working in real time, with real sensor
components instead of just randomly generated numbers.

## Tools used

- Wokwi (wokwi.com) - simulates the ESP32 board and circuit in the browser
- FreeRTOS - comes built into the ESP32 Arduino core, no separate setup
- Arduino (C/C++)

All free, no paid tools involved.

## Hardware used (all simulated, no physical parts)

- ESP32 dev board
- DHT22 - real temperature sensor
- Potentiometer - stands in for a vibration sensor (turning the knob
  changes the "vibration" reading)
- Push button - emergency stop trigger
- SSD1306 OLED display - shows live temperature, vibration, and
  NORMAL/FAULT status directly on the "hardware" itself

## How the system is structured

There are 6 tasks running:

1. **Blink task** - blinks the onboard LED every 500ms, mainly to confirm
   the scheduler is alive.
2. **Temperature task** - reads the DHT22 every 3 seconds.
3. **Vibration task** - reads the potentiometer every 1 second and scales
   it to a 0-2g range.
4. **Processor task** - collects readings from the other two tasks and
   decides if things are normal or if there's a warning.
5. **Display task** - updates the OLED every second with the latest
   temperature, vibration, and status.
6. **Button task** - handles an emergency stop. This one has the highest
   priority so it can interrupt everything else immediately.

## How data moves through the system

The temperature and vibration tasks don't print anything themselves -
they just take a reading and drop it into a queue. Think of the queue
like a line - readings wait their turn in the order they arrive.

The processor task picks up one reading at a time from that queue, checks
it against a threshold (temperature above 40°C, or vibration above 1.2g),
and updates a shared status - which the display task then shows on the
OLED as either NORMAL or FAULT.

For the emergency button, I used an actual hardware interrupt instead of
just checking the button state in a loop. When the button is pressed, an
interrupt fires immediately and wakes up the button task using a
semaphore. Since that task has the highest priority, FreeRTOS pauses
whatever else is running and handles the emergency first, then goes back
to normal operation right where it left off.

One issue I ran into early on: when two tasks tried to print to the
Serial monitor at the same time, the text would get mixed up/garbled.
I fixed this by adding a mutex - basically a lock that only lets one
task print at a time, so nothing overlaps.

Another issue: the OLED screen stayed completely blank at first even
though it initialized without errors. Turned out I never set a text
color, so it was drawing black text on a black background. Adding
`display.setTextColor(SSD1306_WHITE)` fixed it.

## Why I made these choices

- I gave the processor task priority 2 (higher than the sensors, lower
  than the emergency button) because processing the data matters more
  than just collecting it, but nothing should ever be more important
  than the emergency stop.
- I used a semaphore for the button instead of polling it in a loop
  because polling wastes CPU cycles constantly checking something that
  rarely changes. With an interrupt, the task just sleeps until it's
  actually needed.
- I used a potentiometer instead of a real accelerometer for vibration
  since it's simple to wire and lets me manually simulate different
  vibration levels live during a demo just by turning the knob.

## What I'd add if I continued this

- Swap the potentiometer for a proper simulated accelerometer (ADXL345)
- Track memory/CPU usage per task for optimization
- Send fault alerts to a second device or a simple web dashboard instead
  of just the OLED and serial monitor

## How to run it

1. Go to wokwi.com/projects/new/esp32
2. Paste in the sketch.ino and diagram.json files from this project
3. Add a libraries.txt file with: DHT sensor library for ESPx, Adafruit
   SSD1306, Adafruit GFX Library
4. Hit the play button to start the simulation
5. Watch the readings come in on the OLED and serial monitor, turn the
   potentiometer knob to trigger a FAULT, and press the button to see
   the emergency interrupt in action
