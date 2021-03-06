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

#include "proc_audiotx.hpp"
#include "portapack_shared_memory.hpp"
#include "sine_table_int8.hpp"
#include "event_m4.hpp"

#include <cstdint>

void AudioTXProcessor::execute(const buffer_c8_t& buffer){
	
	if (!configured) return;

	ai = 0;
	as = 0;
	
	// Zero-order hold (poop)
	for (size_t i = 0; i < buffer.count; i++) {
		resample_acc += resample_inc;
		if (resample_acc >= 0x1000000) {
			resample_acc -= 0x1000000;
			if (stream) {
				if(bit_type == 0) {
					audio_sample8 = next_audio_sample8;
					this_sample = (int16_t)(audio_sample8 - 0x80) << 8;
					stream->read(&next_audio_sample8, 1);
					interp_step = ((int16_t)next_audio_sample8 - (int16_t)audio_sample8) * 256 / (int16_t)((0x1000000 - resample_acc) / resample_inc);
					bytes_read++;
				} else {
					audio_sample = next_audio_sample;
					this_sample = audio_sample;
					stream->read(&next_audio_sample, 2);
					interp_step = (next_audio_sample - audio_sample) / (int16_t)((0x1000000 - resample_acc) / resample_inc);
					bytes_read += 2;
				}
				if(channels) {
					if(bit_type == 0) {
						audio_sample8_r = next_audio_sample8_r;
						this_sample_r = (int16_t)(audio_sample8_r - 0x80) << 8;
						stream->read(&next_audio_sample8, 1);
						interp_step_r = ((int16_t)next_audio_sample8_r - (int16_t)audio_sample8_r) * 256 / (int16_t)((0x1000000 - resample_acc) / resample_inc);
						bytes_read++;
					} else {
						audio_sample_r = next_audio_sample_r;
						this_sample_r = audio_sample_r;
						stream->read(&next_audio_sample_r, 2);
						interp_step_r = (next_audio_sample_r - audio_sample_r) / (int16_t)((0x1000000 - resample_acc) / resample_inc);
						bytes_read += 2;
					}
				}
			}
		} else {
            this_sample += interp_step;
			this_sample_r += interp_step_r;
        }
		
		if(channels) {
			sample = tone_gen.process((int8_t)((((int32_t)this_sample + (int32_t)this_sample_r) / 2) >> 8));
		} else {
			sample = tone_gen.process((int8_t)(this_sample >> 8));
		}
		
		if(mod_type == 1) { // AM
			re = sample / 2 + 64;
			im = 0;
		} else { // FM
			delta = sample * fm_delta;
			
			phase += delta;
			sphase = phase + (64 << 24);
			
			re = sine_table_i8[(sphase & 0xFF000000U) >> 24];
			im = sine_table_i8[(phase & 0xFF000000U) >> 24];
		}

		if (!as) {
			as = audio_decimation_factor - 1;
			audio_buffer.p[ai] = this_sample;
			if(channels){
				audio_buffer_right.p[ai] = this_sample_r;
			}
			ai++;
		} else {
			as--;
		}
		if(ai == 32) {
			if(channels) {
				audio_output.write_direct(audio_buffer, audio_buffer_right);
			}
			else {
				audio_output.write(audio_buffer);
			}
			ai = 0;
		}
		
		buffer.p[i] = { (int8_t)re, (int8_t)im };
	}
	
	progress_samples += buffer.count;
	if (progress_samples >= progress_interval_samples) {
		progress_samples -= progress_interval_samples;
		
		txprogress_message.progress = bytes_read;	// Inform UI about progress
		txprogress_message.done = false;
		shared_memory.application_queue.push(txprogress_message);
	}
}

void AudioTXProcessor::on_message(const Message* const message) {
	switch(message->id) {
		case Message::ID::AudioTXConfig:
			audio_config(*reinterpret_cast<const AudioTXConfigMessage*>(message));
			break;

		case Message::ID::ReplayConfig:
			configured = false;
			bytes_read = 0;
			replay_config(*reinterpret_cast<const ReplayConfigMessage*>(message));
			break;
		
		case Message::ID::SamplerateConfig:
			samplerate_config(*reinterpret_cast<const SamplerateConfigMessage*>(message));
			break;
		
		case Message::ID::FIFOData:
			configured = true;
			break;
		
		default:
			break;
	}
}

void AudioTXProcessor::audio_config(const AudioTXConfigMessage& message) {
	fm_delta = message.deviation_hz * (0xFFFFFFULL / baseband_fs_base);
	tone_gen.configure(message.tone_key_delta, message.tone_key_mix_weight);
	progress_interval_samples = message.divider;
	resample_acc = 0;
	audio_output.configure(audio_48k_hpf_30hz_config);
	baseband_fs = (size_t)((float)baseband_fs_base / ((float)message.speed / 100.0));
	bit_type = message.bit_type;
	channels = message.channels;
	mod_type = message.mod_type;
}

void AudioTXProcessor::replay_config(const ReplayConfigMessage& message) {
	if( message.config ) {
		
		stream = std::make_unique<StreamOutput>(message.config);
		
		// Tell application that the buffers and FIFO pointers are ready, prefill
		shared_memory.application_queue.push(sig_message);
	} else {
		stream.reset();
	}
}

void AudioTXProcessor::samplerate_config(const SamplerateConfigMessage& message) {
	resample_inc = (((uint64_t)message.sample_rate) << 24) / baseband_fs;	// 16.16 fixed point message.sample_rate
}

int main() {
	EventDispatcher event_dispatcher { std::make_unique<AudioTXProcessor>() };
	event_dispatcher.run();
	return 0;
}
