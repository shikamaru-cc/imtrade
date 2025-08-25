// Dear ImGui: standalone example application for SDL2 + SDL_Renderer
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important to understand: SDL_Renderer is an _optional_ component of SDL2.
// For a multi-platform app consider using e.g. SDL+DirectX on Windows and SDL+OpenGL on Linux/OSX.

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include "implot.h"
#include "implot_internal.h"
#include "livermore.h"

#include <stdio.h>
#include <curl/curl.h>
#include <SDL2/SDL.h>
#include <vector>
#include <string>

#ifdef _WIN32
#include <windows.h>        // SetProcessDPIAware()
#endif

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

template <typename T>
int binary_search(const T* arr, int l, int r, T x) {
    if (r >= l) {
        int mid = l + (r - l) / 2;
        if (arr[mid] == x)
            return mid;
        if (arr[mid] > x)
            return binary_search(arr, l, mid - 1, x);
        return binary_search(arr, mid + 1, r, x);
    }
    return -1;
}

static void plot_candles(const char* label_id, const lv_candles* candles) {
    static const double half_width = 0.25f;
    static ImVec4 bull_col = ImVec4(0.000f, 1.000f, 0.441f, 1.000f);
    static ImVec4 bear_col = ImVec4(0.853f, 0.050f, 0.310f, 1.000f);

    const double * open  = candles->open;
    const double * close = candles->close;
    const double * low   = candles->low;
    const double * high  = candles->high;
    const size_t   count = candles->size;

    double min_price = candles->low[0];
    double max_price = candles->high[0];
    for (size_t i = 1; i < candles->size; i++) {
        if (candles->low[i] < min_price) min_price = candles->low[i];
        if (candles->high[i] > max_price) max_price = candles->high[i];
    }

    double price_range = max_price - min_price;
    ImPlot::SetupAxesLimits(
        -1, (double)candles->size + 3,
        min_price - price_range * 0.1,
        max_price + price_range * 0.1);

    ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Linear);
    ImPlot::SetupAxisFormat(ImAxis_Y1, "$%.2f");

    // Setup custom X-axis ticks with dates - only when day changes
    static std::vector<double> tick_positions;
    static std::vector<const char*> tick_labels;
    static std::vector<std::string> date_strings;
    tick_positions.clear();
    tick_labels.clear();
    date_strings.clear();
    int prev_day;
    char date_buf[16];
    for (size_t i = 0; i < candles->size; i++) {
        struct tm* tm_info = localtime(&candles->timestamp[i]);
        if (i == 0 || tm_info->tm_mday != prev_day) {
            tick_positions.push_back((double)i);
            strftime(date_buf, sizeof(date_buf), "%m-%d", tm_info);
            date_strings.emplace_back(date_buf);
            tick_labels.push_back(date_strings.back().c_str());
            prev_day = tm_info->tm_mday;
        }
    }
    ImPlot::SetupAxisTicks(
        ImAxis_X1, tick_positions.data(),
        (int)tick_positions.size(), tick_labels.data());

    // get ImGui window DrawList
    ImDrawList* draw_list = ImPlot::GetPlotDrawList();
    // begin plot item
    if (ImPlot::BeginItem(label_id)) {
        // override legend icon color
        ImPlot::GetCurrentItem()->Color = IM_COL32(64,64,64,255);

        // fit data if requested
        if (ImPlot::FitThisFrame()) {
            for (int i = 0; i < count; ++i) {
                ImPlot::FitPoint(ImPlotPoint(i, low[i]));
                ImPlot::FitPoint(ImPlotPoint(i, high[i]));
            }
        }

        // render data
        for (int i = 0; i < count; ++i) {
            ImVec2 open_pos  = ImPlot::PlotToPixels(i - half_width, open[i]);
            ImVec2 close_pos = ImPlot::PlotToPixels(i + half_width, close[i]);
            ImVec2 low_pos   = ImPlot::PlotToPixels(i, low[i]);
            ImVec2 high_pos  = ImPlot::PlotToPixels(i, high[i]);
            ImU32 color      = ImGui::GetColorU32(open[i] > close[i] ? bear_col : bull_col);
            draw_list->AddLine(low_pos, high_pos, color);
            draw_list->AddRectFilled(open_pos, close_pos, color);
        }

        // end plot item
        ImPlot::EndItem();
    }

    // custom tool
//    if (ImPlot::IsPlotHovered() && tooltip) {
//        ImPlotPoint mouse   = ImPlot::GetPlotMousePos();
//        mouse.x             = ImPlot::RoundTime(ImPlotTime::FromDouble(mouse.x), ImPlotTimeUnit_Day).ToDouble();
//        float  tool_l       = ImPlot::PlotToPixels(mouse.x - half_width * 1.5, mouse.y).x;
//        float  tool_r       = ImPlot::PlotToPixels(mouse.x + half_width * 1.5, mouse.y).x;
//        float  tool_t       = ImPlot::GetPlotPos().y;
//        float  tool_b       = tool_t + ImPlot::GetPlotSize().y;
//        ImPlot::PushPlotClipRect();
//        draw_list->AddRectFilled(ImVec2(tool_l, tool_t), ImVec2(tool_r, tool_b), IM_COL32(128,128,128,64));
//        ImPlot::PopPlotClipRect();
//        // find mouse location index using binary search
//        int idx = binary_search(xs, 0, count - 1, mouse.x);
//        if (idx >= 0 && idx < count) {
//            ImGui::BeginTooltip();
//            if (timestamps) {
//                char buff[32];
//                struct tm* tm_info = localtime(&timestamps[idx]);
//                strftime(buff, sizeof(buff), "%Y-%m-%d", tm_info);
//                ImGui::Text("Date:  %s", buff);
//            }
//            ImGui::Text("Open:  $%.2f", open[idx]);
//            ImGui::Text("Close: $%.2f", close[idx]);
//            ImGui::Text("Low:   $%.2f", low[idx]);
//            ImGui::Text("High:  $%.2f", high[idx]);
//            ImGui::EndTooltip();
//        }
//    }
}

// Main code
int main(int, char**) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Setup SDL
#ifdef _WIN32
    ::SetProcessDPIAware();
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with SDL_Renderer graphics context
    float main_scale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int)(1280 * main_scale), (int)(720 * main_scale), window_flags);
    if (window == nullptr) {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        SDL_Log("Error creating SDL_Renderer!");
        return -1;
    }
    //SDL_RendererInfo info;
    //SDL_GetRendererInfo(renderer, &info);
    //SDL_Log("Current SDL_Renderer: %s", info.name);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    ImPlot::CreateContext();

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    static lv_candles candles;
    lv_candles_init(&candles, 100);
    lv_candles_fetch(&candles, "sina", "sh000001", "1d");

    // Main loop
    bool done = false;
    while (!done) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ImGui::InputText("Market", market, sizeof(market));
        // ImGui::SameLine();
        // ImGui::InputText("Symbol", symbol, sizeof(symbol));
        // ImGui::SameLine();
        // ImGui::InputText("Interval", interval, sizeof(interval));

        // ImGui::SameLine();
 
        static bool tooltip = true;
        ImGui::Checkbox("Show Tooltip", &tooltip);
        ImGui::SameLine();
        static ImVec4 bull_col = ImVec4(0.000f, 1.000f, 0.441f, 1.000f);
        static ImVec4 bear_col = ImVec4(0.853f, 0.050f, 0.310f, 1.000f);
        ImGui::SameLine(); ImGui::ColorEdit4("##Bull", &bull_col.x, ImGuiColorEditFlags_NoInputs);
        ImGui::SameLine(); ImGui::ColorEdit4("##Bear", &bear_col.x, ImGuiColorEditFlags_NoInputs);
        ImPlot::GetStyle().UseLocalTime = false;

        // Create compressed continuous time axis
        double* dates = new double[candles.size];
        for (size_t i = 0; i < candles.size; i++) {
            dates[i] = (double)i;
        }

        if (ImPlot::BeginPlot("Real-time Candlestick Chart", ImVec2(-1,-1))) {
            ImPlot::SetupAxes(nullptr, nullptr, 0, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
            plot_candles("sh000001", &candles);
            ImPlot::EndPlot();
        }

        // Rendering
        ImGui::Render();
        SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    // Cleanup ImGui
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    ImPlot::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}