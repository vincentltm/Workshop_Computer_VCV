#pragma once

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


class Glitter : public ComputerCard
{
public:
    Glitter();
    void __not_in_flash_func(ProcessSample)();

private:
    // private methods
    void clearBuffers(void);
    void resetPointers(void);
    void ReadKnobs(void);
    void ReadAudio(void);
    void ReadCV(void);
    void __not_in_flash_func(UpdateClock)(void);
    void __not_in_flash_func(GrainProcess)(int16_t& wetL, int16_t& wetR);
    void __not_in_flash_func(RecordProcess)(int16_t audioM);
    
    

    static constexpr uint32_t kBufSize = 2 * 48000;
    static const uint32_t kMaxGrains = 6;
    static constexpr uint32_t kMaxGrainSize = kBufSize;
    static constexpr uint32_t kMinGrainSize = 2048;
    static constexpr uint32_t kMinSleepSize = 24000;
    static constexpr uint32_t kMaxUnsigned = 4095;
    static constexpr uint32_t kMaxSigned = 2047;
    static constexpr uint32_t kGrainSilenceThreshold = 16;
    static constexpr uint16_t kDefaultSleepChance = 1500; // Lower = more sleep, 0-2000
    static constexpr uint16_t kDefaultRepeatChance = 2000; // Lower = more repeats, 0-2000
    static constexpr uint64_t kMaxSamplesBetweenClocks = kBufSize / 2;
    static constexpr uint64_t kClockChangeThreshold = 48; // Ignore clock jitter lower than this
    static constexpr uint64_t kAbsMaxClockShift = 1;
    static constexpr uint8_t kDontShiftBelow = 128;


    // Pitch enum values represent increments to the subsampled
    // grain.currentIndex_ in order to achieve
    // buffer sample increment.
    //
    // So at normal speed you add 256 to the currentIndex_
    // to proceed to the next index in the buffer
    
    enum Pitch
    {
        OctaveHigh = 512,
        FifthHigh = 384,
        Normal = 256,
        FifthLow = 192,
        OctaveLow = 128
    };

    inline constexpr static Pitch pitchesLookup_[] = {Normal, OctaveLow, OctaveHigh, FifthLow, FifthHigh};

    Pitch __not_in_flash_func(GeneratePitch)(int startIndex, int size);

    enum RecordState
    {
        RecordStateOn,
        RecordStateEnteringOn,
        RecordStateOff,
        RecordStateEnteringOff
    };

    enum ClockState
    {
        ClockOff,
        ClockWaitingFirstPulse,
        ClockWaitingSecondPulse,
        ClockRunning
    };
    
    struct Grain
    {
        unsigned int startIndex_;
        unsigned int sizeSamples_;
        // Lower 8 bits of currentIndex_ are subsample, 
        // so actual sample to read from buffer is
        // currentIndex_ >> 8
        unsigned int currentIndex_;
        unsigned int pan_;
        unsigned int level_;
        Pitch pitch_;
        Pitch intendedPitch_;
        unsigned int sleepCounter_;
    };

    int16_t audioBuf_[kBufSize];
    
    uint32_t writeI_ = 1;
    uint32_t lastRecordedWriteI_ = 1;
    uint32_t readI_ = 0;
    Grain grains_[kMaxGrains];
    bool halfTime_ = false;    // Some controls are only read every other Process call
    uint32_t startupCounter_ = 400;
    uint16_t headRoom_ = 4096; // The "level" available for all grains
    uint64_t clockCount_ = 0; // samples between pulse 1 rising
    uint64_t samplesPerPulse_ = 0; 
    uint64_t samplesMultiplier_ = 0;
    uint64_t minClockedBeat_;
    enum ClockState clockState_ = ClockOff;
    uint16_t maxClockShiftDown_ = 1;
    uint16_t maxClockShiftUp_ = 1;
    uint32_t clockLed_ = 0;
    int32_t oldSignalLevel_ = 0;
    int16_t pitchChance_ = 0;

    Switch curSwitch_;
    enum RecordState recordState_ = RecordStateOff;
    uint16_t recordStateHannIndex_ = 0;

    // ui elements
    int xKnob_;
    int yKnob_;
    int mainKnob_;

    // inputs
    int16_t cv1_;
    int16_t cv2_;
    int16_t audioL_;
    int16_t audioR_;
};
