#include <SDL3/SDL.h>
#include <iostream>
#include <vector>
#include <functional>
#include "Emulator.hpp"

constexpr int NES_WIDTH = 256;
constexpr int NES_HEIGHT = 240;
constexpr int MENU_HEIGHT = 32;

class FrameBuffer {
public:
	FrameBuffer(SDL_Renderer* renderer) : renderer(renderer) {
		texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, NES_WIDTH, NES_HEIGHT);
		pixels.resize(NES_WIDTH * NES_HEIGHT, 0xFF000000);
	}

	~FrameBuffer() {
		SDL_DestroyTexture(texture);
	}

	void fillTestPattern() {
		for (int y = 0; y < NES_HEIGHT; y++) {
			for (int x = 0; x < NES_WIDTH; x++) {
				pixels[y * NES_WIDTH + x] = ((x ^ y) & 0x10) ? 0xFF808080 : 0xFF202020;
			}
		}
	}

	void render(int x, int y, int width, int height) {
		SDL_UpdateTexture(texture, nullptr, pixels.data(), NES_WIDTH * sizeof(Uint32));
		SDL_FRect dst = { (float)x, (float)y, (float)width, (float)height };
		SDL_RenderTexture(renderer, texture, nullptr, &dst);
	}

	std::vector<Uint32>& data() { return pixels; }

private:
	SDL_Renderer* renderer = nullptr;
	SDL_Texture* texture = nullptr;
	std::vector<Uint32> pixels;
};

class EmulatorUI {
public:
	EmulatorUI(SDL_Renderer* renderer) : renderer(renderer) {}

	void renderMenu(int windowWidth) const {
		SDL_FRect menuBar{ 0, 0, (float)windowWidth, (float)MENU_HEIGHT };
		SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
		SDL_RenderFillRect(renderer, &menuBar);

		SDL_FRect loadBtn{ 10,4,80,24 };
		SDL_FRect resetBtn{ 100,4,80,24 };
		SDL_FRect debugBtn { 190, 4, 80, 24 };
		SDL_SetRenderDrawColor(renderer, 80, 80, 220, 255);
		SDL_RenderFillRect(renderer, &loadBtn);
		SDL_RenderFillRect(renderer, &resetBtn);
		SDL_RenderFillRect(renderer, &debugBtn);
	}

	void handleClick(int x, int y) {
		if (y > MENU_HEIGHT) return;

		if (x >= 10 && x <= 90) {
			std::cout << "[UI] Load clicked" << std::endl;
			onLoadROM();
		}
		else if (x >= 100 && x <= 180) {
			std::cout << "[UI] Reset clicked" << std::endl;
			onReset();
		} else if (x >= 190 && x <= 270) {
			std::cout << "[Debug] Debug info in console" << std::endl;
			onDebug();
		}
	}

	// Called on the main thread when an open-file path arrives
	void handleFileOpen(const char* path) {
		if (!path) return;
		SDL_Log("Processing ROM on main thread: %s", path);
		emu.reset(path);
	}

	static void emu_reset_callback(void *userdata, const char* const* filelist, int filters) {
		if (!filelist) {
			SDL_Log("An error occured: %s", SDL_GetError());
			return;
		} else if (!*filelist) {
			SDL_Log("The user did not select any file.");
			return;
		}

		// For each selected file, duplicate the C-string and push an SDL user event.
		while (*filelist) {
			char* dup = SDL_strdup(*filelist); // allocate with SDL_strdup so we can SDL_free later
			if (!dup) {
				SDL_Log("Failed to duplicate path");
			} else {
				SDL_Event ev;
				SDL_memset(&ev, 0, sizeof(ev));
				ev.type = SDL_EVENT_USER;
				ev.user.code = 1;            // application-defined code
				ev.user.data1 = dup;        // pass the duplicated string
				if (!SDL_PushEvent(&ev)) {
					SDL_free(dup);
					SDL_Log("SDL_PushEvent failed: %s", SDL_GetError());
				}
			}
			++filelist;
		}
	}


	std::function<void()> onLoadROM = [] {};
	std::function<void()> onReset = [] {};
	std::function<void()> onDebug = [] {};

private:
	SDL_Renderer* renderer;
	Emulator emu;
};

int main(int argc, char** argv) {



	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("NES Emulator", 800, 600, SDL_WINDOW_RESIZABLE);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);

	FrameBuffer framebuffer(renderer);
	framebuffer.fillTestPattern();

	EmulatorUI ui(renderer);

	bool running = true;

	ui.onLoadROM = [&]() {
		std::cout << "[Emulator] TODO" << std::endl;
	};

	ui.onDebug = [&]() {
		std::cout << "[Debug] Current " << std::endl;
	};

	ui.onReset = [&]() {
		constexpr SDL_DialogFileFilter filter = {
			"NES Rom", "nes"
		};

		SDL_ShowOpenFileDialog(EmulatorUI::emu_reset_callback, &ui, window, &filter, 1, "~/", false);
	};

	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT) {
				running = false;
			} else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
				ui.handleClick(e.button.x, e.button.y);
			} else if (e.type == SDL_EVENT_USER && e.user.code == 1) {
				char* path = static_cast<char*>(e.user.data1);
				ui.handleFileOpen(path);
				SDL_free(path); // free the SDL_strdup'd buffer
			}
		}

		int winW, winH;
		SDL_GetWindowSize(window, &winW, &winH);

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

		ui.renderMenu(winW);
		framebuffer.render(0, MENU_HEIGHT, winW, winH - MENU_HEIGHT);

		SDL_RenderPresent(renderer);
		SDL_Delay(16);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
