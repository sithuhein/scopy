/*
 * Copyright (c) 2022 frmdstryr <frmdstryr@protonmail.com>
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
#include "decoder_table_model.hpp"

#include <QDebug>
#include <QHeaderView>

namespace adiscope {


namespace logic {

DecoderTableModel::DecoderTableModel(DecoderTable *decoderTable, LogicAnalyzer *logicAnalyzer):
    QAbstractTableModel(decoderTable),
    m_decoderTable(decoderTable),
    m_logic(logicAnalyzer)
{

}

int DecoderTableModel::rowCount(const QModelIndex &parent) const
{
    // Return max of all packets
    size_t count = 0;
    for (const auto &entry: m_dataset) {
        const auto &curve = entry.first;
        for (const auto &rowmap: curve->getAnnotationRows()) {
            if (rowmap.first.index() == entry.second) {
                count = std::max(count, rowmap.second.size());
            }
        }
    }
    // qDebug() << "Decoder table count: " << count << Qt::endl;
    return count;
}
int DecoderTableModel::columnCount(const QModelIndex &parent) const
{
    // One column per curve
    return m_dataset.size();
}


void DecoderTableModel::activate()
{
    for (const auto &curve: m_curves) {
        if (curve.isNull()) continue;
        QObject::connect(
            curve,
            &AnnotationCurve::annotationsChanged,
            this,
            &DecoderTableModel::annotationsChanged
        );
    }
    m_active = true;
}

void DecoderTableModel::deactivate()
{
    // Disconnect signals
    for (const auto &curve: m_curves) {
        if (curve.isNull()) continue;
        disconnect(curve);
    }
    m_active = false;
}

void DecoderTableModel::reloadDecoders(bool logic)
{
    // Disconnect signals
    deactivate();

    // TODO: At some point this should only update changed
    m_curves.clear();
    m_dataset.clear();

    // Reconnect signals for all the annotation curves
    for (const auto &curve: m_logic->getPlotCurves(logic)) {
		if (const auto annCurve = dynamic_cast<AnnotationCurve *>(curve)) {
            m_curves.emplace_back(annCurve);
            m_dataset.emplace(annCurve, annotationIndexMapping(annCurve));
		}
    }

    // Reconnect signals
    activate();

    // Recompute samples
    annotationsChanged();
}

int DecoderTableModel::annotationIndexMapping(const AnnotationCurve* curve) const
{
    assert(curve);
    for (const auto &rowmap: curve->getAnnotationRows()) {
        const Row &row = rowmap.first;
        const auto title = row.title();
	if (title == "Parallel: Items") {
            return row.index();
	}
        // TODO: What are the others???
    }
    qDebug() << "No mapping found for decoder table" << Qt::endl;
    return -1;
}

void DecoderTableModel::annotationsChanged()
{
    beginResetModel();

    int visibleRows = 0;
    int rowHeight = 20;
    for (const auto &entry: m_dataset) {
        const auto &curve = entry.first;
        const int activeIndex = entry.second;
        if (curve.isNull()) continue;
        visibleRows = std::max(visibleRows, curve->getVisibleRows());
        rowHeight = std::max(rowHeight, static_cast<int>(curve->getTraceHeight()));

        // This is for debugging, can be removed
        for (const auto &rowmap: curve->getAnnotationRows()) {
            const Row &row = rowmap.first;
            const RowData &data = rowmap.second;
            const char* active = (row.index() == activeIndex) ? " (active)": "";
            qDebug() << "decoder: " << row.index() << "-"
                << row.title() << " size: " << data.size() << active << Qt::endl;
        }
    }
    endResetModel();

    const auto verticalHeader = m_decoderTable->verticalHeader();
    const int spacing = 5;
    verticalHeader->setDefaultSectionSize(rowHeight * visibleRows + spacing);
    verticalHeader->sectionResizeMode(QHeaderView::Fixed);
}

QVariant DecoderTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole and orientation == Qt::Horizontal) {
        if (section < 0 or static_cast<size_t>(section) >= m_curves.size()) {
            return QVariant();
        }

        const auto &curve = m_curves.at(section);
        if (curve.isNull()) {
            return QVariant();
        }

        const auto annotationIndex = m_dataset.at(curve);

        // Use decoder class name
        for (const auto &entry: curve->getAnnotationRows()) {
            const Row &row = entry.first;
            if (row.index() != annotationIndex) continue;
            if (row.title().size() > 0) {
                return row.title();
            }
        }
        return QString("No decoder");
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

QVariant DecoderTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();

    if (role == Qt::DisplayRole) {
        const size_t col = index.column();
        if ( col >= m_curves.size() ) return QVariant();
        const auto &curve = m_curves.at(col);
        if (curve.isNull()) return QVariant();

        const auto annotationIndex = m_dataset.at(curve);
        if (annotationIndex < 0) return QVariant();

        // Get annotation data by row
        const size_t row = index.row();
        for (const auto &entry: curve->getAnnotationRows()) {
            if (entry.first.index() != annotationIndex) continue;
            const RowData& data = entry.second;
            if (row < data.size()) {
                const Annotation &ann = data.getAnnAt(row);
                return QVariant::fromValue(DecoderTableItem(
                    curve,
                    ann.start_sample(),
                    ann.end_sample()
                ));
            }
        }
    }
    return QVariant();
}

} // namespace logic
} // namespace adiscope
