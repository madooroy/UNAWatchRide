#include <gui/main_screen/MainView.hpp>
#include <gui/main_screen/MainPresenter.hpp>

MainPresenter::MainPresenter(MainView& v)
    : view(v)
{

}

void MainPresenter::activate()
{
    view.setPositionId(model->menu().get());
    model->menu().resetChildren();
    model->resetIdleTimer();

    view.setGpsFix(model->hasGpsFix());
    view.setAccessoryStatus(model->getAccessoryState(), "");
    view.setIntervalPreset(model->getSettings().intervalPreset);
    view.setRoute(model->getRouteState(), model->getRouteLengthM(), model->isUnitsImperial());
}

void MainPresenter::onSettings(const Settings& settings)
{
    view.setIntervalPreset(settings.intervalPreset);
    view.setRoute(model->getRouteState(), model->getRouteLengthM(), model->isUnitsImperial());
}

void MainPresenter::onRouteInfo()
{
    view.setRoute(model->getRouteState(), model->getRouteLengthM(), model->isUnitsImperial());
}

void MainPresenter::deactivate()
{
    model->menu().set(view.getPositionId());
}

void MainPresenter::onIdleTimeout()
{
    // Waiting on Start (or Intervals) for a GPS fix keeps the app open.
    const uint16_t pos = view.getPositionId();
    if (pos != App::MenuNav::Root::ID_START && pos != App::MenuNav::Root::ID_INTERVALS) {
        model->exitApp();
    }
}

void MainPresenter::onGpsFix(bool acquired)
{
    view.setGpsFix(acquired);
}

void MainPresenter::onAccessoryStatus(uint8_t state, const char* name)
{
    view.setAccessoryStatus(state, name);
}

void MainPresenter::setStartMode(bool intervals)
{
    model->setStartIntervals(intervals);
}

void MainPresenter::startTrack()
{
    model->trackStart();
}

void MainPresenter::exitApp()
{
    model->exitApp();
}
