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

#ifndef __UI_WAVPLAYER_H__
#define __UI_WAVPLAYER_H__

#include "ui_widget.hpp"
#include "ui_transmitter.hpp"
#include "replay_thread.hpp"
#include "baseband_api.hpp"
#include "lfsr_random.hpp"
#include "io_wave.hpp"
#include "tone_key.hpp"
#include "audio.hpp"

namespace ui {

class WavPlayerView : public View {
public:
	WavPlayerView(NavigationView& nav);
	~WavPlayerView();

	void focus() override;

	std::string title() const override { return "WAV player"; };

private:
	NavigationView& nav_;

	const size_t read_size { 2048 };	// Less ?
	const size_t buffer_count { 3 };
	std::unique_ptr<ReplayThread> replay_thread { };
	bool ready_signal { false };
	std::filesystem::path soundfile_path { };


	uint32_t sample_rate { 0 };
	uint8_t bit_type { 0 }; // 0 - 8bit, 1 - 16bit
	uint8_t channels { 0 }; // 0 - 1 ch, 1 - 2ch

	std::unique_ptr<WAVFileReader> wav_reader { };

	bool playing { false };

	void start_play();
	void stop();
	bool is_active() const;
	void set_ready();
	void handle_replay_thread_done(const uint32_t return_code);
	void on_tx_progress(const uint32_t progress);

	void load_wav(std::filesystem::path file_path);

	Labels labels_info {
		{ { 2 * 8 ,  1 * 8 }, "File: ", Color::light_grey() },
		{ { 2 * 8 ,  3 * 8 }, "Title:", Color::light_grey() },
		{ { 2 * 8 ,  5 * 8 }, "Duration:", Color::light_grey() },
		{ { 2 * 8 ,  7 * 8 }, "Sample rate:", Color::light_grey() },
		{ { 2 * 8 ,  9 * 8 }, "Bits:", Color::light_grey() },
		{ { 11 * 8 ,  9 * 8 }, "Channels:", Color::light_grey() },
		{ { 2 * 8 , 12 * 8 - 4 }, "Volume:", Color::light_grey() },
		{ { 2 * 8 , 14 * 8 - 4 }, "Speed:   %", Color::light_grey() },
	};

	Button button_open {
		{ 16 * 8, 12 * 8, 7 * 8, 4 * 8 },
		"Open"
	};

	Button button_playpause {
		{ 16 * 8, 24 * 8, 9 * 8, 4 * 8 },
		"Play"
	};

	Checkbox check_loop {
		{ 4 * 8, 24 * 8 + 4 },
		4,
		"Loop"
	};

	Text text_filename {
		{ 7 * 8, 1 * 8, 15 * 8, 16 },
		"-"
	};

	Text text_title {
		{ 8 * 8, 3 * 8, 15 * 8, 16 },
		"-"
	};

	Text text_duration {
		{ 11 * 8, 5 * 8, 6 * 8, 16 },
		"-"
	};

	Text text_samplerate {
		{ 14 * 8, 7 * 8, 6 * 8, 7 },
		"-"
	};

	Text text_bits {
		{ 7 * 8, 9 * 8, 6 * 8, 2 },
		"-"
	};

	Text text_channels {
		{ 20 * 8, 9 * 8, 6 * 8, 1 },
		"-"
	};

	Text text_cur_pos {
		{ 2 * 8, 18 * 8 - 4, 6 * 8, 16 },
		"-"
	};

	NumberField field_volume {
		{ 9 * 8, 12 * 8 - 4 },
		2,
		{ 0, 99 },
		1,
		' ',
	};

	NumberField field_speed {
		{ 8 * 8, 14 * 8 - 4 },
		3,
		{ 25, 400 },
		1,
		' ',
	};

	ProgressBar progressbar {
		{ 0 * 8, 20 * 8 - 4, 30 * 8, 16 }
	};

	Audio audio {
		{ 0 * 8, 22 * 8, 30 * 8, 8 },
		625
	};

	MessageHandlerRegistration message_handler_replay_thread_error {
		Message::ID::ReplayThreadDone,
		[this](const Message* const p) {
			const auto message = *reinterpret_cast<const ReplayThreadDoneMessage*>(p);
			this->handle_replay_thread_done(message.return_code);
		}
	};

	MessageHandlerRegistration message_handler_fifo_signal {
		Message::ID::RequestSignal,
		[this](const Message* const p) {
			const auto message = static_cast<const RequestSignalMessage*>(p);
			if (message->signal == RequestSignalMessage::Signal::FillRequest) {
				this->set_ready();
			}
		}
	};

	MessageHandlerRegistration message_handler_tx_progress {
		Message::ID::TXProgress,
		[this](const Message* const p) {
			const auto message = *reinterpret_cast<const TXProgressMessage*>(p);
			this->on_tx_progress(message.progress);
		}
	};
};

} /* namespace ui */

#endif/*__UI_WAVPLAYER_H__*/
