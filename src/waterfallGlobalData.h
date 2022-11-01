/* -*- c++ -*- */
/*
 * Copyright 2012 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef WATERFALL_GLOBAL_DATA_HPP
#define WATERFALL_GLOBAL_DATA_HPP

#include <qwt_raster_data.h>
#include <cinttypes>

#if QWT_VERSION >= 0x060000
#include <qwt_interval.h>

typedef QwtInterval QwtDoubleInterval;
#endif

enum WaterfallFlowDirection {
    UP,
    DOWN
};

class WaterfallData : public QwtRasterData
{
public:
    WaterfallData(const double, const double, const uint64_t, const unsigned int);
    ~WaterfallData() override;

    virtual void reset();
    virtual void copy(const WaterfallData*);

    virtual void
    resizeData(const double, const double, const uint64_t, const int history = 0);

    virtual QwtRasterData* copy() const;

#if QWT_VERSION < 0x060000
    virtual QwtDoubleInterval range() const;
    virtual void setRange(const QwtDoubleInterval&);
#elif QWT_VERSION >= 0x060200
    virtual QwtInterval interval(Qt::Axis) const;
    void setInterval(Qt::Axis, const QwtInterval&);
#endif

    double value(double x, double y) const override;

    virtual uint64_t getNumFFTPoints() const;
    virtual void addFFTData(const double*, const uint64_t, const int);

    virtual const double* getSpectrumDataBuffer() const;
    virtual void setSpectrumDataBuffer(const double*);

    virtual int getNumLinesToUpdate() const;
    virtual void setNumLinesToUpdate(const int);
    virtual void incrementNumLinesToUpdate();

    void setFlowDirection(WaterfallFlowDirection direction);
    WaterfallFlowDirection getFlowDirection();
protected:
    std::vector<double> _spectrumData;
    uint64_t _fftPoints;
    uint64_t _historyLength;
    int _numLinesToUpdate;
    WaterfallFlowDirection flow_direction;

#if QWT_VERSION < 0x060000
    QwtDoubleInterval _intensityRange;
#else
    QwtInterval _intensityRange;
#endif

#if QWT_VERSION >= 0x060200
    QwtInterval d_intervals[3];
#endif

private:
};

#endif /* WATERFALL_GLOBAL_DATA_HPP */
