#include <platform/hal/simulator/sdl2/HALSDL2.hpp>
#include <touchgfx/hal/NoDMA.hpp>
#include <common/TouchGFXInit.hpp>
#include <gui_generated/common/SimConstants.hpp>
#include <platform/driver/touch/SDL2TouchController.hpp>
#include <platform/driver/touch/NoTouchController.hpp>
#include <touchgfx/lcd/LCD.hpp>
#include <simulator/mainBase.hpp>
#include "touchgfx/Utils.hpp"

#include "SDK/Kernel/KernelBuilder.hpp"
#include "SDK/Simulator/Kernel/Kernel.hpp"
#include "SDK/Kernel/KernelProviderGUI.hpp"
#include "SDK/Kernel/KernelProviderService.hpp"
#include "SDK/Simulator/OS/OS.hpp"
#include "SDK/Simulator/App/AppCore.hpp"
#include "SDK/Simulator/App/AppMessageCore.hpp"
#include "SDK/Simulator/App/KernelMessageDispatcher.hpp"
#include "SDK/Simulator/Kernel/Mock/Backlight.hpp"
#include "SDK/Simulator/Kernel/Mock/Buzzer.hpp"
#include "SDK/Simulator/Kernel/Mock/Vibro.hpp"

#include "SDK/Simulator/Components/ComponentSimulator.hpp"

#include "Service.hpp"

#include <chrono>
#include <cmath>

#include <stdlib.h>
#include <thread>

#ifdef __linux__
#include <time.h>
#else
#include "Windows.h"
#endif

#define LOG_MODULE_PRX      "main"
#define LOG_MODULE_LEVEL    LOG_LEVEL_DEBUG
#include "SDK/UnaLogger/Logger.h"

using namespace touchgfx;

// Kernel thread function
static void appThreadFunction(App::Core* appCore)
{
    appCore->run();
}

// GUI communication thread 
static void guiCommThreadFunction(App::Core* appCore)
{
    appCore->runGuiComm();
}

// Service thread function
static void serviceThreadFunction(Service* service)
{
    service->run();
}

// KernelMessage thread function
static void appMessageThreadFunction(SDK::App::KernelMessageDispatcher* appMessage)
{
    appMessage->run();
}

static int runTouchGFX(SDK::App::DualAppComm&  appComm,
                       SDK::Simulator::Kernel& srvKernel, 
                       SDK::Simulator::Kernel& guiKernel,
                       int                     argc,
                       char**                  argv)
{
    // Initialize Logger with Service's kernel. In real app Service and GUI will have each its own kernel.
    Logger_init(srvKernel.getKernel().log);
    // Save Service's kernel for global access
    SDK::KernelProviderService::CreateInstance(&srvKernel.getKernel());

    // Save GUI's kernel for global access
    SDK::KernelProviderGUI::CreateInstance(&guiKernel.getKernel());

    // Create the Service of the application
    Service service(SDK::KernelProviderService::GetInstance().getKernel());

	// Create KernelMessageDispatcher core
    SDK::Simulator::Mock::Backlight   mBacklight;
    SDK::Simulator::Mock::Buzzer      mBuzzer;
    SDK::Simulator::Mock::Vibro       mVibro;
    SDK::App::KernelMessageDispatcher kernelMessage(appComm, appComm.getMsgManager(), mVibro, mBacklight, mBuzzer);

	// Create the Application core
	App::Core appCore(appComm, srvKernel, guiKernel);

    //For windows/linux, DMA transfers are simulated
    touchgfx::NoDMA dma;
    LCD& lcd = setupLCD();
    touchgfx::NoTouchController tc;

    touchgfx::HAL& hal = touchgfx::touchgfx_generic_init<touchgfx::HALSDL2>(dma, lcd, tc, SIM_WIDTH, SIM_HEIGHT, 0, 0);

    setupSimulator(argc, argv, hal);

    // Set custom frame rate
    static_cast<touchgfx::HALSDL2&>(hal).setVsyncInterval(1000.0f / SDK::GUI::Config::kFrameRate);

    //// Ensure there is a console window to print to using printf() or
    //// std::cout, and read from using e.g. fgets or std::cin.
    //// Alternatively, instead of using printf(), always use
    //// touchgfx_printf() which will ensure there is a console to write
    //// to.
    touchgfx_enable_stdio();

    // Start threads
    std::thread serviceThread(serviceThreadFunction, &service);
    std::thread appThread(appThreadFunction, &appCore);
    std::thread guiCommThread(guiCommThreadFunction, &appCore);
    std::thread appMessageThread(appMessageThreadFunction, &kernelMessage);

    touchgfx::HAL::getInstance()->taskEntry();  // Main GUI loop
	
    appCore.stopRequest();

    // Stop threads
    serviceThread.join();
    appThread.join();
    guiCommThread.join();
    appMessageThread.join();

    return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------
// Optional hill for the simulator (UNA_SIM_HILL=<grade %>, e.g. 4).
//
// The SDK simulator's barometer is flat (a constant pressure plus noise), so
// climbs never happen. With UNA_SIM_HILL set, a background thread lowers the
// simulated pressure as a road would at that grade, at the simulated rider's
// steady 25 km/h: 60 s flat, then UNA_SIM_HILL_SEC seconds of climbing
// (default 240), then down at the same grade. The app is unchanged: the
// barometer is the only input this touches, so the climb faces and alerts it
// shows come from the app's own detection.
// -----------------------------------------------------------------------------
static void hillThreadFunction(float gradePct, float climbSec)
{
    constexpr float kSpeedMps = 25.0f / 3.6f;   // GpsStepCounterSimulator's base speed.
    constexpr float kFlatSec  = 60.0f;
    constexpr float kP0hPa    = 1013.25f;
    constexpr float kBaseAltM = 250.0f;

    const float rise = kSpeedMps * gradePct / 100.0f;   // m/s
    const auto  t0   = std::chrono::steady_clock::now();
    auto& pressure   = ComponentSimulator::GetInstance().getPressure();

    for (;;) {
        const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
        float h = 0.0f;
        if (t > kFlatSec) {
            const float up = std::min(t - kFlatSec, climbSec);
            const float down = std::max(0.0f, t - kFlatSec - climbSec);
            h = std::max(0.0f, rise * up - rise * down);
        }
        // International barometric formula, as the SDK's own pressure parser uses.
        const float p = kP0hPa * std::pow(1.0f - (kBaseAltM + h) / 44330.0f, 5.255877f);
        pressure.setParam(p);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

static void startHillIfAsked()
{
    const char* grade = getenv("UNA_SIM_HILL");
    if (grade == nullptr || *grade == '\0') {
        return;
    }
    const char* sec = getenv("UNA_SIM_HILL_SEC");
    const float g = static_cast<float>(atof(grade));
    const float c = (sec && *sec) ? static_cast<float>(atof(sec)) : 240.0f;
    printf("Simulated hill: %.1f %% for %.0f s after 60 s flat\n", g, c);  // Logger not up yet.
    std::thread(hillThreadFunction, g, c).detach();
}

#ifdef __linux__
int main(int argc, char** argv)
{
#else
#include <shellapi.h>
#ifdef _UNICODE
#error Cannot run in unicode mode
#endif
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int argc;
    char** argv = touchgfx::HALSDL2::getArgv(&argc);
#endif

    // Create kernel objects and service control
    SDK::App::MessageCore         appMessageCore;

	SDK::Simulator::Mock::SystemService serviceSystem;
    SDK::Simulator::Kernel serviceKernel("service");
	serviceKernel.setIAppComm(appMessageCore.getAppComm().getServiceComm());
	serviceKernel.setISystem(&serviceSystem);

    SDK::Simulator::Mock::SystemGUI guiSystem;
    SDK::Simulator::Kernel guiKernel("gui");
    guiKernel.setIAppComm(appMessageCore.getAppComm().getGuiComm());
    guiKernel.setISystem(&guiSystem);

    // Save GUI kernel for simulator
    SDK::Simulator::KernelHolder::Create(guiKernel);

    startHillIfAsked();

    return runTouchGFX(appMessageCore.getAppComm(),
                       serviceKernel,
                       guiKernel,
                       argc,
                       argv);
}
