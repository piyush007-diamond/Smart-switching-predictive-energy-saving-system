# Predictive Smart Energy Saving System

## Project Overview
This repository contains the hardware design, schematic logic, and firmware (ESP32) for a Predictive Smart Energy Saving System. Unlike standard smart plugs that rely on manual app control or simple timers, this system features a machine learning/heuristic approach. 

The system passively monitors a user's appliance switching habits during a dedicated "Training Phase." Once sufficient data is collected, it transitions to an autonomous mode, predicting when an appliance has been left on unnecessarily and automatically cutting the power to conserve energy.

## The Hardware Problem: Safe State Detection & Override
To make a predictive system work in the real world, the microcontroller needs to know two things: 
1. Is there power available from the grid? (Line-side voltage)
2. Is the user's physical switch turned ON? (Load-side voltage)

Standard smart relays simply replace the physical switch. This project implements a much more robust **Selector System**, allowing the ESP32 to measure voltages on *both* sides of the physical switch safely and act accordingly.

## Hardware Architecture & Component Selection

### 1. The Core Controller (ESP32)
The ESP32 was selected for its dual-core processing power and built-in Wi-Fi capabilities. It handles the data logging during the training phase, runs the predictive pattern-recognition algorithm, and actuates the relays.

### 2. Isolated Switch State Detection (Optocoupler & Power Resistors)
To safely detect 230V AC mains without frying the 3.3V logic of the ESP32, the system uses an optocoupler.
* **Power Resistor Network:** A network of high-wattage power resistors is used to safely step down the high AC grid voltage. This limits the current to a safe level (typically a few milliamps) before it enters the optocoupler.
* **Optocoupler:** The stepped-down voltage drives the internal LED of the optocoupler. The phototransistor on the other side then toggles a clean 3.3V GPIO signal on the ESP32. This provides complete galvanic isolation between the high-voltage mains and the low-voltage microcontroller.

### 3. The Relay Selector System
This is the core innovation of the hardware design. Instead of using multiple expensive sensors, a relay is used as an intelligent multiplexer (selector).
* The relay toggles the measurement circuit between the "before switch" (Line) terminal and the "after switch" (Load) terminal. 
* By sampling the voltage on both sides of the switch at different times, the ESP32 can deduce if the grid is live, if the mechanical switch is closed, and if power is actually flowing to the appliance. 
* A secondary power relay is placed in series with the load. If the predictive algorithm determines the device should be off, this relay opens the circuit, overriding the physical wall switch.

### 4. Energy Metering
A dedicated energy metering module monitors the actual current draw and wattage. This data serves two purposes:
* **Validation:** It confirms that the appliance is actually consuming power (distinguishing between a switch being flipped ON and an appliance actively running, like a thermostat-controlled heater).
* **Analytics:** It logs the total energy consumed and calculates the energy saved by the predictive algorithm overriding the system.

## System Operation Workflow

### Phase 1: Training & Data Collection
When first installed, the ESP32 relay remains closed (transparent to the user). The system acts purely as a data logger. It records the exact timestamps of when the user turns the appliance on and off, cross-referencing this with the energy meter data to establish a baseline of normal usage patterns.

### Phase 2: Autonomous Prediction
After gathering sufficient usage data, the system builds a probabilistic model of the appliance's usage. 
* If the switch remains ON during a time window where the data strongly suggests it should be OFF (e.g., an office light left on at 2:00 AM), the system flags it as an anomaly.
* The ESP32 triggers the main power relay to cut the circuit, saving energy.
* The system continues to monitor the mechanical switch state via the optocoupler so that if the user manually toggles the switch, the ESP32 can instantly restore power and register the manual override to update its training model.



## Setup & Installation
1. Flash the ESP32 firmware using the Arduino IDE or PlatformIO.
2. Follow the schematics in the `/hardware` folder to wire the optocoupler and resistor network. **Warning: This project involves high-voltage AC mains. Ensure all connections are secure and properly isolated before applying power.**
3. Power the system; the ESP32 will immediately enter the initial Training Phase.# Smart-switching-predictive-energy-saving-system
