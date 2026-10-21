#include <gui/main_screen/MainView.hpp>
#include <images/BitmapDatabase.hpp>
#include <gui/containers/RideUi.hpp>
#include "Ride/IntervalSession.hpp"


MainView::MainView() :
    mUpdateItemCb(this, &MainView::updateItem),
    mUpdateCenterItemCb(this, &MainView::updateCenterItem),
    mpAnimationMiddleCb(this, &MainView::onAnimationMiddle)
{
}

void MainView::setupScreen()
{
    MainViewBase::setupScreen();

    setupItems();
    menuLayout.setAnimationSteps(App::Config::kMenuAnimationSteps);
    menuLayout.setTitle(T_TEXT_APP_NAME_UC);
    menuLayout.setInfoMsg(TYPED_TEXT_INVALID);   // GPS status now shown by the icon row
    menuLayout.setUpdateItemCallback(mUpdateItemCb);
    menuLayout.setUpdateCenterItemCallback(mUpdateCenterItemCb);
    menuLayout.setAnimationMiddleCallback(mpAnimationMiddleCb);
    menuLayout.setNumberOfItems(Menu::ID_COUNT);
    menuLayout.invalidate();

    // Sensor-status row (GPS + external HR) below the title.
    add(mSensorRow);
    mSensorRow.setPosition(0, 52, 240, 24);
    mSensorRow.setIcons(BITMAP_SENSORGPSDARK_ID, BITMAP_SENSORGPSLIGHT_ID,
                        BITMAP_SENSORHRDARK_ID, BITMAP_SENSORHRLIGHT_ID);
    mSensorRow.setGps(SDK::GUI::SensorStatusRow::gpsState(mGpsFix));

    updateBackground(menuLayout.getSelectedItem());
}

void MainView::tearDownScreen()
{
    MainViewBase::tearDownScreen();
}

void MainView::setGpsFix(bool state)
{
    mGpsFix = state;
    mSensorRow.setGps(SDK::GUI::SensorStatusRow::gpsState(state));
    updateBackground(menuLayout.getSelectedItem());
}

void MainView::setAccessoryStatus(uint8_t state, const char* /*name*/)
{
    mSensorRow.setHr(SDK::GUI::SensorStatusRow::hrState(state));
}

void MainView::setPositionId(uint16_t id)
{
    menuLayout.selectItem(id);
    updateBackground(menuLayout.getSelectedItem());
}

uint16_t MainView::getPositionId()
{
    return menuLayout.getSelectedItem();
}

void MainView::setIntervalPreset(uint8_t preset)
{
    if (preset >= Ride::IntervalSession::kPresetCount) {
        preset = 0;
    }
    mCenterItems[Menu::ID_INTERVALS].tipChar = Ride::IntervalSession::kPresets[preset].label;
    menuLayout.invalidate();
}

void MainView::setRoute(Ride::RouteState state, float lengthM, bool isImperial)
{
    MenuItemConfig& c = mCenterItems[Menu::ID_START];
    if (state == Ride::RouteState::Loaded || state == Ride::RouteState::Following) {
        char d[12];
        RideUi::distance(d, sizeof(d), lengthM, isImperial);
        std::snprintf(mRouteTip, sizeof(mRouteTip), "Route %s %s", d, isImperial ? "mi" : "km");
        c.style     = MenuItemConfig::TIP;
        c.msgIdType = T_TMP_SEMIBOLD_30;
        c.tipIdType = T_TMP_REGULAR_18;
        c.tipChar   = mRouteTip;
    } else if (state == Ride::RouteState::Failed) {
        c.style     = MenuItemConfig::TIP;
        c.msgIdType = T_TMP_SEMIBOLD_30;
        c.tipIdType = T_TMP_REGULAR_18;
        c.tipChar   = "Route file unreadable";
    } else {
        c.style     = MenuItemConfig::SIMPLE;
        c.msgIdType = T_TMP_SEMIBOLD_35;
        c.tipChar   = nullptr;
    }
    menuLayout.invalidate();
}

void MainView::setupItems()
{
    // START
    mCenterItems[Menu::ID_START].msgId     = T_TEXT_START;
    mCenterItems[Menu::ID_START].msgIdType = T_TMP_SEMIBOLD_35;

    mItems[Menu::ID_START].msgId = T_TEXT_START;


    // INTERVALS (the preset goes underneath; see setIntervalPreset)
    mCenterItems[Menu::ID_INTERVALS].style     = MenuItemConfig::TIP;
    mCenterItems[Menu::ID_INTERVALS].msgChar   = "Intervals";
    mCenterItems[Menu::ID_INTERVALS].msgIdType = T_TMP_SEMIBOLD_30;
    mCenterItems[Menu::ID_INTERVALS].tipIdType = T_TMP_REGULAR_18;

    mItems[Menu::ID_INTERVALS].msgChar = "Intervals";


    // SETTINGS
    mCenterItems[Menu::ID_SETTINGS].msgId = T_TEXT_SETTINGS;

    mItems[Menu::ID_SETTINGS].msgId = T_TEXT_SETTINGS;
}

void MainView::updateItem(MainMenuItem& item, int16_t index)
{
    if (index < 0 || index >= Menu::ID_COUNT) {
        return;
    }
    item.apply(mItems[index]);
}

void MainView::updateCenterItem(MainMenuCenterItem& item, int16_t index)
{
    if (index < 0 || index >= Menu::ID_COUNT) {
        return;
    }
    item.apply(mCenterItems[index]);
}

void MainView::onAnimationMiddle(int16_t index)
{
    updateBackground(index);
}

void MainView::handleKeyEvent(uint8_t key)
{
    if (key == SDK::GUI::Button::L1) {
        menuLayout.selectPrev();
    }

    if (key == SDK::GUI::Button::L2) {
        menuLayout.selectNext();
    }

    if (key == SDK::GUI::Button::R1) {
        onConfirm();
    }

    if (key == SDK::GUI::Button::R2) {
        presenter->exitApp();
    }
}

void MainView::onConfirm()
{
    int16_t idx = static_cast<int16_t>(menuLayout.getSelectedItem());
    if (idx < 0 || idx >= Menu::ID_COUNT) {
        return;
    }

    switch (idx) {
    case Menu::ID_START:
        start(false);
        break;

    case Menu::ID_INTERVALS:
        start(true);
        break;

    case Menu::ID_SETTINGS:
        application().gotoMenuSettingsScreenNoTransition();
        break;

    default:
        break;
    }
}

void MainView::start(bool intervals)
{
    presenter->setStartMode(intervals);
    if (mGpsFix) {
        presenter->startTrack();
        application().gotoTrackScreenNoTransition();
    } else {
        application().gotoTrackStartConfirmationScreenNoTransition();
    }
}

void MainView::updateBackground(int16_t index)
{
    switch (index) {
        case Menu::ID_START:
        case Menu::ID_INTERVALS:
            if (mGpsFix) {
                menuLayout.setBackground(SDK::GUI::Color::TEAL_DARK);
                menuLayout.getButtons().setR1(Buttons::AMBER);
            } else {
                menuLayout.setBackground(SDK::GUI::Color::GRAY_DARK);
                menuLayout.getButtons().setR1(Buttons::NONE);
            }
            break;

        default:
            menuLayout.setBackground(SDK::GUI::Color::TEAL_DARK);
            menuLayout.getButtons().setR1(Buttons::AMBER);
            break;
    }
}
