/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
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

#ifndef __PROC_AUDIOTX_H__
#define __PROC_AUDIOTX_H__

#include "baseband_processor.hpp"
#include "baseband_thread.hpp"
#include "tone_gen.hpp"
#include "stream_output.hpp"

#include "audio_output.hpp"

class AudioTXProcessor : public BasebandProcessor {
public:
	void execute(const buffer_c8_t& buffer) override;
	
	void on_message(const Message* const msg) override;

private:
	size_t baseband_fs = 1536000;
	static constexpr size_t baseband_fs_base = 1536000;
	static constexpr uint8_t audio_decimation_factor = baseband_fs_base / 48000;
	
	BasebandThread baseband_thread { baseband_fs_base, this, NORMALPRIO + 20, baseband::Direction::Transmit };
	
	std::unique_ptr<StreamOutput> stream { };
	
	ToneGen tone_gen { };
	
	uint32_t resample_inc { }, resample_acc { };
	uint8_t mod_type { 0 };
	uint32_t fm_delta { 0 };
	uint32_t phase { 0 }, sphase { 0 };
	int32_t sample { 0 }, delta { };
	int8_t re { 0 }, im { 0 };

	int16_t audio_sample { };
	uint8_t audio_sample8 { };
	int16_t next_audio_sample { 0 };
	uint8_t next_audio_sample8 { 128 };
	int16_t this_sample { };
	int16_t interp_step { 0 };
	int16_t audio_sample_r { };
	uint8_t audio_sample8_r { };
	int16_t next_audio_sample_r { 0 };
	uint8_t next_audio_sample8_r { 128 };
	int16_t this_sample_r { };
	int16_t interp_step_r { 0 };


	uint8_t bit_type { 0 };
	uint8_t channels { 0 };
	
	size_t progress_interval_samples = 0, progress_samples = 0;
	
	bool configured { false };
	uint32_t bytes_read { 0 };
	
	void samplerate_config(const SamplerateConfigMessage& message);
	void audio_config(const AudioTXConfigMessage& message);
	void replay_config(const ReplayConfigMessage& message);
	
	TXProgressMessage txprogress_message { };
	RequestSignalMessage sig_message { RequestSignalMessage::Signal::FillRequest };

	std::array<int16_t, 32> audio { };		// 2048/64
	const buffer_s16_t audio_buffer {
		(int16_t*)audio.data(),
		sizeof(audio) / sizeof(int16_t)
	};
	std::array<int16_t, 32> audio_right { };		// right channel
	const buffer_s16_t audio_buffer_right {
		(int16_t*)audio_right.data(),
		sizeof(audio_right) / sizeof(int16_t)
	};
	uint16_t as { 0 }, ai { 0 };
	AudioOutput audio_output { };
};

#endif
