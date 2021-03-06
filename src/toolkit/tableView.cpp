/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "tableView.hpp"
#include <QMouseEvent>

namespace qt
{
namespace toolkit
{

void TableView::mousePressEvent(QMouseEvent* event) {
#if 1
	auto const index{indexAt(event->pos())};
	if (!index.isValid()) {
		setCurrentIndex(index);
	} else {
		QTableView::mousePressEvent(event);
	}
#else
	QTableView::mousePressEvent(event);
#endif
}

} // namespace toolkit
} // namespace qt
