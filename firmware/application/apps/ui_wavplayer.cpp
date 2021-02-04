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

#include "ui_wavplayer.hpp"
#include "string_format.hpp"
#include "tonesets.hpp"
#include "ui_fileman.hpp"

#include "audio.hpp"

using namespace tonekey;
using namespace portapack;

namespace ui {

bool WavPlayerView::is_active() const {
	return (bool)replay_thread;
}

void WavPlayerView::stop() {
	if (is_active())
		replay_thread.reset();

	baseband::replay_stop();

	audio::output::stop();

	playing = false;
	button_playpause.set_text("Play");
	button_open.hidden(false);
	ready_signal = false;
	set_dirty();
}

void WavPlayerView::handle_replay_thread_done(const uint32_t return_code) {
	stop();
	progressbar.set_value(0);
	if (return_code == ReplayThread::END_OF_FILE) {
		if(check_loop.value()) {
			start_play();
		}
	} else if (return_code == ReplayThread::READ_ERROR) {
		nav_.display_modal("Error", "File read error.");
	}
}

void WavPlayerView::set_ready() {
	ready_signal = true;
}

void WavPlayerView::focus() {
	button_open.focus();
}

void WavPlayerView::start_play() {
	stop();

	wav_reader = std::make_unique<WAVFileReader>();
	if (!wav_reader->open(soundfile_path)) {
		nav_.display_modal("Error", "File read error.");
		return;
	}

	baseband::set_audiotx_config(
		1536000 / 20,		// Update vu-meter at 20Hz
		transmitter_model.channel_bandwidth(),
		0,	// Gain is unused
		0,
		field_speed.value(),
		bit_type,
		channels
	);
	baseband::set_sample_rate(sample_rate);
	// Put replay thread to be initialized at last. maybe meaningful.

	replay_thread = std::make_unique<ReplayThread>(
		std::move(wav_reader),
		read_size, buffer_count,
		&ready_signal,
		[](uint32_t return_code) {
			ReplayThreadDoneMessage message { return_code };
			EventDispatcher::send_message(message);
		}
	);
	audio::output::start();

	playing = true;
	button_playpause.set_text("Stop");
	button_open.hidden(true);
	set_dirty();
}


void WavPlayerView::load_wav(std::filesystem::path file_path) {
	soundfile_path = file_path;

	text_filename.set(file_path.filename().string());
	text_duration.set(to_string_time_ms(wav_reader->ms_duration()));
	text_title.set(wav_reader->title().substr(0, 22));
	text_samplerate.set(to_string_dec_uint(wav_reader->sample_rate()) + " Hz");
	text_bits.set(to_string_dec_int(wav_reader->bits_per_sample()));
	text_channels.set(to_string_dec_int(wav_reader->channels()));
	sample_rate = wav_reader->sample_rate();
	bit_type = wav_reader->bits_per_sample() == 8 ? 0 : 1;
	channels = wav_reader->channels() - 1;

	button_playpause.hidden(false);
	check_loop.hidden(false);

	text_cur_pos.set("0");
	progressbar.set_max(wav_reader->sample_count() * (bit_type ? 2 : 1));

	button_playpause.focus();
}


void WavPlayerView::on_tx_progress(const uint32_t progress) {
	progressbar.set_value(progress);
	text_cur_pos.set(to_string_time_ms(progress / ((sample_rate / 1000) * (bit_type ? 2 : 1) * (channels + 1))));
}

WavPlayerView::WavPlayerView(
	NavigationView& nav
) : nav_ (nav)
{
	baseband::run_image(portapack::spi_flash::image_tag_audio_tx);

	// It's terrible. System transmitting threading seems to be limited to around 2478000Hz.
	// When running wav player, set sampling rate first.
	transmitter_model.set_sampling_rate(1536000);
	transmitter_model.disable();

	wav_reader = std::make_unique<WAVFileReader>();
	add_children({
		&labels_info,
		&text_filename,
		&text_title,
		&text_duration,
		&text_samplerate,
		&text_bits,
		&text_channels,
		&text_cur_pos,
		&field_volume,
		&field_speed,
		&button_open,
		&button_playpause,
		&check_loop,
		&progressbar,
		&audio
	});

//	refresh_list();

	button_playpause.hidden(true);
	check_loop.hidden(true);

	check_loop.set_value(false);

	button_playpause.on_select = [this](Button&) {
		if(playing) {
			stop();
		} else {
			start_play();
		}
	};
	button_open.on_select = [this, &nav](Button&) {
		wav_reader = std::make_unique<WAVFileReader>();
		auto open_view = nav.push<FileLoadView>(".WAV");
		open_view->on_changed = [this](std::filesystem::path file_path) {
			if (!wav_reader->open(file_path)) {
				nav_.display_modal("Error", "Couldn't open file.", INFO, nullptr);
				return;
			}
			if ((wav_reader->channels() > 2) || !((wav_reader->bits_per_sample() == 16) || (wav_reader->bits_per_sample() == 8))) {
				nav_.display_modal("Error", "Wrong format.\nWav player only accepts\n16 or 8-bit files.", INFO, nullptr);
				return;
			}
			load_wav(file_path);
		};
	};

	field_speed.on_select = [this](NumberField&) {
		field_speed.set_value(100);
	};

	field_volume.set_value((receiver_model.headphone_volume() - audio::headphone::volume_range().max).decibel() + 99);
	field_volume.on_change = [this](int32_t v) {
		receiver_model.set_headphone_volume(volume_t::decibel(v - 99) + audio::headphone::volume_range().max);
	};
	receiver_model.set_headphone_volume(receiver_model.headphone_volume());

	field_speed.set_value(100);

	audio::set_rate(audio::Rate::Hz_48000);
}

WavPlayerView::~WavPlayerView() {
	stop();
	baseband::shutdown();
}

}
