/**
 ******************************************************************************
 * @file    ClimbDetector.cpp
 * @brief   See ClimbDetector.hpp.
 ******************************************************************************
 */

#include "Ride/ClimbDetector.hpp"

namespace Ride {

void ClimbDetector::reset()
{
    *this = ClimbDetector{};
}

void ClimbDetector::push(const Sample& s)
{
    mHead = static_cast<uint8_t>((mHead + 1u) % kRing);
    mRing[mHead] = s;
    if (mCount8 < kRing) {
        ++mCount8;
    }
}

const ClimbDetector::Sample& ClimbDetector::at(uint8_t ageFromNewest) const
{
    return mRing[(mHead + kRing - ageFromNewest) % kRing];
}

void ClimbDetector::updateGrade()
{
    mGradeValid = false;
    const Sample& newest = at(0);
    for (uint8_t age = 1; age < mCount8; ++age) {
        const Sample& s = at(age);
        const float span = newest.distanceM - s.distanceM;
        if (span >= kWindowM) {
            mGradePct   = (newest.altitudeM - s.altitudeM) / span * 100.0f;
            mGradeValid = true;
            return;
        }
    }
}

ClimbStats ClimbDetector::stats(const Sample& start, const Sample& end)
{
    ClimbStats st;
    st.distanceM = end.distanceM - start.distanceM;
    st.gainM     = end.altitudeM - start.altitudeM;
    if (st.gainM < 0.0f) {
        st.gainM = 0.0f;
    }
    st.avgGradePct = (st.distanceM > 1.0f) ? (st.gainM / st.distanceM * 100.0f) : 0.0f;
    st.durationS   = (end.sec > start.sec) ? (end.sec - start.sec) : 0u;
    st.vamMph      = (st.durationS >= kMinVamSec)
                         ? (st.gainM * 3600.0f / static_cast<float>(st.durationS))
                         : 0.0f;
    return st;
}

ClimbStats ClimbDetector::current() const
{
    if (mState != State::Climbing) {
        return ClimbStats{};
    }
    // Start to the summit so far, as the finished climb is measured.
    // Measuring to "now" would let the descent or false flat after the summit
    // (until the climb is declared over) drag the average grade and VAM down.
    return stats(mStart, mTop);
}

ClimbDetector::Event ClimbDetector::endClimb()
{
    // Every confirmed climb is reported: confirmation already needs the
    // climb's length and gain, so a rider who felt the start buzz always
    // gets the summit.
    Event ev = Event::None;
    if (mState == State::Climbing) {
        mLast = stats(mStart, mTop);
        ++mCount;
        mTotalGain += mLast.gainM;
        ev = Event::Finished;
    }
    mState = State::Idle;
    return ev;
}

ClimbDetector::Event ClimbDetector::tick(float distanceM, float altitudeM, bool altValid,
                                         uint32_t activeSec)
{
    if (!altValid) {
        return Event::None;
    }

    mNow = Sample{distanceM, altitudeM, activeSec};

    if (mCount8 == 0 || (distanceM - at(0).distanceM) >= kSampleSpacingM) {
        push(mNow);
        updateGrade();
    }

    if (mState == State::Idle) {
        if (!(mGradeValid && mGradePct >= kStartGradePct)) {
            return Event::None;
        }
        // Backdate the start into the window that tripped the detector, to
        // where the road started to rise: the last sample still at the
        // window's low point. The window's own start is too early on a steep
        // climb -- 8 % trips the detector 40 m in, with 60 m of flat road
        // still in the window -- and would dilute the climb's average grade.
        uint8_t windowAge = 0;
        for (uint8_t age = 1; age < mCount8; ++age) {
            if (at(0).distanceM - at(age).distanceM >= kWindowM) {
                windowAge = age;
                break;
            }
        }
        float lowest = at(windowAge).altitudeM;
        for (uint8_t age = 0; age <= windowAge; ++age) {
            lowest = at(age).altitudeM < lowest ? at(age).altitudeM : lowest;
        }
        mStart = at(windowAge);
        for (uint8_t age = 0; age <= windowAge; ++age) {  // Newest first.
            if (at(age).altitudeM <= lowest + kLowPointM) {
                mStart = at(age);
                break;
            }
        }
        mPeak     = mNow;
        mTop      = mNow;
        mSteep    = mNow;
        mState    = State::Candidate;
    }

    // The highest point so far, for the drop that ends the climb.
    if (mNow.altitudeM > mPeak.altitudeM) {
        mPeak = mNow;
    }
    // The summit: the highest point up to where the road was last still
    // climbing (window grade >= kKeepGradePct). A false flat after the steep
    // part may gain a few more metres, but it is not part of the climb: it
    // would stretch the climb and drag its average grade towards 1-2 %.
    if (mGradeValid && mGradePct >= kKeepGradePct) {
        mSteep = mNow;
        mTop   = mPeak;
    }

    if (mState == State::Candidate &&
        (mTop.altitudeM - mStart.altitudeM) >= kConfirmGainM &&
        (mTop.distanceM - mStart.distanceM) >= kMinDistanceM) {
        mState = State::Climbing;
        return Event::Started;
    }

    const bool dropped = (mPeak.altitudeM - mNow.altitudeM) >= kEndDropM;
    const bool flat    = (mNow.distanceM - mSteep.distanceM) >= kEndFlatM;
    if (dropped || flat) {
        return endClimb();
    }
    return Event::None;
}

}  // namespace Ride
