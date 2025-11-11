#include <SDL3/SDL.h>
#include <iostream>
#include <vector>
#include <functional>
#include "Emulator.h"

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
		SDL_SetRenderDrawColor(renderer, 80, 80, 220, 255);
		SDL_RenderFillRect(renderer, &loadBtn);
		SDL_RenderFillRect(renderer, &resetBtn);
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
		}
	}



	static void emu_reset_callback(void *userdata, const char* const* filelist, int filters) {
		if (!filelist) {
			SDL_Log("An error occured: %s", SDL_GetError());
			return;
		} else if (!*filelist) {
			SDL_Log("The user did not select any file.");
			SDL_Log("Most likely, the dialog was canceled.");
			return;
		}

		while (*filelist) {
			SDL_Log("Full path to selected file: '%s'", *filelist);
			filelist++;
		}
	}

	std::function<void()> onLoadROM = [] {};
	std::function<void()> onReset = [] {};

private:
	SDL_Renderer* renderer;
};

int main(int argc, char** argv) {

	Emulator emu;

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



	ui.onReset = [&]() {
		constexpr SDL_DialogFileFilter filter = {
			"NES Rom", "nes"
		};

		SDL_ShowOpenFileDialog(EmulatorUI::emu_reset_callback, nullptr, window, &filter, 1, "~/", false);
	};

	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT)
				running = false;
			else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
				ui.handleClick(e.button.x, e.button.y);
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