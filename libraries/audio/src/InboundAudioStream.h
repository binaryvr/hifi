//
//  InboundAudioStream.h
//  libraries/audio/src
//
//  Created by Yixin Wang on 7/17/2014.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_InboundAudioStream_h
#define hifi_InboundAudioStream_h

#include "NodeData.h"
#include "AudioRingBuffer.h"
#include "MovingMinMaxAvg.h"
#include "SequenceNumberStats.h"
#include "AudioStreamStats.h"
#include "PacketHeaders.h"
#include "StdDev.h"
#include "TimeWeightedAvg.h"

// This adds some number of frames to the desired jitter buffer frames target we use when we're dropping frames.
// The larger this value is, the less frames we drop when attempting to reduce the jitter buffer length.
// Setting this to 0 will try to get the jitter buffer to be exactly _desiredJitterBufferFrames when dropping frames,
// which could lead to a starve soon after.
const int DESIRED_JITTER_BUFFER_FRAMES_PADDING = 1;

// this controls the length of the window for stats used in the stats packet (not the stats used in
// _desiredJitterBufferFrames calculation)
const int STATS_FOR_STATS_PACKET_WINDOW_SECONDS = 30;


// this controls the window size of the time-weighted avg of frames available.  Every time the window fills up,
// _currentJitterBufferFrames is updated with the time-weighted avg and the running time-weighted avg is reset.
const int FRAMES_AVAILABLE_STAT_WINDOW_USECS = 2 * USECS_PER_SECOND;

const int INBOUND_RING_BUFFER_FRAME_CAPACITY = 100;

const int DEFAULT_MAX_FRAMES_OVER_DESIRED = 10;
const bool DEFAULT_DYNAMIC_JITTER_BUFFERS = true;
const int DEFAULT_STATIC_DESIRED_JITTER_BUFFER_FRAMES = 1;
const bool DEFAULT_USE_STDEV_FOR_JITTER_CALC = false;

const int DEFAULT_WINDOW_STARVE_THRESHOLD = 3;
const int DEFAULT_WINDOW_SECONDS_FOR_DESIRED_CALC_ON_TOO_MANY_STARVES = 50;
const int DEFAULT_WINDOW_SECONDS_FOR_DESIRED_REDUCTION = 10;

class InboundAudioStream : public NodeData {
    Q_OBJECT
public:
    class Settings {
    public:
        Settings()
            : _maxFramesOverDesired(DEFAULT_MAX_FRAMES_OVER_DESIRED),
            _dynamicJitterBuffers(DEFAULT_DYNAMIC_JITTER_BUFFERS),
            _staticDesiredJitterBufferFrames(DEFAULT_STATIC_DESIRED_JITTER_BUFFER_FRAMES),
            _useStDevForJitterCalc(DEFAULT_USE_STDEV_FOR_JITTER_CALC),
            _windowStarveThreshold(DEFAULT_WINDOW_STARVE_THRESHOLD),
            _windowSecondsForDesiredCalcOnTooManyStarves(DEFAULT_WINDOW_SECONDS_FOR_DESIRED_CALC_ON_TOO_MANY_STARVES),
            _windowSecondsForDesiredReduction(DEFAULT_WINDOW_SECONDS_FOR_DESIRED_REDUCTION)
        {}

        Settings(int maxFramesOverDesired, bool dynamicJitterBuffers, int staticDesiredJitterBufferFrames,
            bool useStDevForJitterCalc, int windowStarveThreshold, int windowSecondsForDesiredCalcOnTooManyStarves,
            int _windowSecondsForDesiredReduction)
            : _maxFramesOverDesired(maxFramesOverDesired),
            _dynamicJitterBuffers(dynamicJitterBuffers),
            _staticDesiredJitterBufferFrames(staticDesiredJitterBufferFrames),
            _useStDevForJitterCalc(useStDevForJitterCalc),
            _windowStarveThreshold(windowStarveThreshold),
            _windowSecondsForDesiredCalcOnTooManyStarves(windowSecondsForDesiredCalcOnTooManyStarves),
            _windowSecondsForDesiredReduction(windowSecondsForDesiredCalcOnTooManyStarves)
        {}

        // max number of frames over desired in the ringbuffer.
        int _maxFramesOverDesired; 

        // if false, _desiredJitterBufferFrames will always be _staticDesiredJitterBufferFrames.  Otherwise,
        // either fred or philip's method will be used to calculate _desiredJitterBufferFrames based on packet timegaps.
        bool _dynamicJitterBuffers;

        // settings for static jitter buffer mode
        int _staticDesiredJitterBufferFrames;

        // settings for dynamic jitter buffer mode
        bool _useStDevForJitterCalc;       // if true, philip's method is used.  otherwise, fred's method is used.
        int _windowStarveThreshold;
        int _windowSecondsForDesiredCalcOnTooManyStarves;
        int _windowSecondsForDesiredReduction;
    };

public:
    InboundAudioStream(int numFrameSamples, int numFramesCapacity, const Settings& settings);

    void reset();
    void resetStats();
    void clearBuffer();

    virtual int parseData(const QByteArray& packet);

    int popFrames(int maxFrames, bool allOrNothing, bool starveIfNoFramesPopped = true);
    int popSamples(int maxSamples, bool allOrNothing, bool starveIfNoSamplesPopped = true);

    bool lastPopSucceeded() const { return _lastPopSucceeded; };
    const AudioRingBuffer::ConstIterator& getLastPopOutput() const { return _lastPopOutput; }


    void setToStarved();


    void setSettings(const Settings& settings);

    void setMaxFramesOverDesired(int maxFramesOverDesired) { _maxFramesOverDesired = maxFramesOverDesired; }
    void setDynamicJitterBuffers(bool setDynamicJitterBuffers);
    void setStaticDesiredJitterBufferFrames(int staticDesiredJitterBufferFrames);
    void setUseStDevForJitterCalc(bool useStDevForJitterCalc) { _useStDevForJitterCalc = useStDevForJitterCalc; }
    void setWindowStarveThreshold(int windowStarveThreshold) { _starveThreshold = windowStarveThreshold; }
    void setWindowSecondsForDesiredCalcOnTooManyStarves(int windowSecondsForDesiredCalcOnTooManyStarves);
    void setWindowSecondsForDesiredReduction(int windowSecondsForDesiredReduction);
    

    virtual AudioStreamStats getAudioStreamStats() const;

    /// returns the desired number of jitter buffer frames under the dyanmic jitter buffers scheme
    int getCalculatedJitterBufferFrames() const { return _useStDevForJitterCalc ? 
        _calculatedJitterBufferFramesUsingStDev : _calculatedJitterBufferFramesUsingMaxGap; };

    /// returns the desired number of jitter buffer frames using Philip's method
    int getCalculatedJitterBufferFramesUsingStDev() const { return _calculatedJitterBufferFramesUsingStDev; }

    /// returns the desired number of jitter buffer frames using Freddy's method
    int getCalculatedJitterBufferFramesUsingMaxGap() const { return _calculatedJitterBufferFramesUsingMaxGap; }

    int getDesiredJitterBufferFrames() const { return _desiredJitterBufferFrames; }
    int getMaxFramesOverDesired() const { return _maxFramesOverDesired; }
    int getNumFrameSamples() const { return _ringBuffer.getNumFrameSamples(); }
    int getFrameCapacity() const { return _ringBuffer.getFrameCapacity(); }
    int getFramesAvailable() const { return _ringBuffer.framesAvailable(); }
    double getFramesAvailableAverage() const { return _framesAvailableStat.getAverage(); }

    bool isStarved() const { return _isStarved; }
    bool hasStarted() const { return _hasStarted; }

    int getConsecutiveNotMixedCount() const { return _consecutiveNotMixedCount; }
    int getStarveCount() const { return _starveCount; }
    int getSilentFramesDropped() const { return _silentFramesDropped; }
    int getOverflowCount() const { return _ringBuffer.getOverflowCount(); }

    int getPacketsReceived() const { return _incomingSequenceNumberStats.getReceived(); }

public slots:
    /// This function should be called every second for all the stats to function properly. If dynamic jitter buffers
    /// is enabled, those stats are used to calculate _desiredJitterBufferFrames.
    /// If the stats are not used and dynamic jitter buffers is disabled, it's not necessary to call this function.
    void perSecondCallbackForUpdatingStats();

private:
    void packetReceivedUpdateTimingStats();
    int clampDesiredJitterBufferFramesValue(int desired) const;

    int writeSamplesForDroppedPackets(int numSamples);

    void popSamplesNoCheck(int samples);
    void framesAvailableChanged();

protected:
    // disallow copying of InboundAudioStream objects
    InboundAudioStream(const InboundAudioStream&);
    InboundAudioStream& operator= (const InboundAudioStream&);

    /// parses the info between the seq num and the audio data in the network packet and calculates
    /// how many audio samples this packet contains (used when filling in samples for dropped packets).
    virtual int parseStreamProperties(PacketType type, const QByteArray& packetAfterSeqNum, int& numAudioSamples) = 0;

    /// parses the audio data in the network packet.
    /// default implementation assumes packet contains raw audio samples after stream properties
    virtual int parseAudioData(PacketType type, const QByteArray& packetAfterStreamProperties, int numAudioSamples);

    int writeDroppableSilentSamples(int numSilentSamples);
    
protected:

    AudioRingBuffer _ringBuffer;

    bool _lastPopSucceeded;
    AudioRingBuffer::ConstIterator _lastPopOutput;

    bool _dynamicJitterBuffers;         // if false, _desiredJitterBufferFrames is locked at 1 (old behavior)
    int _staticDesiredJitterBufferFrames;

    // if jitter buffer is dynamic, this determines what method of calculating _desiredJitterBufferFrames 
    // if true, Philip's timegap std dev calculation is used.  Otherwise, Freddy's max timegap calculation is used
    bool _useStDevForJitterCalc;

    int _desiredJitterBufferFrames;

    // if there are more than _desiredJitterBufferFrames + _maxFramesOverDesired frames, old ringbuffer frames
    // will be dropped to keep audio delay from building up
    int _maxFramesOverDesired;

    bool _isStarved;
    bool _hasStarted;

    // stats

    int _consecutiveNotMixedCount;
    int _starveCount;
    int _silentFramesDropped;
    int _oldFramesDropped;

    SequenceNumberStats _incomingSequenceNumberStats;

    quint64 _lastPacketReceivedTime;
    MovingMinMaxAvg<quint64> _timeGapStatsForDesiredCalcOnTooManyStarves;   // for Freddy's method
    int _calculatedJitterBufferFramesUsingMaxGap;
    StDev _stdevStatsForDesiredCalcOnTooManyStarves;                        // for Philip's method
    int _calculatedJitterBufferFramesUsingStDev;                     // the most recent desired frames calculated by Philip's method
    MovingMinMaxAvg<quint64> _timeGapStatsForDesiredReduction;

    int _starveHistoryWindowSeconds;
    RingBufferHistory<quint64> _starveHistory;
    int _starveThreshold;

    TimeWeightedAvg<int> _framesAvailableStat;

    // this value is periodically updated with the time-weighted avg from _framesAvailableStat. it is only used for
    // dropping silent frames right now.
    int _currentJitterBufferFrames;


    MovingMinMaxAvg<quint64> _timeGapStatsForStatsPacket;
};

#endif // hifi_InboundAudioStream_h
