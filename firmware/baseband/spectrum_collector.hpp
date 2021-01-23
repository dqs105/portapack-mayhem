/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __SPECTRUM_COLLECTOR_H__
#define __SPECTRUM_COLLECTOR_H__

#include "dsp_types.hpp"
#include "complex.hpp"

#include "block_decimator.hpp"

#include <cstdint>
#include <array>

#include "message.hpp"

template<typename T>
static typename T::value_type spectrum_window_none(const T& s, const size_t i) {
	static_assert(power_of_two(s.size()), "Array size must be power of 2");
	return s[i];
};

template<typename T>
static typename T::value_type spectrum_window_hamming_3(const T& s, const size_t i) {
	static_assert(power_of_two(s.size()), "Array size must be power of 2");
	constexpr size_t mask = s.size() - 1;
	// Three point Hamming window.
	return s[i] * 0.54f + (s[(i-1) & mask] + s[(i+1) & mask]) * -0.23f;
};

template<typename T>
static typename T::value_type spectrum_window_blackman_3(const T& s, const size_t i) {
	static_assert(power_of_two(s.size()), "Array size must be power of 2");
	constexpr size_t mask = s.size() - 1;
	// Three term Blackman window.
	constexpr float alpha = 0.42f;
	constexpr float beta = 0.5f * 0.5f;
	constexpr float gamma = 0.08f * 0.05f;
	return s[i] * alpha - (s[(i-1) & mask] + s[(i+1) & mask]) * beta + (s[(i-2) & mask] + s[(i+2) & mask]) * gamma;
};

class SpectrumCollector {
public:
	void on_message(const Message* const message);

	void set_decimation_factor(const size_t decimation_factor);

	void feed(
		const buffer_c16_t& channel,
		const uint32_t filter_pass_frequency,
		const uint32_t filter_stop_frequency
	);

private:
	BlockDecimator<complex16_t, 256> channel_spectrum_decimator { 1 };
	ChannelSpectrum fifo_data[1 << ChannelSpectrumConfigMessage::fifo_k] { };
	ChannelSpectrumFIFO fifo { fifo_data, ChannelSpectrumConfigMessage::fifo_k };

	volatile bool channel_spectrum_request_update { false };
	bool streaming { false };
	std::array<std::complex<float>, 256> channel_spectrum { };
	uint32_t channel_spectrum_sampling_rate { 0 };
	uint32_t channel_filter_pass_frequency { 0 };
	uint32_t channel_filter_stop_frequency { 0 };

	void post_message(const buffer_c16_t& data);

	void set_state(const SpectrumStreamingConfigMessage& message);
	void start();
	void stop();

	void update();
};

#endif/*__SPECTRUM_COLLECTOR_H__*/
