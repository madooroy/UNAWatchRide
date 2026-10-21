#include <gui/menusettings_screen/MenuSettingsView.hpp>
#include <gui/menusettings_screen/MenuSettingsPresenter.hpp>

MenuSettingsPresenter::MenuSettingsPresenter(MenuSettingsView& v)
    : view(v)
{

}

void MenuSettingsPresenter::activate()
{
    view.setPositionId(model->menu().settings.get());
    model->menu().settings.resetChildren();
    model->resetIdleTimer();

    view.setGpsFix(model->hasGpsFix());
    view.setAccessoryStatus(model->getAccessoryState(), "");
    view.setAutoPause(model->getSettings().autoPauseEn);
    view.setPhoneNotif(model->getSettings().phoneNotifEn);
    view.setRideSettings(model->getSettings());
    view.setRouteInfo(model->getRouteState(), model->getRouteLengthM(), model->isUnitsImperial());
}

void MenuSettingsPresenter::onRouteInfo()
{
    view.setRouteInfo(model->getRouteState(), model->getRouteLengthM(), model->isUnitsImperial());
}

void MenuSettingsPresenter::saveClimbAlerts(bool state)
{
    Settings sett = model->getSettings();
    sett.climbAlertsEn = state;
    model->saveSettings(sett);
}

void MenuSettingsPresenter::saveRoute(bool state)
{
    Settings sett = model->getSettings();
    sett.routeEn = state;
    model->saveSettings(sett);   // The service (re)loads or drops the route.
}

void MenuSettingsPresenter::saveHrTarget(uint8_t zone)
{
    Settings sett = model->getSettings();
    sett.hrTargetZone = zone;
    model->saveSettings(sett);
}

void MenuSettingsPresenter::saveIntervalPreset(uint8_t preset)
{
    Settings sett = model->getSettings();
    sett.intervalPreset = preset;
    model->saveSettings(sett);
}

void MenuSettingsPresenter::deactivate()
{
    model->menu().settings.set(view.getPositionId());
}

void MenuSettingsPresenter::onGpsFix(bool acquired)
{
    view.setGpsFix(acquired);
}

void MenuSettingsPresenter::onAccessoryStatus(uint8_t state, const char* name)
{
    view.setAccessoryStatus(state, name);
}

void MenuSettingsPresenter::saveAutoPause(bool state)
{
    Settings sett = model->getSettings();
    sett.autoPauseEn = state;
    model->saveSettings(sett);
}

void MenuSettingsPresenter::savePhoneNotif(bool state)
{
    Settings sett = model->getSettings();
    sett.phoneNotifEn = state;
    model->saveSettings(sett);
}
