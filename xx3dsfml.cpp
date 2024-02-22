/*
 * This software is provided as is, without any warranty, express or implied.
 * Copyright 2023 Chris Malnick. All rights reserved.
 */

#include <libftd3xx/ftd3xx.h>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include<fstream>
#include<sstream>

#include <thread>
#include <queue>

#define NAME "xx3dsfml"

#define PRODUCT_1 "N3DSXL"
#define PRODUCT_2 "N3DSXL.2"

#define BULK_OUT 0x02
#define BULK_IN 0x82

#define FIFO_CHANNEL 0

#define CAP_WIDTH 240
#define CAP_HEIGHT 720

#define CAP_RES (CAP_WIDTH * CAP_HEIGHT)

#define FRAME_SIZE_RGB (CAP_RES * 3)
#define FRAME_SIZE_RGBA (CAP_RES * 4)

#define AUDIO_CHANNELS 2
#define SAMPLE_RATE 32734

#define SAMPLE_SIZE_8 2192
#define SAMPLE_SIZE_16 (SAMPLE_SIZE_8 / 2)

#define BUF_COUNT 8
#define BUF_SIZE (FRAME_SIZE_RGB + SAMPLE_SIZE_8)

#define TOP_WIDTH 400
#define TOP_HEIGHT 240

#define TOP_RES (TOP_WIDTH * TOP_HEIGHT)

#define BOT_WIDTH 320
#define BOT_HEIGHT 240

#define DELTA_WIDTH (TOP_WIDTH - BOT_WIDTH)
#define DELTA_RES (DELTA_WIDTH * BOT_HEIGHT)

struct Sample {
	Sample(sf::Int16 *bytes, std::size_t size) : bytes(bytes), size(size) {}

	sf::Int16 *bytes;
	std::size_t size;
};

FT_HANDLE handle;

UCHAR in_buf[BUF_COUNT][BUF_SIZE];
ULONG read[BUF_COUNT];

sf::Texture in_tex;
std::queue<Sample> samples;

int curr_in = 0;
bool running = true;

bool split, swap = false;

int volume = 50;
bool mute = false;

bool init = true;

bool *p_tb, *p_bb, *p_jb, *p_tc, *p_bc, *p_jc;
int *p_tr, *p_br, *p_jr;
double *p_ts, *p_bs, *p_js;

class Audio : public sf::SoundStream {
public:
	Audio() {
		sf::SoundStream::initialize(AUDIO_CHANNELS, SAMPLE_RATE);
		sf::SoundStream::setProcessingInterval(sf::seconds(0));
	}

private:
	bool onGetData(sf::SoundStream::Chunk &data) override {
		if (samples.empty()) {
			return false;
		}

		data.samples = samples.front().bytes;
		data.sampleCount = samples.front().size;

		samples.pop();

		return true;
	}

	void onSeek(sf::Time timeOffset) override {}
};

Audio audio;

void load(std::string name) {
	std::ifstream file(name);
	std::string line;

	while (std::getline(file, line)) {
		std::istringstream kvp(line);
		std::string key;

		if (std::getline(kvp, key, '=')) {
			std::string value;

			if (std::getline(kvp, value)) {
				if (key == "bot_blur") {
					*p_bb = std::stoi(value);
					continue;
				}

				if (key == "bot_crop") {
					*p_bc = std::stoi(value);
					continue;
				}

				if (key == "bot_rotation") {
					*p_br = std::stoi(value);
					continue;
				}

				if (key == "bot_scale") {
					*p_bs = std::stod(value);
					continue;
				}

				if (key == "joint_blur") {
					*p_jb = std::stoi(value);
					continue;
				}

				if (key == "joint_crop") {
					*p_jc = std::stoi(value);
					continue;
				}

				if (key == "joint_rotation") {
					*p_jr = std::stoi(value);
					continue;
				}

				if (key == "joint_scale") {
					*p_js = std::stod(value);
					continue;
				}

				if (key == "mute") {
					mute = std::stoi(value);
					continue;
				}

				if (key == "split") {
					split = std::stoi(value);
					continue;
				}

				if (key == "top_blur") {
					*p_tb = std::stoi(value);
					continue;
				}

				if (key == "top_crop") {
					*p_tc = std::stoi(value);
					continue;
				}

				if (key == "top_rotation") {
					*p_tr = std::stoi(value);
					continue;
				}

				if (key == "top_scale") {
					*p_ts = std::stod(value);
					continue;
				}

				if (key == "volume") {
					volume = std::stoi(value);
					continue;
				}
			}
		}
	}

	file.close();
}

void save(std::string name) {
	std::ofstream file(name);

	file << "bot_blur=" << *p_bb << std::endl;
	file << "bot_crop=" << *p_bc << std::endl;
	file << "bot_rotation=" << *p_br << std::endl;
	file << "bot_scale=" << *p_bs << (*p_bs - static_cast<int>(*p_bs) ? "" : ".0") << std::endl;
	file << "joint_blur=" << *p_jb << std::endl;
	file << "joint_crop=" << *p_jc << std::endl;
	file << "joint_rotation=" << *p_jr << std::endl;
	file << "joint_scale=" << *p_js << (*p_js - static_cast<int>(*p_js) ? "" : ".0") << std::endl;
	file << "mute=" << mute << std::endl;
	file << "split=" << split << std::endl;
	file << "top_blur=" << *p_tb << std::endl;
	file << "top_crop=" << *p_tc << std::endl;
	file << "top_rotation=" << *p_tr << std::endl;
	file << "top_scale=" << *p_ts << (*p_ts - static_cast<int>(*p_ts) ? "" : ".0") << std::endl;
	file << "volume=" << volume << std::endl;

	file.close();
}

class Screen {
public:
	sf::RectangleShape in_rect;
	sf::RenderWindow win;

	Screen(bool **pp_b, bool **pp_c, int **pp_r, double **pp_s) {
		*pp_b = &this->b;
		*pp_c = &this->c;
		*pp_r = &this->r;
		*pp_s = &this->s;
	}

	void build(int w, int h, int u, int v, std::string t, bool o) {
		this->x = this->w = w;
		this->y = this->h = h;

		this->t = t;

		this->in_rect.setTexture(&in_tex);
		this->in_rect.setTextureRect(sf::IntRect(v, u, h, w));

		this->in_rect.setSize(sf::Vector2f(h, w));
		this->in_rect.setOrigin(h / 2, w / 2);

		this->in_rect.setRotation(-90);
		this->in_rect.setPosition(w / 2, h / 2);

		this->out_tex.create(w, h);

		this->out_rect.setSize(sf::Vector2f(w, h));
		this->out_rect.setTexture(&this->out_tex.getTexture());

		this->view.reset(sf::FloatRect(0, 0, w, h));

		if (o) {
			this->open();
		}
	}

	void reset() {
		this->w = this->x;
		this->h = this->y;

		this->view.setRotation(0);

		this->view.setSize(this->w, this->h);
		this->win.setView(this->view);

		this->out_tex.setSmooth(this->b);

		if (this->r) {
			if (this->r / 10 % 2) {
				std::swap(this->w, this->h);
			}

			this->rotate();
		}

		if (this->c) {
			this->crop();
		}
	}

	void poll() {
		while (this->win.pollEvent(this->event)) {
			switch (this->event.type) {
			case sf::Event::Closed:
				running = false;
				break;

			case sf::Event::KeyPressed:
				switch (event.key.code) {
				case sf::Keyboard::S:
					split ^= swap = true;
					break;

				case sf::Keyboard::C:
					this->c ^= true;
					this->crop();

					break;

				case sf::Keyboard::B:
					out_tex.setSmooth(this->b ^= true);
					break;

				case sf::Keyboard::Hyphen:
					this->s -= this->s == 1.0 ? 0.0 : 0.5;
					break;

				case sf::Keyboard::Equal:
					this->s += this->s == 4.5 ? 0.0 : 0.5;
					break;

				case sf::Keyboard::LBracket:
					std::swap(this->w, this->h);

					this->r = ((this->r + 90) % 360 + 360) % 360;
					this->rotate();

					break;

				case sf::Keyboard::RBracket:
					std::swap(this->w, this->h);

					this->r = ((this->r - 90) % 360 + 360) % 360;
					this->rotate();

					break;

				case sf::Keyboard::M:
					audio.setVolume((mute ^= true) ? 0 : volume);
					break;

				case sf::Keyboard::Comma:
					audio.setVolume(volume -= volume == 0 ? 0 : 5);
					break;

				case sf::Keyboard::Period:
					audio.setVolume(volume += volume == 100 ? 0 : 5);
					break;

				case sf::Keyboard::F1:
				case sf::Keyboard::F2:
				case sf::Keyboard::F3:
				case sf::Keyboard::F4:
					init = true;

					load("./presets/layout" + std::to_string(event.key.code - sf::Keyboard::F1 + 1) + ".conf");
					audio.setVolume(mute ? 0 : volume);

					break;

				case sf::Keyboard::F5:
				case sf::Keyboard::F6:
				case sf::Keyboard::F7:
				case sf::Keyboard::F8:
					save("./presets/layout" + std::to_string(event.key.code - sf::Keyboard::F5 + 1) + ".conf");
					break;
				}

				break;
			}
		}
	}

	void toggle() {
		this->win.isOpen() ? this->win.close() : this->open();
	}

	void draw() {
		this->win.setSize(sf::Vector2u(this->w * this->s, this->h * this->s));

		this->out_tex.clear();
		this->out_tex.draw(this->in_rect);
		this->out_tex.display();

		this->win.clear();
		this->win.draw(this->out_rect);
		this->win.display();
	}

	void draw(sf::RectangleShape *p_top_rect, sf::RectangleShape *p_bot_rect) {
		this->win.setSize(sf::Vector2u(this->w * this->s, this->h * this->s));

		this->out_tex.clear();
		this->out_tex.draw(*p_top_rect);
		this->out_tex.draw(*p_bot_rect);
		this->out_tex.display();

		this->win.clear();
		this->win.draw(this->out_rect);
		this->win.display();
	}

private:
	bool b, c;
	int x, y, w, h, r;
	double s;

	std::string t;

	sf::RenderTexture out_tex;
	sf::RectangleShape out_rect;

	sf::View view;
	sf::Event event;

	void open() {
		this->win.create(sf::VideoMode(this->w * this->s, this->h * this->s), this->t);
		this->win.setView(this->view);
	}

	void rotate() {
		this->view.setRotation(this->r);

		this->view.setSize(this->w, this->h);
		this->win.setView(this->view);
	}

	void crop() {
		(this->r / 10 % 2 ? this->h : this->w) += this->c ? -DELTA_WIDTH : DELTA_WIDTH;

		this->view.setSize(this->w, this->h);
		this->win.setView(this->view);
	}
};

bool open() {
	if (FT_Create(const_cast<char*>(PRODUCT_1), FT_OPEN_BY_DESCRIPTION, &handle)) {
		if (FT_Create(const_cast<char*>(PRODUCT_2), FT_OPEN_BY_DESCRIPTION, &handle)) {
			printf("[%s] Create failed.\n", NAME);
			return false;
		}
	}

	UCHAR buf[4] = {0x40, 0x80, 0x00, 0x00};
	ULONG written = 0;

	if (FT_WritePipe(handle, BULK_OUT, buf, 4, &written, 0)) {
		printf("[%s] Write failed.\n", NAME);
		return false;
	}

	buf[1] = 0x00;

	if (FT_WritePipe(handle, BULK_OUT, buf, 4, &written, 0)) {
		printf("[%s] Write failed.\n", NAME);
		return false;
	}

	if (FT_SetStreamPipe(handle, false, false, BULK_IN, BUF_SIZE)) {
		printf("[%s] Stream failed.\n", NAME);
		return false;
	}

	return true;
}

void capture() {
	OVERLAPPED overlap[BUF_COUNT];

	for (curr_in = 0; curr_in < BUF_COUNT; ++curr_in) {
		if (FT_InitializeOverlapped(handle, &overlap[curr_in])) {
			printf("[%s] Initialize failed.\n", NAME);
			goto end;
		}
	}

	for (curr_in = 0; curr_in < BUF_COUNT; ++curr_in) {
		if (FT_ReadPipeAsync(handle, FIFO_CHANNEL, in_buf[curr_in], BUF_SIZE, &read[curr_in], &overlap[curr_in]) != FT_IO_PENDING) {
			printf("[%s] Read failed.\n", NAME);
			goto end;
		}
	}

	curr_in = 0;

	while (running) {
		if (FT_GetOverlappedResult(handle, &overlap[curr_in], &read[curr_in], true) == FT_IO_INCOMPLETE && FT_AbortPipe(handle, BULK_IN)) {
			printf("[%s] Abort failed.\n", NAME);
			goto end;
		}

		if (FT_ReadPipeAsync(handle, FIFO_CHANNEL, in_buf[curr_in], BUF_SIZE, &read[curr_in], &overlap[curr_in]) != FT_IO_PENDING) {
			printf("[%s] Read failed.\n", NAME);
			goto end;
		}

		if (++curr_in == BUF_COUNT) {
			curr_in = 0;
		}
	}

end:
	for (curr_in = 0; curr_in < BUF_COUNT; ++curr_in) {
		if (FT_ReleaseOverlapped(handle, &overlap[curr_in])) {
			printf("[%s] Release failed.\n", NAME);
		}
	}

	if (FT_Close(handle)) {
		printf("[%s] Close failed.\n", NAME);
	}

	running = false;
}

void map(UCHAR *p_in, UCHAR *p_out) {
	for (int i = 0, j = DELTA_RES, k = TOP_RES; i < CAP_RES; ++i) {
		if (i < DELTA_RES) {
			p_out[4 * i + 0] = p_in[3 * i + 0];
			p_out[4 * i + 1] = p_in[3 * i + 1];
			p_out[4 * i + 2] = p_in[3 * i + 2];
			p_out[4 * i + 3] = 0xff;
		}

		else if (i / CAP_WIDTH & 1) {
			p_out[4 * j + 0] = p_in[3 * i + 0];
			p_out[4 * j + 1] = p_in[3 * i + 1];
			p_out[4 * j + 2] = p_in[3 * i + 2];
			p_out[4 * j + 3] = 0xff;

			++j;
		}

		else {
			p_out[4 * k + 0] = p_in[3 * i + 0];
			p_out[4 * k + 1] = p_in[3 * i + 1];
			p_out[4 * k + 2] = p_in[3 * i + 2];
			p_out[4 * k + 3] = 0xff;

			++k;
		}
	}
}

void map(UCHAR *p_in, sf::Int16 *p_out) {
	for (int i = 0; i < SAMPLE_SIZE_16; ++i) {
		p_out[i] = p_in[i * 2 + 1] << 8 | p_in[i * 2];
	}
}

void playback() {
	sf::Int16 out_buf[BUF_COUNT][SAMPLE_SIZE_16];
	int curr_out, prev_out = 0;

	audio.setVolume(mute ? 0 : volume);

	while (running) {
		curr_out = (curr_in == 0 ? BUF_COUNT : curr_in) - 1;

		if (curr_out != prev_out) {
			if (read[curr_out] > FRAME_SIZE_RGB) {
				map(&in_buf[curr_out][FRAME_SIZE_RGB], out_buf[curr_out]);
				samples.emplace(out_buf[curr_out], (read[curr_out] - FRAME_SIZE_RGB) / 2);

				if (audio.getStatus() != sf::SoundStream::Playing) {
					audio.play();
				}
			}

			prev_out = curr_out;
		}

		sf::sleep(sf::seconds(0));
	}

	audio.stop();
}

void render() {
	UCHAR out_buf[FRAME_SIZE_RGBA];
	int curr_out, prev_out = 0;

	in_tex.create(CAP_WIDTH, CAP_HEIGHT);

	Screen top_screen(&p_tb, &p_tc, &p_tr, &p_ts);
	Screen bot_screen(&p_bb, &p_bc, &p_br, &p_bs);
	Screen joint_screen(&p_jb, &p_jc, &p_jr, &p_js);

	load("./" + std::string(NAME) + ".conf");

	top_screen.build(TOP_WIDTH, TOP_HEIGHT, 0, 0, std::string(NAME) + "-top", split);
	bot_screen.build(BOT_WIDTH, BOT_HEIGHT, TOP_WIDTH, 0, std::string(NAME) + "-bot", split);
	joint_screen.build(TOP_WIDTH, TOP_HEIGHT + BOT_HEIGHT, 0, 0, NAME, !split);

	if (!split) {
		bot_screen.in_rect.move(DELTA_WIDTH / 2, TOP_HEIGHT);
	}

	std::thread thread(playback);

	while (running) {
		top_screen.poll();
		bot_screen.poll();
		joint_screen.poll();

		curr_out = (curr_in == 0 ? BUF_COUNT : curr_in) - 1;

		if (curr_out != prev_out) {
			map(in_buf[curr_out], out_buf);
			in_tex.update(out_buf, CAP_WIDTH, CAP_HEIGHT, 0, 0);

			if (init) {
				top_screen.reset();
				bot_screen.reset();
				joint_screen.reset();

				swap = !(joint_screen.win.isOpen() ^ split);

				init = false;
			}

			if (swap) {
				top_screen.toggle();
				bot_screen.toggle();
				joint_screen.toggle();

				split ? bot_screen.in_rect.move(-DELTA_WIDTH / 2, -TOP_HEIGHT) : bot_screen.in_rect.move(DELTA_WIDTH / 2, TOP_HEIGHT);

				swap = false;
			}

			if (split) {
				top_screen.draw();
				bot_screen.draw();
			}

			else {
				joint_screen.draw(&top_screen.in_rect, &bot_screen.in_rect);
			}

			prev_out = curr_out;
		}

		sf::sleep(sf::seconds(0));
	}

	thread.join();

	top_screen.win.close();
	bot_screen.win.close();
	joint_screen.win.close();

	save("./" + std::string(NAME) + ".conf");
}

int main() {
	if (!open()) {
		return -1;
	}

	std::thread thread(capture);

	render();
	thread.join();

	return 0;
}
