/**
 * @file BoardPins.h
 * @brief Example-only pin selection for the available ESP32-S3 motor bench.
 *
 * The user confirmed TX47, RX48 and DE21 on the board connected to COM13.
 * These are bench wiring facts, not a motor-library or product requirement.
 * See docs/hardware_bench.md for wiring provenance and qualification.
 * No GPIO or UART is configured by this header.
 */

#pragma once

namespace MotorControlRSExample {

/** @brief Selected ESP32-S3 bench wiring; replace for another application. */
struct Esp32S3BenchBoard {
  /// @brief MCU UART transmit pin, confirmed by the user for this bench.
  static constexpr int kRs485TxPin = 47;
  /// @brief MCU UART receive pin, confirmed by the user for this bench.
  static constexpr int kRs485RxPin = 48;
  /// @brief Combined transceiver direction pin, confirmed by the user.
  static constexpr int kRs485DeRePin = 21;
  /// @brief Bench transceiver wiring: high transmits, low receives.
  static constexpr bool kRs485DeReActiveHigh = true;
  /// @brief Wiring contract: combined DE/~RE disables the receiver during TX.
  /// The application rejects RX before physical TX completion and DE release.
  /// Set false for wiring whose local echo cannot be excluded; no analyzer is
  /// required to use this declared topology, and it is not measured timing.
  static constexpr bool kRs485ReceiverDisabledDuringTransmit = true;

  // Bench fixture outputs and input, user-identified on 16 November 2026 and
  // matching the CO2Control HW2.0.0 board contract (FieldCore, read-only).
  // All outputs are active high and held low by the board at reset. Physical
  // continuity, reset levels and wiring to the drive terminals are not
  // verified by this header; see docs/bench_io.md.
  /// @brief Relay 1 coil driver (DPDT dry contacts). Strapping pin.
  static constexpr int kRelay1Pin = 45;
  /// @brief Relay 2 coil driver (DPDT dry contacts).
  static constexpr int kRelay2Pin = 18;
  /// @brief Low-side MOSFET 1 gate. Strapping pin.
  static constexpr int kMosfet1Pin = 3;
  /// @brief Low-side MOSFET 2 gate. Strapping pin.
  static constexpr int kMosfet2Pin = 46;
  /// @brief Isolated AUX_DI1_24V input; an energized input reads low.
  static constexpr int kAux1InputPin = 17;
  static constexpr bool kAux1InputActiveLow = true;
};

/// @brief Explicit example bench selection; not part of the reusable library.
using Board = Esp32S3BenchBoard;

}  // namespace MotorControlRSExample
