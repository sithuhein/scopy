/*
 * Copyright (c) 2019 Analog Devices Inc.
 *
 * This file is part of Scopy
 * (see http://www.github.com/analogdevicesinc/scopy).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SISMOGRAPH_HPP
#define SISMOGRAPH_HPP

#include <QVector>
#include <QWidget>
#include <QMap>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>

#include "autoScaler.hpp"
#include "customFifo.hpp"

#include "gui/customqwtscaledraw.hpp"

namespace adiscope {

	class Sismograph : public QwtPlot
	{
		Q_OBJECT

		Q_PROPERTY(int numSamples
				READ getNumSamples
				WRITE setNumSamples
		)

		Q_PROPERTY(double sampleRate
				READ getSampleRate
				WRITE setSampleRate
		)

	public:
		explicit Sismograph(QWidget *parent = nullptr);
		~Sismograph();

		int getNumSamples() const;
		void setNumSamples(int num);
		double getSampleRate() const;
		void setSampleRate(double rate);

        void setUnitOfMeasure(QString unitOfMeasureName,QString unitOfMeasureSymbol);
        void setPlotAxisXTitle(QString title);

	public Q_SLOTS:
		void plot(double sample);
		void reset();
		void setColor(const QColor& color);
		void updateScale(const QwtScaleDiv);
		void updateYScale(double max, double min);
		void setLineWidth(qreal width);
		void setLineStyle(Qt::PenStyle lineStyle);
		void setHistoryDuration(double time);

	private:
		QwtPlotCurve curve;
		unsigned int numSamples;
		double sampleRate;
		AutoScaler *scaler;
        double m_currentScale;
        QString m_unitOfMeasureName;
        QString m_unitOfMeasureSymbol;

		QVector<double> ydata;
		QVector<double> xdata;

		void updateScale();
		double findMaxInFifo();

     Q_SIGNALS:
        void dataChanged(std::vector<double> data);

    private:
         CustomQwtScaleDraw* scaleLabel;
		 double m_currentMaxValue;

	};
}

#endif /* SISMOGRAPH_HPP */
