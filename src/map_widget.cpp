/*
 *    Copyright 2012 Thomas Schöps
 *    Copyright 2013, 2014 Thomas Schöps, Kai Pastor
 *
 *    This file is part of OpenOrienteering.
 *
 *    OpenOrienteering is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    OpenOrienteering is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with OpenOrienteering.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "map_widget.h"

#include <QtWidgets>

#include "core/map_color.h"
#include "georeferencing.h"
#include "map.h"
#include "map_editor_activity.h"
#include "settings.h"
#include "template.h"
#include "tool.h"
#include "object.h"
#include "tool_edit.h"
#include "touch_cursor.h"
#include "util.h"
#include "gps_display.h"
#include "compass_display.h"
#include "gps_temporary_markers.h"
#include "gui/widgets/pie_menu.h"


MapWidget::MapWidget(bool show_help, bool force_antialiasing, QWidget* parent)
 : QWidget(parent),
   show_help(show_help),
   force_antialiasing(force_antialiasing),
   touch_cursor(NULL),
   gps_display(NULL),
   compass_display(NULL),
   marker_display(NULL)
{
	view = NULL;
	tool = NULL;
	dragging = false;
	drag_offset = QPoint(0, 0);
	below_template_cache = NULL;
	above_template_cache = NULL;
	map_cache = NULL;
	drawing_dirty_rect_old = QRect();
	drawing_dirty_rect_new = QRectF();
	drawing_dirty_rect_new_border = -1;
	activity_dirty_rect_old = QRect();
	activity_dirty_rect_new = QRectF();
	activity_dirty_rect_new_border = -1;
	zoom_label = NULL;
	cursorpos_label = NULL;
	objecttag_label = NULL;
	coords_type = MAP_COORDS;
	last_cursor_pos = MapCoordF(0, 0);
	current_pressed_buttons = 0;
	last_mouse_release_time = QTime::currentTime();
	
	below_template_cache_dirty_rect = rect();
	above_template_cache_dirty_rect = rect();
	map_cache_dirty_rect = rect();
	
	context_menu = new PieMenu(this);
// 	context_menu->setMinimumActionCount(8);
// 	context_menu->setIconSize(24);
	
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAutoFillBackground(false);
	setMouseTracking(true);
	setFocusPolicy(Qt::ClickFocus);
	setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
}

MapWidget::~MapWidget()
{
	if (view)
		view->removeMapWidget(this);
	
	delete below_template_cache;
	delete above_template_cache;
	delete map_cache;
	
	delete touch_cursor;
}

void MapWidget::setMapView(MapView* view)
{
	if (this->view != view)
	{
		if (this->view)
			this->view->removeMapWidget(this);
		
		this->view = view;
		
		if (view)
			view->addMapWidget(this);
		
		connect(view->getMap(), SIGNAL(objectSelectionChanged()), this, SLOT(updateObjectTagLabel()));
		
		update();
	}
}

void MapWidget::setTool(MapEditorTool* tool)
{
	// Redraw if touch cursor usage changes
	bool redrawTouchCursor = (touch_cursor && this->tool && tool
		&& (this->tool->usesTouchCursor() || tool->usesTouchCursor()));

	this->tool = tool;
	
	if (tool)
		setCursor(*tool->getCursor());
	else
		unsetCursor();
	if (redrawTouchCursor)
		touch_cursor->updateMapWidget(false);
}

void MapWidget::setActivity(MapEditorActivity* activity)
{
	this->activity = activity;
}

void MapWidget::applyMapTransform(QPainter* painter)
{
	painter->translate(width() / 2.0 + getMapView()->getDragOffset().x(),
					   height() / 2.0 + getMapView()->getDragOffset().y());
	getMapView()->applyTransform(painter);
}

QRectF MapWidget::viewportToView(const QRect& input)
{
	return QRectF(input.left() - 0.5*width() - drag_offset.x(), input.top() - 0.5*height() - drag_offset.y(), input.width(), input.height());
}
QPointF MapWidget::viewportToView(QPoint input)
{
	return QPointF(input.x() - 0.5*width() - drag_offset.x(), input.y() - 0.5*height() - drag_offset.y());
}
QPointF MapWidget::viewportToView(QPointF input)
{
	return QPointF(input.x() - 0.5*width() - drag_offset.x(), input.y() - 0.5*height() - drag_offset.y());
}
QRectF MapWidget::viewToViewport(const QRectF& input)
{
	return QRectF(input.left() + 0.5*width() + drag_offset.x(), input.top() + 0.5*height() + drag_offset.y(), input.width(), input.height());
}
QRectF MapWidget::viewToViewport(const QRect& input)
{
	return QRectF(input.left() + 0.5*width() + drag_offset.x(), input.top() + 0.5*height() + drag_offset.y(), input.width(), input.height());
}
QPointF MapWidget::viewToViewport(QPoint input)
{
	return QPointF(input.x() + 0.5*width() + drag_offset.x(), input.y() + 0.5*height() + drag_offset.y());
}
QPointF MapWidget::viewToViewport(QPointF input)
{
	return QPointF(input.x() + 0.5*width() + drag_offset.x(), input.y() + 0.5*height() + drag_offset.y());
}

MapCoord MapWidget::viewportToMap(QPoint input)
{
	return view->viewToMap(viewportToView(input));
}
MapCoordF MapWidget::viewportToMapF(QPoint input)
{
	return view->viewToMapF(viewportToView(input));
}
MapCoordF MapWidget::viewportToMapF(QPointF input)
{
	return view->viewToMapF(viewportToView(input));
}
QPointF MapWidget::mapToViewport(MapCoord input)
{
	return viewToViewport(view->mapToView(input));
}
QPointF MapWidget::mapToViewport(MapCoordF input)
{
	return viewToViewport(view->mapToView(input));
}
QPointF MapWidget::mapToViewport(QPointF input)
{
	return viewToViewport(view->mapToView(MapCoordF(input.x(), input.y())));
}
QRectF MapWidget::mapToViewport(const QRectF& input)
{
	QRectF result;
	rectIncludeSafe(result, mapToViewport(input.topLeft()));
	rectIncludeSafe(result, mapToViewport(input.bottomRight()));
	if (view->getRotation() != 0)
	{
		rectIncludeSafe(result, mapToViewport(input.topRight()));
		rectIncludeSafe(result, mapToViewport(input.bottomLeft()));
	}
	return result;
}

void MapWidget::zoom(float factor)
{
	Q_UNUSED(factor);
	
	// No need to update dirty rects, because everything is redrawn ...
	/*zoomDirtyRect(above_template_cache_dirty_rect);
	zoomDirtyRect(below_template_cache_dirty_rect);
	zoomDirtyRect(drawing_dirty_rect_old);
	zoomDirtyRect(activity_dirty_rect_old);*/
	
	updateEverything();
}

void MapWidget::updateEverythingInRect(const QRect& dirty_rect)
{
	below_template_cache_dirty_rect = dirty_rect;
	above_template_cache_dirty_rect = dirty_rect;
	map_cache_dirty_rect = dirty_rect;
	updateAllDirtyCaches();
	update(dirty_rect);
}

void MapWidget::moveView(qint64 x, qint64 y)
{
	Q_UNUSED(x);
	Q_UNUSED(y);
	// View moved externally
	updateEverything();
}

void MapWidget::panView(qint64 x, qint64 y)
{
	moveDirtyRect(above_template_cache_dirty_rect, -x, -y);
	moveDirtyRect(below_template_cache_dirty_rect, -x, -y);
	moveDirtyRect(drawing_dirty_rect_old, -x, -y);
	moveDirtyRect(activity_dirty_rect_old, -x, -y);
	
	MapCoordF screen_offset(x, y);
	screen_offset.rotate(view->getRotation());
	
	float px = view->lengthToPixel(screen_offset.getX());
	int ix = qRound(px);
	float py = view->lengthToPixel(screen_offset.getY());
	int iy = qRound(py);
	float int_deviation = qMax(qAbs(px - ix), qAbs(py - iy));
	
	ix = -ix;
	iy = -iy;
	
	// Only do a partial redraw in very specific circumstances where only very few objects are visible because a complete redraw is often faster
	bool partial_redraw = int_deviation < 0.01 && qAbs(px) < width() / 3.0f && qAbs(py) < height() / 3.0f;
	if (partial_redraw)
	{
		const int visible_objects_threshold = 200;
		int max_visible_objects = 0;
		if (ix > 0)
			max_visible_objects += view->getMap()->countObjectsInRect(view->calculateViewedRect(viewportToView(QRect(0, iy, ix, height() - iy))), false);
		else if (ix < 0)
			max_visible_objects += view->getMap()->countObjectsInRect(view->calculateViewedRect(viewportToView(QRect(width() + ix, iy, -ix, height() - iy))), false);
		
		if (max_visible_objects < visible_objects_threshold)
		{
			if (iy > 0)
				max_visible_objects += view->getMap()->countObjectsInRect(view->calculateViewedRect(viewportToView(QRect(0, 0, width(), iy))), false);
			else if (iy < 0)
				max_visible_objects += view->getMap()->countObjectsInRect(view->calculateViewedRect(viewportToView(QRect(0, height() + iy, width(), -iy))), false);
			
			if (max_visible_objects >= visible_objects_threshold)
				partial_redraw = false;
		}
		else
			partial_redraw = false;
	}
	
	if (partial_redraw)
	{
		// Update only the parts of the caches which have changed
		shiftCache(ix, iy, below_template_cache);
		shiftCache(ix, iy, above_template_cache);
		shiftCache(ix, iy, map_cache);
		
		if (ix > 0)
			updateEverythingInRect(QRect(0, iy, ix, height() - iy));
		else if (ix < 0)
			updateEverythingInRect(QRect(width() + ix, iy, -ix, height() - iy));
		
		if (iy > 0)
			updateEverythingInRect(QRect(0, 0, width(), iy));
		else if (iy < 0)
			updateEverythingInRect(QRect(0, height() + iy, width(), -iy));
	}
	else
	{
		// Update the whole caches
		below_template_cache_dirty_rect = rect();
		above_template_cache_dirty_rect = rect();
		map_cache_dirty_rect = rect();
		
		if (ix > 0)
			update(QRect(0, iy, ix, height() - iy));
		else if (ix < 0)
			update(QRect(width() + ix, iy, -ix, height() - iy));
		
		if (iy > 0)
			update(QRect(0, 0, width(), iy));
		else if (iy < 0)
			update(QRect(0, height() + iy, width(), -iy));
	}
}

void MapWidget::setDragOffset(QPoint offset)
{
	drag_offset = offset;
	update();
}

QPoint MapWidget::getDragOffset() const
{
	return drag_offset;
}

void MapWidget::completeDragging(qint64 dx, qint64 dy)
{
	drag_offset = QPoint(0, 0);
	panView(dx, dy);
}

void MapWidget::ensureVisibilityOfRect(const QRectF& map_rect, bool show_completely, bool zoom_in_steps)
{
	// Amount in pixels that is scrolled "too much" if the rect is not completely visible
	// TODO: change to absolute size using dpi value
	const int pixel_border = 70;
	QRectF viewport_rect = mapToViewport(map_rect);
	
	// TODO: this method assumes that the viewport is not rotated.
	
	if (!show_completely)
	{
		// Check if enough of the rect is visible
		QRectF intersected_rect = QRectF(rect()).intersected(viewport_rect);
		
		float min_visible_area = 120 * 100;
		float visible_area = intersected_rect.width() * intersected_rect.height();
		if (visible_area >= min_visible_area)
			return;
	}
	
	if (rect().contains(viewport_rect.topLeft().toPoint()) && rect().contains(viewport_rect.bottomRight().toPoint()))
		return;
	
	if (viewport_rect.left() < 0)
		view->setPositionX(view->getPositionX() + view->pixelToLength(viewport_rect.left() - pixel_border));
	else if (viewport_rect.right() > width())
		view->setPositionX(view->getPositionX() + view->pixelToLength(viewport_rect.right() - width() + pixel_border));
	
	if (viewport_rect.top() < 0)
		view->setPositionY(view->getPositionY() + view->pixelToLength(viewport_rect.top() - pixel_border));
	else if (viewport_rect.bottom() > height())
		view->setPositionY(view->getPositionY() + view->pixelToLength(viewport_rect.bottom() - height() + pixel_border));
	
	// If the rect is still not completely in view, we have to zoom out
	viewport_rect = mapToViewport(map_rect);
	if (!(rect().contains(viewport_rect.topLeft().toPoint()) && rect().contains(viewport_rect.bottomRight().toPoint())))
		adjustViewToRect(map_rect, zoom_in_steps);
}

void MapWidget::adjustViewToRect(const QRectF& map_rect, bool zoom_in_steps)
{
	const double log2 = log(2);
	view->setPositionX(qRound64(1000 * (map_rect.left() + map_rect.width() / 2)));
	view->setPositionY(qRound64(1000 * (map_rect.top() + map_rect.height() / 2)));
	
	if (map_rect.isValid())
	{
		// NOTE: The loop is an inelegant way to fight inaccuracies that occur somewhere ...
		const int pixel_border = 15;
		const float initial_zoom = view->getZoom();
		for (int i = 0; i < 10; ++i)
		{
			float zoom_factor = qMin(height() / (view->lengthToPixel(1000 * map_rect.height()) + 2*pixel_border),
			                         width() / (view->lengthToPixel(1000 * map_rect.width()) + 2*pixel_border));
			float zoom = view->getZoom() * zoom_factor;
			if (zoom_in_steps)
			{
				zoom = log(zoom) / log2;
				zoom = (zoom - log(initial_zoom) / log2) * 2.0;
				zoom = floor(zoom);
				zoom = (zoom * 0.5) + log(initial_zoom) / log2;
				zoom = pow(2, zoom);
			}
			view->setZoom(zoom);
		}
	}
}

void MapWidget::zoomDirtyRect(QRectF& dirty_rect, qreal zoom_factor)
{
	if (!dirty_rect.isValid())
		return;
	
	dirty_rect = QRectF(dirty_rect.topLeft() * zoom_factor, dirty_rect.bottomRight() * zoom_factor);
}

void MapWidget::zoomDirtyRect(QRect& dirty_rect, qreal zoom_factor)
{
	if (!dirty_rect.isValid())
		return;
	
	dirty_rect = QRect(dirty_rect.topLeft() * zoom_factor, dirty_rect.bottomRight() * zoom_factor);
}

void MapWidget::moveDirtyRect(QRectF& dirty_rect, qreal x, qreal y)
{
	if (!dirty_rect.isValid())
		return;
	
	dirty_rect.adjust(x, y, x, y);
}

void MapWidget::moveDirtyRect(QRect& dirty_rect, qreal x, qreal y)
{
	if (!dirty_rect.isValid())
		return;
	
	dirty_rect.adjust(x, y, x, y);
}

void MapWidget::markTemplateCacheDirty(QRectF view_rect, int pixel_border, bool front_cache)
{
	QRect* cache_dirty_rect = front_cache ? &above_template_cache_dirty_rect : &below_template_cache_dirty_rect;
	QRectF viewport_rect = viewToViewport(view_rect);
	QRect integer_rect = QRect(viewport_rect.left() - (1+pixel_border), viewport_rect.top() - (1+pixel_border),
							   viewport_rect.width() + 2*(1+pixel_border), viewport_rect.height() + 2*(1+pixel_border));
	
	if (!integer_rect.intersects(rect()))
		return;
	
	if (cache_dirty_rect->isValid())
		*cache_dirty_rect = cache_dirty_rect->united(integer_rect);
	else
		*cache_dirty_rect = integer_rect;
	
	update(integer_rect);
}

void MapWidget::markObjectAreaDirty(QRectF map_rect)
{
	const int pixel_border = 0;
	QRect viewport_rect = calculateViewportBoundingBox(map_rect, pixel_border);
	
	if (!viewport_rect.intersects(rect()))
		return;
	
	if (map_cache_dirty_rect.isValid())
		map_cache_dirty_rect = map_cache_dirty_rect.united(viewport_rect);
	else
		map_cache_dirty_rect = viewport_rect;
	
	update(viewport_rect);
}

void MapWidget::setDrawingBoundingBox(QRectF map_rect, int pixel_border, bool do_update)
{
	setDynamicBoundingBox(map_rect, pixel_border, drawing_dirty_rect_old, drawing_dirty_rect_new, drawing_dirty_rect_new_border, do_update);
}

void MapWidget::clearDrawingBoundingBox()
{
	clearDynamicBoundingBox(drawing_dirty_rect_old, drawing_dirty_rect_new, drawing_dirty_rect_new_border);
}

void MapWidget::setActivityBoundingBox(QRectF map_rect, int pixel_border, bool do_update)
{
	setDynamicBoundingBox(map_rect, pixel_border, activity_dirty_rect_old, activity_dirty_rect_new, activity_dirty_rect_new_border, do_update);
}

void MapWidget::clearActivityBoundingBox()
{
	clearDynamicBoundingBox(activity_dirty_rect_old, activity_dirty_rect_new, activity_dirty_rect_new_border);
}

void MapWidget::updateDrawing(QRectF map_rect, int pixel_border)
{
	QRect viewport_rect = calculateViewportBoundingBox(map_rect, pixel_border);
	
	if (viewport_rect.intersects(rect()))
		update(viewport_rect);
}

void MapWidget::updateDrawingLater(QRectF map_rect, int pixel_border)
{
	QRect viewport_rect = calculateViewportBoundingBox(map_rect, pixel_border);
	
	if (viewport_rect.intersects(rect()))
	{
		if (!cached_update_rect.isValid())
		{
			// Start the update timer
			QTimer::singleShot(15, this, SLOT(updateDrawingLaterSlot()));
		}
		
		// NOTE: this may require a mutex for concurrent access with updateDrawingLaterSlot()?
		rectIncludeSafe(cached_update_rect, viewport_rect);
	}
}

void MapWidget::updateDrawingLaterSlot()
{
	update(cached_update_rect);
	cached_update_rect = QRect();
}

void MapWidget::updateEverything()
{
	below_template_cache_dirty_rect = rect();
	above_template_cache_dirty_rect = rect();
	map_cache_dirty_rect = rect();
	update();
}

void MapWidget::setDynamicBoundingBox(QRectF map_rect, int pixel_border, QRect& dirty_rect_old, QRectF& dirty_rect_new, int& dirty_rect_new_border, bool do_update)
{
	dirty_rect_new = map_rect;
	dirty_rect_new_border = pixel_border;
	
	if (!do_update)
		return;
	
	QRect viewport_rect = calculateViewportBoundingBox(map_rect, pixel_border);
	
	if (!viewport_rect.intersects(rect()))
	{
		if (dirty_rect_old.isValid() && dirty_rect_old.intersects(rect()))
			update(dirty_rect_old);
		return;
	}
	
	if (dirty_rect_old.isValid())
		update(dirty_rect_old.united(viewport_rect));
	else
		update(viewport_rect);
}

void MapWidget::clearDynamicBoundingBox(QRect& dirty_rect_old, QRectF& dirty_rect_new, int& dirty_rect_new_border)
{
	if (!dirty_rect_new.isValid() && dirty_rect_new_border < 0)
		return;
	
	dirty_rect_new = QRectF();
	dirty_rect_new_border = -1;
	
	if (dirty_rect_old.isValid() && dirty_rect_old.intersects(rect()))
		update(dirty_rect_old);
}

QRect MapWidget::calculateViewportBoundingBox(QRectF map_rect, int pixel_border)
{
	QRectF view_rect = view->calculateViewBoundingBox(map_rect);
	QRectF viewport_rect = viewToViewport(view_rect);
	QRect integer_rect = QRect(viewport_rect.left() - (1+pixel_border), viewport_rect.top() - (1+pixel_border),
							   viewport_rect.width() + 2*(1+pixel_border), viewport_rect.height() + 2*(1+pixel_border));
	return integer_rect;
}

void MapWidget::setZoomLabel(QLabel* zoom_label)
{
	this->zoom_label = zoom_label;
	updateZoomLabel();
}

void MapWidget::setCursorposLabel(QLabel* cursorpos_label)
{
	this->cursorpos_label = cursorpos_label;
}

void MapWidget::setObjectTagLabel(QLabel *objecttag_label)
{
	this->objecttag_label = objecttag_label;
}

void MapWidget::updateZoomLabel()
{
	if (!zoom_label)
		return;
	
	zoom_label->setText(tr("%1x", "Zoom factor").arg(view->getZoom(), 0, 'g', 3));
}

void MapWidget::setCoordsDisplay(CoordsType type)
{
	coords_type = type;
	updateCursorposLabel(last_cursor_pos);
}

void MapWidget::updateCursorposLabel(const MapCoordF pos)
{
	last_cursor_pos = pos;
	
	if (!cursorpos_label)
		return;
	
	if (coords_type == MAP_COORDS)
	{
		cursorpos_label->setText( QString("%1 %2 (%3)").
		  arg(locale().toString(pos.getX(), 'f', 2)).
		  arg(locale().toString(-pos.getY(), 'f', 2)).
		  arg(tr("mm", "millimeters")));
	}
	else
	{
		const Georeferencing& georef = view->getMap()->getGeoreferencing();
		bool ok = true;
		if (coords_type == PROJECTED_COORDS)
		{
			const QPointF projected_point(georef.toProjectedCoords(pos));
			cursorpos_label->setText(
			  QString("%1 %2 (%3)").
			  arg(QString::number(projected_point.x(), 'f', 0)).
			  arg(QString::number(projected_point.y(), 'f', 0)).
			  arg(tr("m", "meters"))
			); 
		}
		else if (coords_type == GEOGRAPHIC_COORDS)
		{
			const LatLon lat_lon(georef.toGeographicCoords(pos, &ok));
			cursorpos_label->setText(
			  QString::fromUtf8("%1° %2°").
			  arg(locale().toString(georef.radToDeg(lat_lon.latitude), 'f', 6)).
			  arg(locale().toString(georef.radToDeg(lat_lon.longitude), 'f', 6))
			); 
		}
		else if (coords_type == GEOGRAPHIC_COORDS_DMS)
		{
			const LatLon lat_lon(georef.toGeographicCoords(pos, &ok));
			cursorpos_label->setText(
			  QString::fromUtf8("%1 %2").
			  arg(georef.radToDMS(lat_lon.latitude)).
			  arg(georef.radToDMS(lat_lon.longitude))
			); 
		}
		else
		{
			// shall never happen
			ok = false;
		}
		
		if (!ok)
			cursorpos_label->setText(tr("Error"));
	}
}

void MapWidget::updateObjectTagLabel(const MapCoordF pos)
{
	if (objecttag_label)
	{
		QString text;
		SelectionInfoVector objects;
		view->getMap()->findObjectsAt(pos, 0.001f * view->pixelToLength(5), false, false, false, true, objects);
		if (!objects.empty())
		{
			std::sort(objects.begin(), objects.end(), ObjectSelector::sortObjects);
			Object* object = objects.front().second;
			if (object->tags().contains("name"))
				text = object->tags()["name"];
		}
		objecttag_label->setText(text);
	}
}

void MapWidget::updateObjectTagLabel()
{
	updateObjectTagLabel(last_cursor_pos);
}

int MapWidget::getTimeSinceLastInteraction()
{
	if (current_pressed_buttons != 0)
		return 0;
	else
		return last_mouse_release_time.msecsTo(QTime::currentTime());
}

void MapWidget::setGPSDisplay(GPSDisplay* gps_display)
{
	this->gps_display = gps_display;
}

void MapWidget::setCompassDisplay(CompassDisplay* compass_display)
{
	this->compass_display = compass_display;
}

void MapWidget::setTemporaryMarkerDisplay(GPSTemporaryMarkers* marker_display)
{
	this->marker_display = marker_display;
}

QWidget* MapWidget::getContextMenu()
{
	return context_menu;
}

QSize MapWidget::sizeHint() const
{
    return QSize(640, 480);
}

void MapWidget::startPanning(QPoint cursor_pos)
{
	if (dragging)
		return;
	dragging = true;
	drag_start_pos = cursor_pos;
	normal_cursor = cursor();
	setCursor(Qt::ClosedHandCursor);
}

void MapWidget::finishPanning(QPoint cursor_pos)
{
	if (!dragging)
		return;
	dragging = false;
	view->completeDragging(cursor_pos - drag_start_pos);
	setCursor(normal_cursor);
}

void MapWidget::moveMap(int steps_x, int steps_y)
{
	const float move_factor = 1 / 4.0f;
	
	if (steps_x != 0)
	{
		float pixels_x = width() * steps_x * move_factor;
		view->setPositionX(view->getPositionX() + view->pixelToLength(pixels_x));
	}
	if (steps_y != 0)
	{
		float pixels_y = height() * steps_y * move_factor;
		view->setPositionY(view->getPositionY() + view->pixelToLength(pixels_y));
	}
}

void MapWidget::showHelpMessage(QPainter* painter, const QString& text)
{
	painter->fillRect(rect(), QColor(Qt::gray));
	
	QFont font = painter->font();
	font.setPointSize(2 * font.pointSize());
	font.setBold(true);
	painter->setFont(font);
	painter->drawText(QRect(0, 0, width(), height()), Qt::AlignCenter, text);
}

void MapWidget::paintEvent(QPaintEvent* event)
{
	// Draw on the widget
	QPainter painter;
	painter.begin(this);
	painter.setClipRect(event->rect());
	
	// Background color
	if (drag_offset.x() > 0)
		painter.fillRect(QRect(0, drag_offset.y(), drag_offset.x(), height() - drag_offset.y()), QColor(Qt::gray));
	else if (drag_offset.x() < 0)
		painter.fillRect(QRect(width() + drag_offset.x(), drag_offset.y(), -drag_offset.x(), height() - drag_offset.y()), QColor(Qt::gray));
	
	if (drag_offset.y() > 0)
		painter.fillRect(QRect(0, 0, width(), drag_offset.y()), QColor(Qt::gray));
	else if (drag_offset.y() < 0)
		painter.fillRect(QRect(0, height() + drag_offset.y(), width(), -drag_offset.y()), QColor(Qt::gray));
	
	// No colors defined? Provide a litte help message ...
	bool no_contents = view->getMap()->getNumObjects() == 0 && view->getMap()->getNumTemplates() == 0 && !view->isGridVisible();
	if (show_help && view && no_contents)
	{
		if (view->getMap()->getNumColors() == 0)
			showHelpMessage(&painter, tr("Empty map!\n\nStart by defining some colors:\nSelect Symbols -> Color window to\nopen the color dialog and\ndefine the colors there."));
		else if (view->getMap()->getNumSymbols() == 0)
			showHelpMessage(&painter, tr("No symbols!\n\nNow define some symbols:\nRight-click in the symbol bar\nand select \"New symbol\"\nto create one."));
		else
			showHelpMessage(&painter, tr("Ready to draw!\n\nStart drawing or load a base map.\nTo load a base map, click\nTemplates -> Open template...") + "\n\n" + tr("Hint: Hold the middle mouse button to drag the map,\nzoom using the mouse wheel, if available."));
	}
	else if (view)
	{
		// Update all dirty caches
		// TODO: It would be an idea to do these updates in a background thread and use the old caches in the meantime
		updateAllDirtyCaches();
		
		// TODO: Make sure that some cache (below_cache or map_cache) contains the background (white?) or it is drawn here
		
		// Draw caches
		if (!view->areAllTemplatesHidden() && isBelowTemplateVisible() && below_template_cache && view->getMap()->getFirstFrontTemplate() > 0)
			painter.drawImage(drag_offset, *below_template_cache, rect());
		else
			painter.fillRect(QRect(drag_offset.x(), drag_offset.y(), width(), height()), Qt::white);	// TODO: It's not as easy as that, see above.
		
		if (map_cache && view->getMapVisibility()->visible)
		{
			float map_opacity = view->getMapVisibility()->opacity;
			if (map_opacity < 1.0)
			{
				painter.save();
				painter.setOpacity(map_opacity);
				painter.drawImage(drag_offset, *map_cache, rect());
				painter.restore();
			}
			else
			{
				painter.drawImage(drag_offset, *map_cache, rect());
			}
		}
		
		if (!view->areAllTemplatesHidden() && isAboveTemplateVisible() && above_template_cache && view->getMap()->getNumTemplates() - view->getMap()->getFirstFrontTemplate() > 0)
			painter.drawImage(drag_offset, *above_template_cache, rect());
	}
	
	// Show current drawings
	if (activity_dirty_rect_new.isValid() || activity_dirty_rect_new_border >= 0)
	{
		QRect viewport_dirty_rect = calculateViewportBoundingBox(activity_dirty_rect_new, activity_dirty_rect_new_border);
		
		if (viewport_dirty_rect.intersects(event->rect()))
		{
			painter.setClipRect(viewport_dirty_rect.intersected(event->rect()));
			activity->draw(&painter, this);
		}
		
		activity_dirty_rect_old = viewport_dirty_rect;
	}
	
	if (drawing_dirty_rect_new.isValid() || drawing_dirty_rect_new_border >= 0)
	{
		QRect viewport_dirty_rect = calculateViewportBoundingBox(drawing_dirty_rect_new, drawing_dirty_rect_new_border);
		
		if (viewport_dirty_rect.intersects(event->rect()))
		{
			painter.setClipRect(viewport_dirty_rect.intersected(event->rect()));
			tool->draw(&painter, this);
		}
		
		drawing_dirty_rect_old = viewport_dirty_rect;
	}
	
	painter.setClipRect(event->rect());
	
	// Draw temporary GPS marker display
	if (marker_display)
		marker_display->paint(&painter);
	
	// Draw GPS display
	if (gps_display)
		gps_display->paint(&painter);
	
	// Draw touch cursor
	if (touch_cursor && tool && tool->usesTouchCursor())
		touch_cursor->paint(&painter);
	
	// Draw compass display
	if (compass_display)
		compass_display->paint(&painter);
	
	painter.end();
}

void MapWidget::resizeEvent(QResizeEvent* event)
{
	if (below_template_cache && below_template_cache->size() != event->size())
	{
		delete below_template_cache;
		below_template_cache = NULL;
		below_template_cache_dirty_rect = rect();
	}
	if (above_template_cache && above_template_cache->size() != event->size())
	{
		delete above_template_cache;
		above_template_cache = NULL;
		above_template_cache_dirty_rect = rect();
	}
	
	if (map_cache && map_cache->size() != event->size())
	{
		delete map_cache;
		map_cache = NULL;
		map_cache_dirty_rect = rect();
	}
}

void MapWidget::mousePressEvent(QMouseEvent* event)
{
	current_pressed_buttons = event->buttons();
	if (touch_cursor && tool && tool->usesTouchCursor())
	{
		touch_cursor->mousePressEvent(event);
		if (event->type() == QEvent::MouseMove)
		{
			_mouseMoveEvent(event);
			return;
		}
	}
	_mousePressEvent(event);
}

void MapWidget::_mousePressEvent(QMouseEvent* event)
{
	if (dragging)
	{
		event->accept();
		return;
	}
	
	if (tool && tool->mousePressEvent(event, view->viewToMapF(viewportToView(event->pos())), this))
	{
		event->accept();
		return;
	}
	
	if (event->button() == Qt::MiddleButton)
	{
		startPanning(event->pos());
		event->accept();
	}
}

void MapWidget::mouseMoveEvent(QMouseEvent* event)
{
	if (touch_cursor && tool && tool->usesTouchCursor())
	{
		if (!touch_cursor->mouseMoveEvent(event))
			return;
	}
	_mouseMoveEvent(event);
}

void MapWidget::_mouseMoveEvent(QMouseEvent* event)
{
	if (dragging)
	{
		view->setDragOffset(event->pos() - drag_start_pos);
		return;
	}
	else
    {
		updateCursorposLabel(view->viewToMapF(viewportToView(event->pos())));
        updateObjectTagLabel(view->viewToMapF(viewportToView(event->pos())));
    }
	
	if (tool && tool->mouseMoveEvent(event, view->viewToMapF(viewportToView(event->pos())), this))
	{
		event->accept();
		return;
	}
}

void MapWidget::mouseReleaseEvent(QMouseEvent* event)
{
	current_pressed_buttons = event->buttons();
	last_mouse_release_time = QTime::currentTime();
	if (touch_cursor && tool && tool->usesTouchCursor())
	{
		if (!touch_cursor->mouseReleaseEvent(event))
			return;
	}
	_mouseReleaseEvent(event);
}

void MapWidget::_mouseReleaseEvent(QMouseEvent* event)
{
	if (dragging)
	{
		finishPanning(event->pos());
		event->accept();
		return;
	}
	
	if (tool && tool->mouseReleaseEvent(event, view->viewToMapF(viewportToView(event->pos())), this))
	{
		event->accept();
		return;
	}
}

void MapWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (touch_cursor && tool && tool->usesTouchCursor())
	{
		if (!touch_cursor->mouseDoubleClickEvent(event))
			return;
	}
	_mouseDoubleClickEvent(event);
}

void MapWidget::_mouseDoubleClickEvent(QMouseEvent* event)
{
	if (tool && tool->mouseDoubleClickEvent(event, view->viewToMapF(viewportToView(event->pos())), this))
	{
		event->accept();
		return;
	}
	
	QWidget::mouseDoubleClickEvent(event);
}

void MapWidget::wheelEvent(QWheelEvent* event)
{
	if (event->orientation() == Qt::Vertical)
	{
		float degrees = event->delta() / 8.0f;
		float num_steps = degrees / 15.0f;
		
		if (view)
		{
			bool preserve_cursor_pos = (event->modifiers() & Qt::ControlModifier) == 0;
			if (num_steps < 0 && !Settings::getInstance().getSettingCached(Settings::MapEditor_ZoomOutAwayFromCursor).toBool())
				preserve_cursor_pos = !preserve_cursor_pos;
			view->zoomSteps(num_steps, preserve_cursor_pos, viewportToView(event->pos()));
			
			// Send a mouse move event to the current tool as zooming out can move the mouse position on the map
			if (tool)
			{
				QMouseEvent* mouse_event = new QMouseEvent(QEvent::HoverMove, event->pos(), Qt::NoButton, QApplication::mouseButtons(), Qt::NoModifier);
				tool->mouseMoveEvent(mouse_event, view->viewToMapF(viewportToView(event->pos())), this);
				delete mouse_event;
			}
		}
		
		event->accept();
	}
	else
		event->ignore();
}

void MapWidget::leaveEvent(QEvent* event)
{
	if (tool)
		tool->leaveEvent(event);
}

bool MapWidget::keyPressEventFilter(QKeyEvent* event)
{
	if (tool && tool->keyPressEvent(event))
	{
		return true;
	}
	
	switch (event->key())
	{
	case Qt::Key_F6:
		startPanning(mapFromGlobal(QCursor::pos()));
		return true;
		
	case Qt::Key_Up:
		moveMap(0, -1);
		return true;
		
	case Qt::Key_Down:
		moveMap(0, 1);
		return true;
		
	case Qt::Key_Left:
		moveMap(-1, 0);
		return true;
		
	case Qt::Key_Right:
		moveMap(1, 0);
		return true;
		
	default:
		return false;
	}
}

bool MapWidget::keyReleaseEventFilter(QKeyEvent* event)
{
	if (tool && tool->keyReleaseEvent(event))
	{
		return true;
	}
	
	return false;
}

void MapWidget::enableTouchCursor(bool enabled)
{
	if (enabled && !touch_cursor)
		touch_cursor = new TouchCursor(this);
	else if (!enabled && touch_cursor)
	{
		touch_cursor->updateMapWidget(false);
		delete touch_cursor;
		touch_cursor = NULL;
	}
}

void MapWidget::focusOutEvent(QFocusEvent* event)
{
	if (tool)
		tool->focusOutEvent(event);
	QWidget::focusOutEvent(event);
}

void MapWidget::contextMenuEvent(QContextMenuEvent* event)
{
	if (!context_menu->isEmpty())
		context_menu->popup(event->globalPos());
	
	event->accept();
}

bool MapWidget::containsVisibleTemplate(int first_template, int last_template)
{
	if (first_template > last_template)
		return false;	// no template visible
		
	Map* map = view->getMap();
	for (int i = first_template; i <= last_template; ++i)
	{
		if (view->isTemplateVisible(map->getTemplate(i)))
			return true;
	}
	
	return false;
}

void MapWidget::updateTemplateCache(QImage*& cache, QRect& dirty_rect, int first_template, int last_template, bool use_background)
{
	Q_ASSERT(containsVisibleTemplate(first_template, last_template));
	
	if (!cache)
	{
		// Lazy allocation of cache image
		cache = new QImage(size(), QImage::Format_ARGB32_Premultiplied);
		dirty_rect = rect();
	}
	
	// Make sure not to use a bigger draw rect than necessary
	dirty_rect = dirty_rect.intersected(rect());
	
	// Start drawing
	QPainter painter;
	painter.begin(cache);
	painter.setClipRect(dirty_rect);
	
	// Fill with background color (TODO: make configurable)
	if (use_background)
		painter.fillRect(dirty_rect, Qt::white);
	else
	{
		QPainter::CompositionMode mode = painter.compositionMode();
		painter.setCompositionMode(QPainter::CompositionMode_Clear);
		painter.fillRect(dirty_rect, Qt::transparent);
		painter.setCompositionMode(mode);
	}
	
	// Draw templates
	painter.save();
	painter.translate(width() / 2.0, height() / 2.0);
	view->applyTransform(&painter);
	
	Map* map = view->getMap();
	QRectF map_view_rect = view->calculateViewedRect(viewportToView(dirty_rect));
	
	map->drawTemplates(&painter, map_view_rect, first_template, last_template, view, true);
	
	painter.restore();
	painter.end();
	
	dirty_rect.setWidth(-1); // => !dirty_rect.isValid()
}

void MapWidget::updateMapCache(bool use_background)
{
	if (!map_cache)
	{
		// Lazy allocation of cache image
		map_cache = new QImage(size(), QImage::Format_ARGB32_Premultiplied);
		map_cache_dirty_rect = rect();
	}
	
	// Make sure not to use a bigger draw rect than necessary
	map_cache_dirty_rect = map_cache_dirty_rect.intersected(rect());
	
	// Start drawing
	QPainter painter;
	painter.begin(map_cache);
	painter.setClipRect(map_cache_dirty_rect);
	
	// Fill with background color (TODO: make configurable)
	if (use_background)
		painter.fillRect(map_cache_dirty_rect, Qt::white);
	else
	{
		QPainter::CompositionMode mode = painter.compositionMode();
		painter.setCompositionMode(QPainter::CompositionMode_Clear);
		painter.fillRect(map_cache_dirty_rect, Qt::transparent);
		painter.setCompositionMode(mode);
	}
	
	bool use_antialiasing = force_antialiasing || Settings::getInstance().getSettingCached(Settings::MapDisplay_Antialiasing).toBool();
	if (use_antialiasing)
		painter.setRenderHint(QPainter::Antialiasing);
		
	Map* map = view->getMap();
	QRectF map_view_rect = view->calculateViewedRect(viewportToView(map_cache_dirty_rect));

	painter.translate(width() / 2.0, height() / 2.0);
	view->applyTransform(&painter);
	if (view->isOverprintingSimulationEnabled())
		map->drawOverprintingSimulation(&painter, map_view_rect, !use_antialiasing, view->calculateFinalZoomFactor(), true, true);
	else
		map->draw(&painter, map_view_rect, !use_antialiasing, view->calculateFinalZoomFactor(), true, true);
	
	if (view->isGridVisible())
		map->drawGrid(&painter, map_view_rect, true);
	
	// Finish drawing
	painter.end();
	
	map_cache_dirty_rect.setWidth(-1); // => !map_cache_dirty_rect.isValid()
}

void MapWidget::updateAllDirtyCaches()
{
	if (isBelowTemplateVisible() && below_template_cache_dirty_rect.isValid())
		updateTemplateCache(below_template_cache, below_template_cache_dirty_rect, 0, view->getMap()->getFirstFrontTemplate() - 1, true);
	if (isAboveTemplateVisible() && above_template_cache_dirty_rect.isValid())
		updateTemplateCache(above_template_cache, above_template_cache_dirty_rect, view->getMap()->getFirstFrontTemplate(), view->getMap()->getNumTemplates() - 1, false);
	
	if (map_cache_dirty_rect.isValid())
		updateMapCache(false);
}

void MapWidget::shiftCache(int sx, int sy, QImage*& cache)
{
	if (!cache) return;
	QImage* new_cache = new QImage(cache->size(), cache->format());
	QPainter painter;
	painter.begin(new_cache);
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.drawImage(sx, sy, *cache);
	painter.end();
	delete cache;
	cache = new_cache;
}