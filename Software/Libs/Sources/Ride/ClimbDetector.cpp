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
    return stats(mStart, mNow);
}

ClimbDetector::Event ClimbDetector::endClimb()
{
    Event ev = Event::None;
    if (mState == State::Climbing && (mPeak.distanceM - mStart.distanceM) >= kMinDistanceM) {
        mLast = stats(mStart, mPeak);
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
        mRise     = mNow;
        mState    = State::Candidate;
    }

    // Track the summit. The rise marker only moves on a meaningful gain, so
    // barometer jitter on a plateau does not keep the climb alive forever.
    if (mNow.altitudeM > mPeak.altitudeM) {
        mPeak = mNow;
    }
    if (mNow.altitudeM >= mRise.altitudeM + kPeakStepM) {
        mRise = mNow;
    }

    if (mState == State::Candidate && (mPeak.altitudeM - mStart.altitudeM) >= kConfirmGainM) {
        mState = State::Climbing;
        return Event::Started;
    }

    const bool dropped = (mPeak.altitudeM - mNow.altitudeM) >= kEndDropM;
    const bool flat    = (mNow.distanceM - mRise.distanceM) >= kEndFlatM;
    if (dropped || flat) {
        return endClimb();
    }
    return Event::None;
}

}  // namespace Ride
