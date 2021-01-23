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

#include "proc_wideband_spectrum.hpp"

#include "event_m4.hpp"

#include <cstdint>
#include <cstddef>

#include <array>

void WidebandSpectrum::execute(const buffer_c8_t& buffer) {
	// 2048 complex8_t samples per buffer.
	// 102.4us per buffer. 20480 instruction cycles per buffer.
	
	if (!configured) return;

	if( phase == 0 ) {
		std::fill(spectrum.begin(), spectrum.end(), 0);
	}

	// Combine all buffer data.
	for(size_t k = 0; k < 2048 / spectrum.size(); k++) {
		for(size_t i = 0; i < spectrum.size(); i++) {
			// TODO: Removed window-presum windowing, due to lack of available code RAM.
			// TODO: Apply window to improve spectrum bin sidelobes.
			spectrum[i] += buffer.p[i + spectrum.size() * k];
		}
	}

	if( phase == trigger ) {
		// Calculate DC offset
		offset.real(0);
		offset.imag(0);
		for(size_t i = 0; i < spectrum.size(); i++) {
			offset.real(offset.real() + spectrum[i].real());
			offset.imag(offset.imag() + spectrum[i].imag());
		}

		offset.real(offset.real() / spectrum.size());
		offset.imag(offset.imag() / spectrum.size());

		for(size_t i=0; i<spectrum.size(); i++) {
			spectrum[i].real((spectrum[i].real() - offset.real()) * sp_gain);
			spectrum[i].imag((spectrum[i].imag() - offset.imag()) * sp_gain);
		}
		const buffer_c16_t buffer_c16_s {
			spectrum.data(),
			spectrum.size(),
			buffer.sampling_rate / 20 / (trigger + 1)
		};
		const buffer_c16_t buffer_c16 {
			spectrum.data(),
			spectrum.size(),
			buffer.sampling_rate
		};
		feed_channel_stats(buffer_c16_s);
		channel_spectrum.feed(
			buffer_c16,
			0, 0, 0
		);
		phase = 0;
	} else {
		phase++;
	}
}

void WidebandSpectrum::on_message(const Message* const msg) {
	const WidebandSpectrumConfigMessage message = *reinterpret_cast<const WidebandSpectrumConfigMessage*>(msg);
	
	switch(msg->id) {
	case Message::ID::UpdateSpectrum:
	case Message::ID::SpectrumStreamingConfig:
		channel_spectrum.on_message(msg);
		break;
		
	case Message::ID::WidebandSpectrumConfig:
		baseband_fs = message.sampling_rate;
		trigger = message.trigger;
		baseband_thread.set_sampling_rate(baseband_fs);
		phase = 0;
		configured = true;
		break;

	default:
		break;
	}
}

int main() {
	EventDispatcher event_dispatcher { std::make_unique<WidebandSpectrum>() };
	event_dispatcher.run();
	return 0;
}
