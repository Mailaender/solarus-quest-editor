/*
 * Copyright (C) 2014-2016 Christopho, Solarus - http://www.solarus-games.org
 *
 * Solarus Quest Editor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Solarus Quest Editor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "widgets/gui_tools.h"
#include "widgets/pan_tool.h"
#include "widgets/sprite_scene.h"
#include "widgets/sprite_view.h"
#include "widgets/zoom_tool.h"
#include "view_settings.h"
#include <QAction>
#include <QApplication>
#include <QGraphicsItem>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>

namespace SolarusEditor {

/**
 * @brief Creates a sprite view.
 * @param parent The parent widget or nullptr.
 */
SpriteView::SpriteView(QWidget* parent) :
  QGraphicsView(parent),
  scene(nullptr),
  delete_direction_action(nullptr),
  state(State::NORMAL),
  current_area_item(nullptr),
  view_settings(nullptr),
  zoom(1.0) {

  setAlignment(Qt::AlignTop | Qt::AlignLeft);

  delete_direction_action = new QAction(
        QIcon(":/images/icon_delete.png"), tr("Delete..."), this);
  delete_direction_action->setShortcut(QKeySequence::Delete);
  delete_direction_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(delete_direction_action, SIGNAL(triggered()),
          this, SIGNAL(delete_selected_direction_requested()));
  addAction(delete_direction_action);
  duplicate_direction_action = new QAction(
        QIcon(":/images/icon_copy.png"), tr("Duplicate..."), this);
  // TODO: set a shortcut to duplicate a direction
  connect(duplicate_direction_action, SIGNAL(triggered()),
          this, SLOT(duplicate_selected_direction_requested()));
  addAction(duplicate_direction_action);

  ViewSettings* view_settings = new ViewSettings(this);
  set_view_settings(*view_settings);
}

/**
 * @brief Returns the sprite scene represented in this view.
 * @return The scene or nullptr if no sprite was set.
 */
SpriteScene* SpriteView::get_scene() {
  return scene;
}

/**
 * @brief Sets the sprite to represent in this view.
 * @param model The sprite model, or nullptr to remove any model.
 * This class does not take ownership on the model.
 * The model can be deleted safely.
 */
void SpriteView::set_model(SpriteModel* model) {

  if (this->model != nullptr) {
    this->model = nullptr;
    this->scene = nullptr;
  }

  this->model = model;

  if (model != nullptr) {
    // Create the scene from the model.
    scene = new SpriteScene(*model, this);
    setScene(scene);

    // Enable useful features if there is an image.
    setDragMode(QGraphicsView::RubberBandDrag);

    if (view_settings != nullptr) {
      view_settings->set_zoom(2.0);  // Initial zoom: x2.
    }
    horizontalScrollBar()->setValue(0);
    verticalScrollBar()->setValue(0);

    // Install panning and zooming helpers.
    new PanTool(this);
    new ZoomTool(this);
  }
}

/**
 * @brief Sets the view settings for this view.
 *
 * When they change, the view is updated accordingly.
 *
 * @param view_settings The settings to watch.
 */
void SpriteView::set_view_settings(ViewSettings& view_settings) {

  this->view_settings = &view_settings;

  connect(&view_settings, SIGNAL(zoom_changed(double)),
          this, SLOT(update_zoom()));
  update_zoom();

  connect(this->view_settings, SIGNAL(grid_visibility_changed(bool)),
          this, SLOT(update_grid_visibility()));
  connect(this->view_settings, SIGNAL(grid_size_changed(QSize)),
          this, SLOT(update_grid_visibility()));
  connect(this->view_settings, SIGNAL(grid_style_changed(GridStyle)),
          this, SLOT(update_grid_visibility()));
  connect(this->view_settings, SIGNAL(grid_color_changed(QColor)),
          this, SLOT(update_grid_visibility()));
  update_grid_visibility();

  horizontalScrollBar()->setValue(0);
  verticalScrollBar()->setValue(0);
}

/**
 * @brief Sets the zoom level of the view from the settings.
 *
 * Zooming will be anchored at the mouse position.
 * The zoom value will be clamped between 0.25 and 4.0.
 */
void SpriteView::update_zoom() {

  if (view_settings == nullptr) {
    return;
  }

  double zoom = view_settings->get_zoom();
  zoom = qMin(4.0, qMax(0.25, zoom));

  if (zoom == this->zoom) {
    return;
  }

  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  double scale_factor = zoom / this->zoom;
  scale(scale_factor, scale_factor);
  this->zoom = zoom;
}

/**
 * @brief Scales the view by a factor of 2.
 *
 * Zooming will be anchored at the mouse position.
 * The maximum zoom value is 4.0: this function does nothing if you try to
 * zoom more.
 */
void SpriteView::zoom_in() {

  if (view_settings == nullptr) {
    return;
  }

  view_settings->set_zoom(view_settings->get_zoom() * 2.0);
}

/**
 * @brief Scales the view by a factor of 0.5.
 *
 * Zooming will be anchored at the mouse position.
 * The maximum zoom value is 0.25: this function does nothing if you try to
 * zoom less.
 */
void SpriteView::zoom_out() {

  if (view_settings == nullptr) {
    return;
  }

  view_settings->set_zoom(view_settings->get_zoom() / 2.0);
}

/**
 * @brief Shows or hides the grid according to the view settings.
 */
void SpriteView::update_grid_visibility() {

  if (view_settings == nullptr) {
    return;
  }

  if (view_settings->is_grid_visible()) {
    // Necessary to correctly show the grid when scrolling,
    // because it is part of the foreground, not of graphics items.
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  }
  else {
    // Faster.
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
  }

  if (scene != nullptr) {
    // The foreground has changed.
    scene->invalidate();
  }
}

/**
 * @brief Slot called when the user asks for ducplicate the selected direction.
 */
void SpriteView::duplicate_selected_direction_requested() {

  SpriteModel::Index index = model->get_selected_index();
  if (!index.is_direction_index()) {
    return;
  }
  QPoint posistion = model->get_direction_position(index);
  emit duplicate_selected_direction_requested(posistion);
}

/**
 * @brief Draws the sprite view.
 * @param event The paint event.
 */
void SpriteView::paintEvent(QPaintEvent* event) {

  QGraphicsView::paintEvent(event);

  if (view_settings == nullptr || !view_settings->is_grid_visible()) {
    return;
  }

  QSize grid = view_settings->get_grid_size();
  QRect rect = event->rect();
  rect.setTopLeft(mapFromScene(0, 0));

  // Draw the grid.
  QPainter painter(viewport());
  GuiTools::draw_grid(
    painter, rect, grid * zoom, view_settings->get_grid_color(),
    view_settings->get_grid_style());
}

/**
 * @brief Receives a mouse press event.
 *
 * Reimplemented to handle the selection.
 *
 * @param event The event to handle.
 */
void SpriteView::mousePressEvent(QMouseEvent* event) {

  if (model == nullptr) {
    return;
  }

  if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {

    // Left or right button: possibly change the selection.
    QList<QGraphicsItem*> items_under_mouse = items(
          QRect(event->pos(), QSize(1, 1)),
          Qt::IntersectsItemBoundingRect  // Pick transparent items too.
          );
    QGraphicsItem* item = items_under_mouse.empty() ? nullptr : items_under_mouse.first();

    if (item != nullptr) {
      if (!item->isSelected()) {
        // Select the item.
        scene->clearSelection();
        item->setSelected(true);
      }
      if (event->button() == Qt::LeftButton &&
               model->get_selected_index().is_direction_index()) {
        // Allow to move it.
        start_state_moving_direction(event->pos());
      }
    }
    else {
      // Left click outside items: trace a selection rectangle.
      scene->clearSelection();
      start_state_drawing_rectangle(event->pos());
    }
  }
}

/**
 * @brief Receives a mouse release event.
 *
 * Reimplemented to scroll the view when the middle mouse button is pressed.
 *
 * @param event The event to handle.
 */
void SpriteView::mouseReleaseEvent(QMouseEvent* event) {

  if (model == nullptr) {
    return;
  }

  if (state == State::DRAWING_RECTANGLE) {
    end_state_drawing_rectangle();
  }
  else if (state == State::MOVING_DIRECTION) {
    end_state_moving_direction();
  }

  QGraphicsView::mouseReleaseEvent(event);
}

/**
 * @brief Receives a mouse move event.
 *
 * Reimplemented to scroll the view when the middle mouse button is pressed.
 *
 * @param event The event to handle.
 */
void SpriteView::mouseMoveEvent(QMouseEvent* event) {

  if (model == nullptr) {
    return;
  }

  if (state == State::DRAWING_RECTANGLE) {

    // Compute the selected area.
    QPoint dragging_previous_point = dragging_current_point;
    dragging_current_point = mapToScene(event->pos()).toPoint() / 8 * 8;

    if (dragging_current_point != dragging_previous_point) {

      QRect new_direction_area;

      // The area has changed: recalculate the rectangle.
      if (dragging_start_point.x() < dragging_current_point.x()) {
        new_direction_area.setX(dragging_start_point.x());
        new_direction_area.setWidth(dragging_current_point.x() - dragging_start_point.x());
      }
      else {
        new_direction_area.setX(dragging_current_point.x());
        new_direction_area.setWidth(dragging_start_point.x() - dragging_current_point.x());
      }

      if (dragging_start_point.y() < dragging_current_point.y()) {
        new_direction_area.setY(dragging_start_point.y());
        new_direction_area.setHeight(dragging_current_point.y() - dragging_start_point.y());
      }
      else {
        new_direction_area.setY(dragging_current_point.y());
        new_direction_area.setHeight(dragging_start_point.y() - dragging_current_point.y());
      }

      set_current_area(new_direction_area);
    }
  }
  else if (state == State::MOVING_DIRECTION) {

    SpriteModel::Index index = model->get_selected_index();
    if (!index.is_direction_index()) {
      // Direction was deselected: cancel the movement.
      end_state_moving_direction();
    }
    else {
      dragging_current_point = mapToScene(event->pos()).toPoint() / 8 * 8;

      QRect new_direction_area = current_area_item->rect().toRect();
      QRect old_direction_area = model->get_direction_all_frames_rect(index);
      new_direction_area.moveTopLeft(QPoint(
            old_direction_area.x() + dragging_current_point.x() - dragging_start_point.x(),
            old_direction_area.y() + dragging_current_point.y() - dragging_start_point.y()));

      set_current_area(new_direction_area);
    }
  }

  // The parent class tracks mouse movements for internal needs
  // such as anchoring the viewport to the mouse when zooming.
  QGraphicsView::mouseMoveEvent(event);
}

/**
 * @brief Receives a context menu event.
 * @param event The event to handle.
 */
void SpriteView::contextMenuEvent(QContextMenuEvent* event) {

  if (scene == nullptr) {
    return;
  }

  QPoint where;
  if (event->pos() != QPoint(0, 0)) {
    where = event->pos();
  }
  else {
    QList<QGraphicsItem*> selected_items = scene->selectedItems();
    where = mapFromScene(selected_items.first()->pos() + QPoint(8, 8));
  }

  show_context_menu(where);
}

/**
 * @brief Shows a context menu with actions relative to the selected directions.
 *
 * Does nothing if the view is in read-only mode.
 *
 * @param where Where to show the menu, in view coordinates.
 */
void SpriteView::show_context_menu(const QPoint& where) {

  if (model == nullptr) {
    return;
  }

  SpriteModel::Index index = model->get_selected_index();
  if (!index.is_direction_index()) {
    return;
  }

  QMenu* menu = new QMenu(this);

  // Delete direction.
  menu->addAction(delete_direction_action);
  menu->addAction(duplicate_direction_action);
  menu->addSeparator();
  menu->addAction(tr("Cancel"));

  // Create the menu at 1,1 to avoid the cursor being already in the first item.
  menu->popup(viewport()->mapToGlobal(where) + QPoint(1, 1));
}

/**
 * @brief Sets the normal state.
 */
void SpriteView::start_state_normal() {

  this->state = State::NORMAL;
}

/**
 * @brief Moves to the state of drawing a rectangle for a selection or a
 * new direction.
 * @param initial_point Where the user starts drawing the rectangle,
 * in view coordinates.
 */
void SpriteView::start_state_drawing_rectangle(const QPoint& initial_point) {

  this->state = State::DRAWING_RECTANGLE;
  this->dragging_start_point = mapToScene(initial_point).toPoint() / 8 * 8;

  current_area_item = new QGraphicsRectItem();
  current_area_item->setZValue(2);
  current_area_item->setPen(QPen(Qt::yellow));
  scene->addItem(current_area_item);
}

/**
 * @brief Finishes drawing a rectangle.
 */
void SpriteView::end_state_drawing_rectangle() {

  QRect rectangle = current_area_item->rect().toRect();
  if (!rectangle.isEmpty() &&
      sceneRect().contains(rectangle) &&
      !model->get_selected_index().is_direction_index()) {

    // Context menu to create a direction.
    QMenu menu;
    QAction* new_direction_action = new QAction(tr("New direction"), this);
    connect(new_direction_action, &QAction::triggered, [this, rectangle] {
      emit add_direction_requested(rectangle);
    });
    menu.addAction(new_direction_action);
    menu.addSeparator();
    menu.addAction(tr("Cancel"));
    menu.exec(cursor().pos() + QPoint(1, 1));
  }

  scene->removeItem(current_area_item);
  delete current_area_item;
  current_area_item = nullptr;

  start_state_normal();
}

/**
 * @brief Moves to the state of moving the selected direction.
 * @param initial_point Where the user starts dragging the direction,
 * in view coordinates.
 */
void SpriteView::start_state_moving_direction(const QPoint& initial_point) {

  SpriteModel::Index index = model->get_selected_index();
  if (!index.is_direction_index()) {
    return;
  }

  state = State::MOVING_DIRECTION;
  dragging_start_point = mapToScene(initial_point).toPoint()/ 8 * 8;
  const QRect& box = model->get_direction_all_frames_rect(index);
  current_area_item = new QGraphicsRectItem(box);
  current_area_item->setZValue(2);
  current_area_item->setPen(QPen(Qt::yellow));
  scene->addItem(current_area_item);
}

/**
 * @brief Finishes moving a direction.
 */
void SpriteView::end_state_moving_direction() {

  SpriteModel::Index index = model->get_selected_index();
  QRect box = current_area_item->rect().toRect();
  if (!box.isEmpty() &&
      sceneRect().contains(box) &&
      index.is_direction_index() &&
      box != model->get_direction_all_frames_rect(index)) {

    // Context menu to move the direction.
    QMenu menu;
    QAction* move_direction_action = new QAction(tr("Move here"), this);
    connect(move_direction_action, &QAction::triggered, [this, box] {
      emit change_selected_direction_position_requested(box.topLeft());
    });
    menu.addAction(move_direction_action);
    QAction* duplicate_direction_action =
      new QAction(QIcon(":/images/icon_copy.png"), tr("Duplicate here"), this);
    connect(duplicate_direction_action, &QAction::triggered, [this, box] {
      emit duplicate_selected_direction_requested(box.topLeft());
    });
    menu.addAction(duplicate_direction_action);
    menu.addSeparator();
    menu.addAction(tr("Cancel"));
    menu.exec(cursor().pos() + QPoint(1, 1));
  }

  scene->removeItem(current_area_item);
  delete current_area_item;
  current_area_item = nullptr;

  start_state_normal();
}

/**
 * @brief Changes the position of the direction the user is creating or moving.
 *
 * If the specified area is the same as before, nothing is done.
 *
 * @param new_area new position of the direction.
 */
void SpriteView::set_current_area(const QRect& area) {

  if (current_area_item->rect().toRect() == area) {
    // No change.
    return;
  }

  current_area_item->setRect(area);
}

}
