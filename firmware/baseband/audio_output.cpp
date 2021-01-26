/*
 * Copyright (C) 2014 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2016 Furrtek
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

#include "audio_output.hpp"

#include "portapack_shared_memory.hpp"

#include "audio_dma.hpp"

#include "message.hpp"

#include <cstdint>
#include <cstddef>
#include <array>

void AudioOutput::configure(
	const bool do_proc
) {
	do_processing = do_proc;
}

void AudioOutput::configure(
	const iir_biquad_config_t& hpf_config,
	const iir_biquad_config_t& deemph_config,
	const float squelch_threshold
) {
	hpf.configure(hpf_config);
	deemph.configure(deemph_config);
	squelch.set_threshold(squelch_threshold);
}

// Mono

void AudioOutput::write(
	const buffer_s16_t& audio
) {
	std::array<float, 32> audio_f;
	for(size_t i=0; i<audio.count; i++) {
		audio_f[i] = audio.p[i] * ki;
	}
	write(buffer_f32_t {
		audio_f.data(),
		audio.count,
		audio.sampling_rate
	});
}

void AudioOutput::write(
	const buffer_f32_t& audio
) {
	block_buffer.feed(
		audio,
		[this](const buffer_f32_t& buffer) {
			this->on_block(buffer);
		}
	);
}

void AudioOutput::on_block(
	const buffer_f32_t& audio
) {
	if (do_processing) {
		const auto audio_present_now = squelch.execute(audio);

		hpf.execute_in_place(audio);
		deemph.execute_in_place(audio);

		audio_present_history = (audio_present_history << 1) | (audio_present_now ? 1 : 0);
		audio_present = (audio_present_history != 0);
		
		if( !audio_present ) {
			for(size_t i=0; i<audio.count; i++) {
				audio.p[i] = 0;
			}
		}
	} else
		audio_present = true;

	fill_audio_buffer(audio, audio_present);
}

bool AudioOutput::is_squelched() {
	return !audio_present;
}

void AudioOutput::fill_audio_buffer(const buffer_f32_t& audio, const bool send_to_fifo) {
	std::array<int16_t, 32> audio_int;

	auto audio_buffer = audio::dma::tx_empty_buffer();
	for(size_t i=0; i<audio_buffer.count; i++) {
		const int32_t sample_int = audio.p[i] * k;
		const int32_t sample_saturated = __SSAT(sample_int, 16);
		audio_buffer.p[i].left = audio_buffer.p[i].right = sample_saturated;
		audio_int[i] = sample_saturated;
	}
	if( stream && send_to_fifo ) {
		stream->write(audio_int.data(), audio_buffer.count * sizeof(audio_int[0]));
	}

	feed_audio_stats(audio);
}

// Stereo

void AudioOutput::write(
	const buffer_s16_t& audio_left,
	const buffer_s16_t& audio_right
) {
	std::array<float, 32> audio_fl;
	std::array<float, 32> audio_fr;
	for(size_t i=0; i<audio_left.count; i++) {
		audio_fl[i] = audio_left.p[i] * ki;
		audio_fr[i] = audio_right.p[i] * ki;
	}
	write(buffer_f32_t {
		audio_fl.data(),
		audio_left.count,
		audio_left.sampling_rate
	},
	buffer_f32_t {
		audio_fr.data(),
		audio_right.count,
		audio_right.sampling_rate
	});
}

void AudioOutput::write(
	const buffer_f32_t& audio_left,
	const buffer_f32_t& audio_right
) {
	// Dont use buffer. Buffer messes things up.
	on_block(audio_left, audio_right);
}

void AudioOutput::on_block(
	const buffer_f32_t& audio_left,
	const buffer_f32_t& audio_right
) {
	if (do_processing) {
		const auto audio_present_now = squelch.execute(audio_left);

		hpf.execute_in_place(audio_left);
		hpf.execute_in_place(audio_right);
		deemph.execute_in_place(audio_left);
		deemph.execute_in_place(audio_right);

		audio_present_history = (audio_present_history << 1) | (audio_present_now ? 1 : 0);
		audio_present = (audio_present_history != 0);
		
		if( !audio_present ) {
			for(size_t i=0; i<audio_left.count; i++) {
				audio_left.p[i] = 0;
				audio_right.p[i] = 0;
			}
		}
	} else
		audio_present = true;

	fill_audio_buffer(audio_left, audio_right, audio_present);
}

void AudioOutput::fill_audio_buffer(const buffer_f32_t& audio_left, const buffer_f32_t& audio_right, const bool send_to_fifo) {
	std::array<int16_t, 32> audio_int;

	auto audio_buffer = audio::dma::tx_empty_buffer();
	for(size_t i=0; i<audio_buffer.count; i++) {
		const int32_t sample_intl = audio_left.p[i] * k;
		const int32_t sample_saturatedl = __SSAT(sample_intl, 16);
		const int32_t sample_intr = audio_right.p[i] * k;
		const int32_t sample_saturatedr = __SSAT(sample_intr, 16);
		audio_buffer.p[i].left = sample_saturatedr;
		audio_buffer.p[i].right = sample_saturatedl;
		audio_int[i] = (sample_saturatedl + sample_saturatedr) / 2;
	}
	if( stream && send_to_fifo ) {
		stream->write(audio_int.data(), audio_buffer.count * sizeof(audio_int[0]));
	}

	feed_audio_stats(audio_left);
}

// Direct

void AudioOutput::write_direct(const buffer_s16_t& audio_left, const buffer_s16_t& audio_right) {
	std::array<float, 32> audio_float;

	auto audio_buffer = audio::dma::tx_empty_buffer();
	for(size_t i=0; i<audio_buffer.count; i++) {
		audio_buffer.p[i].left  = audio_right.p[i];
		audio_buffer.p[i].right = audio_left.p[i];
		audio_float[i] = ((int32_t)audio_left.p[i] + (int32_t)audio_right.p[i]) / 2 * ki;
	}
	if( stream ) {
		stream->write(audio_left.p, audio_left.count * sizeof(audio_left.p[0]));
	}

	feed_audio_stats(buffer_f32_t{audio_float.data(), audio_left.count, audio_left.sampling_rate});
}

void AudioOutput::feed_audio_stats(const buffer_f32_t& audio) {
	audio_stats.feed(
		audio,
		[](const AudioStatistics& statistics) {
			const AudioStatisticsMessage audio_stats_message { statistics };
			shared_memory.application_queue.push(audio_stats_message);
		}
	);
}
