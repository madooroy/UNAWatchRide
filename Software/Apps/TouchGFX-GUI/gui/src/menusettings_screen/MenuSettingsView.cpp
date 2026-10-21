#include <gui/menusettings_screen/MenuSettingsView.hpp>
#include <gui/containers/RideUi.hpp>
#include "Ride/IntervalSession.hpp"
#include <cstdio>

MenuSettingsView::MenuSettingsView() :
    mUpdateItemCb(this, &MenuSettingsView::updateItem),
    mUpdateCenterItemCb(this, &MenuSettingsView::updateCenterItem)
{
}

void MenuSettingsView::setupScreen()
{
    MenuSettingsViewBase::setupScreen();

    menuLayout.setAnimationSteps(App::Config::kMenuAnimationSteps);
    menuLayout.setTitle(T_TEXT_SETTINGS_UC);
    menuLayout.setUpdateItemCallback(mUpdateItemCb);
    menuLayout.setUpdateCenterItemCallback(mUpdateCenterItemCb);
    menuLayout.setNumberOfItems(Menu::ID_COUNT);

    menuLayout.showSensorRow(true);
    setGpsFix(mGpsFix);

    menuLayout.invalidate();
}

void MenuSettingsView::tearDownScreen()
{
    MenuSettingsViewBase::tearDownScreen();
}

void MenuSettingsView::setGpsFix(bool state)
{
    mGpsFix = state;
    menuLayout.setGps(state);
}

void MenuSettingsView::setAccessoryStatus(uint8_t state, const char* /*name*/)
{
    menuLayout.setHr(state);
}

void MenuSettingsView::setAutoPause(bool state)
{
    mAutoPause = state;
    menuLayout.invalidate();
}

void MenuSettingsView::setPhoneNotif(bool state)
{
    mPhoneNotif = state;
    menuLayout.invalidate();
}

void MenuSettingsView::setRideSettings(const Settings& s)
{
    mClimbAlerts = s.climbAlertsEn;
    mRouteEn     = s.routeEn;
    mHrTarget    = s.hrTargetZone <= Settings::kHrTargetMax ? s.hrTargetZone : 0;
    mPreset      = s.intervalPreset < Ride::IntervalSession::kPresetCount ? s.intervalPreset : 0;
    refreshTips();
}

void MenuSettingsView::setRouteInfo(Ride::RouteState state, float lengthM, bool isImperial)
{
    mRouteState = state;
    switch (state) {
    case Ride::RouteState::Loaded:
    case Ride::RouteState::Following: {
        char d[12];
        RideUi::distance(d, sizeof(d), lengthM, isImperial);
        std::snprintf(mRouteTip, sizeof(mRouteTip), "%s %s", d, isImperial ? "mi" : "km");
    } break;
    case Ride::RouteState::Failed:   std::snprintf(mRouteTip, sizeof(mRouteTip), "BAD FILE"); break;
    case Ride::RouteState::Disabled: std::snprintf(mRouteTip, sizeof(mRouteTip), "OFF");      break;
    case Ride::RouteState::None:
    default:                         std::snprintf(mRouteTip, sizeof(mRouteTip), "NO FILE");  break;
    }
    menuLayout.invalidate();
}

void MenuSettingsView::refreshTips()
{
    if (mHrTarget == 0) {
        std::snprintf(mHrTip, sizeof(mHrTip), "OFF");
    } else {
        std::snprintf(mHrTip, sizeof(mHrTip), "ZONE %u", static_cast<unsigned>(mHrTarget));
    }
    menuLayout.invalidate();
}

void MenuSettingsView::setPositionId(uint16_t id)
{
    menuLayout.selectItem(id);
}

uint16_t MenuSettingsView::getPositionId()
{
    return menuLayout.getSelectedItem();
}

void MenuSettingsView::updateItem(MainMenuItem& item, int16_t index)
{
    MenuItemConfig cfg;

    switch (index) {
    case Menu::ID_ALERTS:
        cfg.style = MenuItemConfig::SIMPLE;
        cfg.msgId = T_TEXT_LAP_ALERTS;
        break;
    case Menu::ID_AUTO_PAUSE:
        cfg.style    = MenuItemConfig::TIP;
        cfg.msgId    = T_TEXT_AUTO_PAUSE;
        cfg.tipId    = mAutoPause ? T_TEXT_ON_UC : T_TEXT_OFF_UC;
        cfg.tipColor = mAutoPause ? SDK::GUI::Color::YELLOW_DARK
                                  : SDK::GUI::Color::TEAL;
        break;
    case Menu::ID_HR_TARGET:
        cfg.style    = MenuItemConfig::TIP;
        cfg.msgChar  = "HR target";
        cfg.tipChar  = mHrTip;
        cfg.tipColor = mHrTarget ? SDK::GUI::Color::YELLOW_DARK : SDK::GUI::Color::TEAL;
        break;
    case Menu::ID_INTERVALS:
        cfg.style    = MenuItemConfig::TIP;
        cfg.msgChar  = "Intervals";
        cfg.tipChar  = Ride::IntervalSession::kPresets[mPreset].label;
        cfg.tipColor = SDK::GUI::Color::YELLOW_DARK;
        break;
    case Menu::ID_CLIMB_ALERTS:
        cfg.style    = MenuItemConfig::TIP;
        cfg.msgChar  = "Climb alerts";
        cfg.tipId    = mClimbAlerts ? T_TEXT_ON_UC : T_TEXT_OFF_UC;
        cfg.tipColor = mClimbAlerts ? SDK::GUI::Color::YELLOW_DARK : SDK::GUI::Color::TEAL;
        break;
    case Menu::ID_ROUTE:
        cfg.style    = MenuItemConfig::TIP;
        cfg.msgChar  = "Follow route";
        cfg.tipChar  = mRouteTip;
        cfg.tipColor = (mRouteEn && mRouteState != Ride::RouteState::Failed &&
                        mRouteState != Ride::RouteState::None)
                           ? SDK::GUI::Color::YELLOW_DARK : SDK::GUI::Color::TEAL;
        break;
    case Menu::ID_PHONE_NOTIF:
        cfg.style    = MenuItemConfig::TIP;
        cfg.msgId    = T_TEXT_PHONE_NOTIF_DOT;
        cfg.tipId    = mPhoneNotif ? T_TEXT_ON_UC : T_TEXT_OFF_UC;
        cfg.tipColor = mPhoneNotif ? SDK::GUI::Color::YELLOW_DARK
                                   : SDK::GUI::Color::TEAL;
        break;
    default:
        return;
    }

    item.apply(cfg);
}

void MenuSettingsView::updateCenterItem(MainMenuCenterItem& item, int16_t index)
{
    MenuItemConfig cfg;

    switch (index) {
    case Menu::ID_ALERTS:
        cfg.style = MenuItemConfig::SIMPLE;
        cfg.msgId = T_TEXT_LAP_ALERTS;
        break;
    case Menu::ID_AUTO_PAUSE:
        cfg.style       = MenuItemConfig::TOGGLE;
        cfg.msgId       = T_TEXT_AUTO_BR_PAUSE;
        cfg.msgIdType   = T_TMP_SEMIBOLD_25;
        cfg.toggleState = mAutoPause;
        break;
    case Menu::ID_HR_TARGET:
        cfg.style     = MenuItemConfig::TIP;
        cfg.msgChar   = "HR target";
        cfg.msgIdType = T_TMP_SEMIBOLD_25;
        cfg.tipChar   = mHrTip;
        // On the teal centre band, "off" needs a light colour to show.
        cfg.tipColor  = mHrTarget ? SDK::GUI::Color::YELLOW_DARK : SDK::GUI::Color::WHITE;
        break;
    case Menu::ID_INTERVALS:
        cfg.style     = MenuItemConfig::TIP;
        cfg.msgChar   = "Intervals";
        cfg.msgIdType = T_TMP_SEMIBOLD_25;
        cfg.tipChar   = Ride::IntervalSession::kPresets[mPreset].label;
        cfg.tipColor  = SDK::GUI::Color::YELLOW_DARK;
        break;
    case Menu::ID_CLIMB_ALERTS:
        cfg.style       = MenuItemConfig::TOGGLE;
        cfg.msgChar     = "Climb\nalerts";
        cfg.msgIdType   = T_TMP_SEMIBOLD_25;
        cfg.toggleState = mClimbAlerts;
        break;
    case Menu::ID_ROUTE:
        cfg.style       = MenuItemConfig::TOGGLE;
        cfg.msgChar     = "Follow\nroute";
        cfg.msgIdType   = T_TMP_SEMIBOLD_25;
        cfg.toggleState = mRouteEn;
        break;
    case Menu::ID_PHONE_NOTIF:
        cfg.style       = MenuItemConfig::TOGGLE;
        cfg.msgId       = T_TEXT_PHONE_BR_NOTIF_DOT;
        cfg.msgIdType   = T_TMP_SEMIBOLD_25;
        cfg.toggleState = mPhoneNotif;
        break;
    default:
        return;
    }

    item.apply(cfg);
}

void MenuSettingsView::handleKeyEvent(uint8_t key)
{
    if (key == SDK::GUI::Button::L1) {
        menuLayout.selectPrev();
    }

    if (key == SDK::GUI::Button::L2) {
        menuLayout.selectNext();
    }

    if (key == SDK::GUI::Button::R1) {
        uint32_t id = menuLayout.getSelectedItem();

        switch (id) {
        case Menu::ID_ALERTS:
            application().gotoMenuAlertsScreenNoTransition();
            break;
        case Menu::ID_AUTO_PAUSE:
            mAutoPause = !mAutoPause;
            setAutoPause(mAutoPause);
            presenter->saveAutoPause(mAutoPause);
            break;
        case Menu::ID_HR_TARGET:
            // Off, zone 1 ... zone 5, off.
            mHrTarget = static_cast<uint8_t>((mHrTarget + 1u) % (Settings::kHrTargetMax + 1u));
            refreshTips();
            presenter->saveHrTarget(mHrTarget);
            break;
        case Menu::ID_INTERVALS:
            mPreset = static_cast<uint8_t>((mPreset + 1u) % Ride::IntervalSession::kPresetCount);
            menuLayout.invalidate();
            presenter->saveIntervalPreset(mPreset);
            break;
        case Menu::ID_CLIMB_ALERTS:
            mClimbAlerts = !mClimbAlerts;
            menuLayout.invalidate();
            presenter->saveClimbAlerts(mClimbAlerts);
            break;
        case Menu::ID_ROUTE:
            mRouteEn = !mRouteEn;
            menuLayout.invalidate();
            presenter->saveRoute(mRouteEn);
            break;
        case Menu::ID_PHONE_NOTIF:
            mPhoneNotif = !mPhoneNotif;
            setPhoneNotif(mPhoneNotif);
            presenter->savePhoneNotif(mPhoneNotif);
            break;
        default:
            break;
        }
    }

    if (key == SDK::GUI::Button::R2) {
        application().gotoMainScreenNoTransition();
    }
}
