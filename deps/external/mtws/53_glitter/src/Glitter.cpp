// MIT License

// Copyright (c) 2025 sdrjones

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#define COMPUTERCARD_NOIMPL
#include "ComputerCard.h"
#include "Glitter.h"
#include "Utils.h"
#include "HannWindow.h"
#include "PowerPan.h"

// Some Random functions from Chris J

uint32_t __not_in_flash_func(rnd8)()
{
    static uint32_t lcg_seed = 1;
    lcg_seed = 1664525 * lcg_seed + 1013904223;
    return lcg_seed >> 24;
}

uint32_t __not_in_flash_func(rnd12)()
{
    static uint32_t lcg_seed = 1;
    lcg_seed = 1664525 * lcg_seed + 1013904223;
    return lcg_seed >> 20;
}
uint32_t __not_in_flash_func(rnd16)()
{
    static uint32_t lcg_seed = 1;
    lcg_seed = 1664525 * lcg_seed + 1013904223;
    return lcg_seed >> 16;
}
uint32_t __not_in_flash_func(rnd24)()
{
    static uint32_t lcg_seed = 1;
    lcg_seed = 1664525 * lcg_seed + 1013904223;
    return lcg_seed >> 8;
}

int32_t __not_in_flash_func(rndi32)()
{
    static uint32_t lcg_seed = 1;
    lcg_seed = 1664525 * lcg_seed + 1013904223;
    return lcg_seed;
}

uint32_t __not_in_flash_func(distance_in_circular_buffer)(uint32_t a, uint32_t b, uint32_t buffer_size)
{
    if (a <= b)
    {
        return b - a;
    }
    else
    {
        return buffer_size - a + b;
    }
}

Glitter::Glitter()
{
    clearBuffers();
    resetPointers();
    ReadKnobs();
    halfTime_ = false;
    startupCounter_ = 400;
    recordStateHannIndex_ = 0;
    recordState_ = RecordStateOff;
    curSwitch_ = SwitchVal();
    clockCount_ = 0;
    samplesPerPulse_ = 0;
    samplesMultiplier_ = 0;
    clockLed_ = 0;
    clockState_ = ClockOff;
    maxClockShiftDown_ = 1;
    maxClockShiftUp_ = 1;
    pitchChance_ = 0;

    for (int g = 0; g < kMaxGrains; ++g)
    {
        grains_[g].currentIndex_ = 0;
        grains_[g].sizeSamples_ = kMaxGrainSize;
        grains_[g].startIndex_ = rand() % kBufSize;
        grains_[g].level_ = 0;
        grains_[g].pan_ = 128;
        grains_[g].pitch_ = Normal;
        grains_[g].intendedPitch_ = Normal;
        grains_[g].sleepCounter_ = 400;
    }
}

void Glitter::UpdateClock()
{
    switch (clockState_)
    {
    case ClockOff:
    {
        clockState_ = ClockWaitingFirstPulse;
        samplesPerPulse_ = 0;
    }
    break;

    case ClockWaitingFirstPulse:
    {
        if (PulseIn1RisingEdge())
        {
            clockCount_ = 0;
            clockState_ = ClockWaitingSecondPulse;
        }
    }
    break;

    case ClockWaitingSecondPulse:
    case ClockRunning:
    {
        clockCount_++;
        if (PulseIn1RisingEdge())
        {
            if (abs(clockCount_ - samplesPerPulse_) > kClockChangeThreshold)
            {
                // Use this to debug clock changes
                // LedBrightness(5, clockLed_ * 2048);
                // clockLed_ = (clockLed_ + 1) % 2;

                samplesPerPulse_ = clockCount_;
                maxClockShiftDown_ = 0;
                maxClockShiftUp_ = 0;

                uint64_t samplesPerQuaver_ = samplesPerPulse_ << 1;

                if ((samplesPerQuaver_ * 6) <= kBufSize)
                {
                    samplesMultiplier_ = samplesPerQuaver_;
                }
                else if ((samplesPerQuaver_ * 3) <= kBufSize)
                {
                    samplesMultiplier_ = samplesPerPulse_;
                }
                else
                {
                    samplesMultiplier_ = samplesPerPulse_ >> 1;
                }

                uint32_t tmp = kMinGrainSize;
                while ((tmp < samplesPerPulse_) && (maxClockShiftDown_ <= kAbsMaxClockShift))
                {
                    tmp = tmp << 1;
                    maxClockShiftDown_++;
                }

                tmp = kMaxGrainSize;
                while ((tmp >= samplesPerPulse_) && (maxClockShiftUp_ <= kAbsMaxClockShift))
                {
                    tmp = tmp << 1;
                    maxClockShiftUp_++;
                }
            }
            clockCount_ = 0;
            clockState_ = ClockRunning;
        }
        else if (clockCount_ > kMaxSamplesBetweenClocks)
        {
            clockCount_ = 0;
            clockState_ = ClockWaitingFirstPulse;
        }
    }
    break;

    default:
        break;
    }
}

void Glitter::RecordProcess(int16_t audioM)
{

    bool shouldRecord = false;

    // Currently Middle is don't record,
    // either up or down is record
    if (curSwitch_ != Switch::Middle)
    {
        shouldRecord = true;
        if (oldSignalLevel_ < xKnob_)
        {
            oldSignalLevel_++;
        }
        else if (oldSignalLevel_ > xKnob_)
        {
            oldSignalLevel_--;
        }
    }
    else
    {
        pitchChance_ = xKnob_;
    }

    // The recorded signal is a mix of the existing signal and the new input, determined
    // by oldSignalLevel_

    audioM = (audioBuf_[writeI_] * (oldSignalLevel_) >> 12) + (audioM * (4095 - (oldSignalLevel_)) >> 12);

    switch (recordState_)
    {
    case RecordStateOn:
    {
        audioBuf_[writeI_] = audioM;
        if (!shouldRecord)
        {
            recordState_ = RecordStateEnteringOff;
        }
        lastRecordedWriteI_ = writeI_;
    }
    break;

    case RecordStateEnteringOn:
    {

        uint32_t fadedIn = audioM * kHannWindowFirstHalf[recordStateHannIndex_];
        uint32_t fadedBuf = audioBuf_[writeI_] * (kHannWindowFirstHalf[(kHalfHannSize - 1) - recordStateHannIndex_]);

        // Because this is a crossfade there is no need to right shift the
        // sum result
        uint32_t fadedSum = (fadedIn + fadedBuf);
        uint16_t fadedSum16 = static_cast<uint16_t>(fadedSum >> 15);

        audioBuf_[writeI_] = fadedSum16;

        recordStateHannIndex_++;
        if (!shouldRecord)
        {
            recordState_ = RecordStateEnteringOff;
        }
        else if (recordStateHannIndex_ >= (kHalfHannSize - 1))
        {
            recordState_ = RecordStateOn;
        }
        lastRecordedWriteI_ = writeI_;
    }
    break;

    case RecordStateEnteringOff:
    {
        uint32_t fadedIn = audioM * kHannWindowFirstHalf[recordStateHannIndex_];
        uint32_t fadedBuf = audioBuf_[writeI_] * (kHannWindowFirstHalf[kHalfHannSize - recordStateHannIndex_]);

        uint32_t fadedSum = (fadedIn + fadedBuf);
        uint16_t fadedSum16 = static_cast<uint16_t>(fadedSum >> 15);
        audioBuf_[writeI_] = fadedSum16;

        recordStateHannIndex_--;
        if (shouldRecord)
        {
            recordState_ = RecordStateEnteringOn;
        }
        else if (recordStateHannIndex_ == 0)
        {
            recordState_ = RecordStateOff;
        }
        lastRecordedWriteI_ = writeI_;
    }
    break;

    case RecordStateOff:
    {
        if (shouldRecord)
        {
            recordState_ = RecordStateEnteringOn;
        }
    }
    break;

    default:
        break;
    }
}

Glitter::Pitch Glitter::GeneratePitch(int startIndex, int grainSize)
{
    int outPitchIndex = 0;

    // pitchChance_ is the last recorded
    // xKnob in play mode
    int octChance = pitchChance_;
    int fifthChance = 0;

    if (octChance > 2047)
    {
        fifthChance = octChance - 2047; 
        octChance = 2047;
    }

    uint16_t octRand = rnd12();
    uint16_t fifthRand = rnd12();

    uint16_t distBehind = distance_in_circular_buffer(startIndex, writeI_, kBufSize);
    uint16_t distAhead = kBufSize - distBehind;

    uint16_t factor = 0;
    if (distBehind < (grainSize >> 1))
    {
        // must play low or normal

        // check if we must play normal
        if ((distAhead >> 1) < grainSize) return Normal;

        factor = 0;
    }
    else if ((distAhead >> 1) < grainSize)
    {
        // must play high or normal
        factor = 1;
    }
    else
    {
        // can play high or low
        factor = octRand & 0x01;
    }
    
    if (fifthRand < fifthChance)
    {
        outPitchIndex = factor + 3;
    } 
    else if (octRand < octChance)
    {
        outPitchIndex = factor + 1;
    }
    return pitchesLookup_[outPitchIndex];
}

void Glitter::GrainProcess(int16_t &wetL, int16_t &wetR)
{
    uint32_t startRange = kBufSize;
    uint32_t sizeRange = kMaxGrainSize;
    uint16_t repeatChance = cv1_;
    uint16_t sleepChance = cv2_;
    for (unsigned int g = 0; g < kMaxGrains; ++g)
    {
        Grain &grain = grains_[g];

        // Decide whether the grain should go to sleep
        if (grain.sleepCounter_ > 0)
        {
            grain.sleepCounter_--;

            if (grain.sleepCounter_ == 0)
            {
                // Snooze logic
                // (go back to sleep immediately)
                uint16_t sleepRand = rnd12() >> 1;
                if (sleepRand > sleepChance)
                {
                    uint32_t sleepSize = grain.sizeSamples_;
                    if (sleepSize < kMinSleepSize)
                    {
                        sleepSize = kMinSleepSize;
                    }
                    grain.sleepCounter_ = sleepSize;
                }
            }
            continue;
        }

        // Randomize grain start position and size if the grain has finished
        if (((grain.currentIndex_ >> 8) >= grain.sizeSamples_) && (g < kMaxGrains))
        {
            grain.currentIndex_ = 0;
            uint16_t repeatRnd = rnd12() >> 1;
            if (repeatRnd < repeatChance)
            {
                // Don't repeat - get a new set
                // of attributes

                // put the grain's old level back
                // in the headroom
                headRoom_ += grain.level_;

                // Check we haven't gone over the headroom limit
                if (headRoom_ > 4096)
                {
                    headRoom_ = 4096;
                }

                uint32_t maxSize = (sizeRange * yKnob_) >> 12;
                if (maxSize < kMinGrainSize)
                {
                    maxSize = kMinGrainSize;
                }

                uint64_t nextSize = 0;
                if (clockState_ != ClockRunning)
                {
                    nextSize = (rndi32() % (maxSize - kMinGrainSize)) + kMinGrainSize;
                }
                else
                {
                    uint32_t shift = 0;
                    shift = ((maxClockShiftDown_ + maxClockShiftUp_ + 1) * (4095 - yKnob_)) >> 12;

                    nextSize = (samplesPerPulse_ << maxClockShiftUp_) >> shift;

                    if (nextSize < kMinGrainSize)
                    {
                        // This will be out of beat so
                        // indicates that something has gone wrong
                        // with the maxClockShiftDown_ logic
                        nextSize = kMinGrainSize;
                    }
                    else if (nextSize > kMaxGrainSize)
                    {
                        nextSize = kMaxGrainSize;
                    }
                }

                uint64_t offset = 0;
                if (clockState_ != ClockRunning)
                {
                    offset = rndi32() % startRange;
                }
                else
                {
                    offset = (samplesMultiplier_)*g;
                }

                grain.startIndex_ = (lastRecordedWriteI_ + kBufSize - (offset)) % kBufSize;

                grain.sizeSamples_ = nextSize;

                grain.pan_ = rnd8(); // pan indexes the power pan_ array that has 256 entries
                if (grain.pan_ > 255)
                {
                    grain.pan_ = 255;
                }
                uint32_t rndLevel = rnd12();
                uint32_t pitchRand = rnd12();

#ifdef OLD_PITCH
                // Calculate the distance from the write pointer to figure out if we can play the grain
                // at a different rate from the rate at which we are writing to audioBuf
                uint16_t distBehind = distance_in_circular_buffer(grain.startIndex_, lastRecordedWriteI_, kBufSize);
                uint16_t distAhead = kBufSize - distBehind;

                if ((pitchRand < (pitchChance_ >> 1)) && ((distBehind >> 1) > grain.sizeSamples_))
                {
                    grain.pitch_ = OctaveHigh;
                }
                else if ((pitchRand < (pitchChance_)) && ((distAhead >> 1) > grain.sizeSamples_))
                {
                    grain.pitch_ = OctaveLow;
                }
                else
                {
                    grain.pitch_ = Normal;
                }
#else
                grain.pitch_ = GeneratePitch(grain.startIndex_, grain.sizeSamples_);
#endif

                grain.intendedPitch_ = grain.pitch_;

                // Set the level to the random level
                grain.level_ = rndLevel;

                if (grain.level_ < kGrainSilenceThreshold)
                {
                    grain.level_ = 0;
                }

                // Check there is enough headroom to take that level
                if (grain.level_ > headRoom_)
                {
                    grain.level_ = headRoom_;
                    headRoom_ = 0;
                }
                else
                {
                    // Remove the level from the headroom
                    headRoom_ -= grain.level_;
                }
            }
            else
            {
                // We want to repeat the grain but that might be problematic due to the
                // changed write position so double check that and revert to pitch normal
                // (where we will never overtake or get undertaken by the record head)
                uint16_t distBehind = distance_in_circular_buffer(grain.startIndex_, writeI_, kBufSize);
                uint16_t distAhead = kBufSize - distBehind;
                // Reset the pitch to its original selection
                grain.pitch_ = grain.intendedPitch_;

                // Downgrade pitch to normal if it's going to trip over
                // the write head
                if ((grain.pitch_ > 256) && (distBehind < (grain.sizeSamples_ >> 1)))
                {
                    grain.pitch_ = Normal;
                }
                else if ((grain.pitch_ < 256) && ((distAhead >> 1) < grain.sizeSamples_))
                {
                    grain.pitch_ = Normal;
                }
            }
        }

        // Save some processing if the grain is silent
        int16_t grainSample = 0;
        if (grain.level_ > 0)
        {
            unsigned int grainReadIndex = (grain.startIndex_ + (grain.currentIndex_ >> 8)) % kBufSize;
            grainSample = audioBuf_[grainReadIndex];

            // Apply level
            grainSample = static_cast<uint16_t>((grainSample * grain.level_) >> 12);

            // If we're near the start of end of the grain then we fade in/out
            // using a Hann window lookup table
            // As the hann window is symmetrical I'm using half of one to save
            // on space

            uint32_t hannIndex = (grain.currentIndex_ >> 8);
            if (hannIndex > kHalfHannSize)
            {
                hannIndex = grain.sizeSamples_ - hannIndex;
            }
            if (hannIndex < kHalfHannSize)
            {
                uint32_t faded32 = grainSample * kHannWindowFirstHalf[hannIndex];
                grainSample = static_cast<uint16_t>(faded32 >> 15);
            }

            // Pan the grain into wet left and right signals
            wetL += static_cast<int16_t>(grainSample * kLeftGains[grain.pan_] >> 12);
            wetR += static_cast<int16_t>(grainSample * kLeftGains[255 - grain.pan_] >> 12);
        }

        grain.currentIndex_ += grain.pitch_;


        if ((grain.currentIndex_  >> 8)  >= grain.sizeSamples_)
        {
            uint16_t sleepRand = rnd12() >> 1;
            if (sleepRand > sleepChance)
            {
                uint32_t sleepSize = grain.sizeSamples_;
                if (sleepSize < kMinSleepSize)
                {
                    sleepSize = kMinSleepSize;
                }
                grain.sleepCounter_ = sleepSize;
            }
        }

        if (g < kMaxGrains)
        {
            LedBrightness(g, abs(grainSample) << 2);
        }
    }
}

void Glitter::ProcessSample()
{

    halfTime_ = !halfTime_;

    if (startupCounter_)
    {
        startupCounter_--;
    }

    if (startupCounter_ == 0)
    {
        ReadAudio();

        if (halfTime_)
        {
            ReadKnobs();
            ReadCV();
            curSwitch_ = SwitchVal();
        }

        if (Connected(Input::Pulse1))
        {
            UpdateClock();
        }
        else
        {
            clockState_ = ClockOff;
        }

        // The audio buffer is mono = so mix the inputs
        // and shift if both are connected
        uint16_t inputShift = 0;
        if (Connected(Input::Audio1) && Connected(Input::Audio2))
        {
            inputShift = 1;
        }
        int16_t audioM = (audioL_ + audioR_) >> inputShift;

        int16_t wetL = 0;
        int16_t wetR = 0;

        GrainProcess(wetL, wetR);

        // Right shift recorded signal volume by more in order to match the perceived
        // level of the wet signal

        int16_t mixOutL = (audioBuf_[readI_] * (mainKnob_) >> 13) + (wetL * (4095 - (mainKnob_)) >> 12);
        int16_t mixOutR = (audioBuf_[readI_] * (mainKnob_) >> 13) + (wetR * (4095 - (mainKnob_)) >> 12);

        RecordProcess(audioM);

        // Update the read/write heads
        writeI_ = (writeI_ + 1) % kBufSize;
        readI_ = (readI_ + 1) % kBufSize;

        AudioOut1(mixOutL);
        AudioOut2(mixOutR);
    }
}

void Glitter::clearBuffers(void)
{
    for (int i = 0; i < kBufSize; ++i)
    {
        audioBuf_[i] = 0;
    }
}

void Glitter::resetPointers(void)
{
    writeI_ = 1;
    readI_ = 0;
}

void Glitter::ReadKnobs(void)
{
    // Virtual detent the knob values
    mainKnob_ = virtualDetentedKnob(KnobVal(Knob::Main));
    xKnob_ = virtualDetentedKnob(KnobVal(Knob::X));
    yKnob_ = virtualDetentedKnob(KnobVal(Knob::Y));
}

void Glitter::ReadAudio(void)
{
    audioL_ = AudioIn1(); // -2048 to 2047
    audioR_ = AudioIn2(); // -2048 to 2047
}

void Glitter::ReadCV(void)
{
    cv1_ = Connected(ComputerCard::Input(Input::CV1)) ? CVIn1() : kDefaultRepeatChance; // -2048 to 2047
    cv2_ = Connected(ComputerCard::Input(Input::CV2)) ? CVIn2() : kDefaultSleepChance;  // -2048 to 2047

    if (cv1_ > 2000)
        cv1_ = 2000;
    if (cv1_ < 0)
        cv1_ = 0;

    if (cv2_ > 2000)
        cv2_ = 2000;
    if (cv2_ < 0)
        cv2_ = 0;
}