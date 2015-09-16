/**********************************************************************
File name: terraform.hpp
This file is part of: SCC (working title)

LICENSE

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.

FEEDBACK & QUESTIONS

For feedback and questions about SCC please e-mail one of the authors named in
the AUTHORS file.
**********************************************************************/
#ifndef SCC_TERRAFORM_H
#define SCC_TERRAFORM_H

#include "fixups.hpp"

#include <QAbstractListModel>
#include <QActionGroup>
#include <QDir>

#include "ffengine/math/octree.hpp"

#include "ffengine/gl/resource.hpp"

#include "ffengine/render/camera.hpp"
#include "ffengine/render/scenegraph.hpp"
#include "ffengine/render/fancyterrain.hpp"
#include "ffengine/render/aabb.hpp"
#include "ffengine/render/fluidsource.hpp"

#include "ffengine/sim/terrain.hpp"
#include "ffengine/sim/world.hpp"
#include "ffengine/sim/server.hpp"

#include "mode.hpp"
#include "mousebinding.hpp"

#include "terraform/brush.hpp"
#include "terraform/terratool.hpp"


struct TerraformScene
{
    TerraformScene(ffe::FancyTerrainInterface &terrain_interface,
                   const sim::WorldState &state,
                   QSize window_size,
                   ffe::DynamicAABBs::DiscoverCallback &&aabb_callback);
    ~TerraformScene();

    ffe::GLResourceManager m_resources;
    ffe::WindowRenderTarget m_window;
    ffe::SceneGraph m_scenegraph;
    ffe::PerspectivalCamera m_camera;
    ffe::Scene m_scene;
    ffe::RenderGraph m_rendergraph;

    ffe::FBO &m_prewater_buffer;

    ffe::Texture2D &m_prewater_colour;
    ffe::Texture2D &m_prewater_depth;

    ffe::RenderPass &m_solid_pass;
    ffe::RenderPass &m_transparent_pass;
    ffe::RenderPass &m_water_pass;

    ffe::Texture2D &m_grass;
    ffe::Texture2D &m_rock;
    ffe::Texture2D &m_blend;
    ffe::Texture2D &m_waves;

    ffe::FullTerrainNode &m_full_terrain;
    ffe::FancyTerrainNode &m_terrain_geometry;

    ffe::Material &m_overlay_material;
    ffe::Material &m_bezier_material;
    ffe::Material &m_road_material;
    ffe::Material &m_aabb_material;
    ffe::FluidSourceMaterial &m_fluid_source_material;

    ffe::Texture2D &m_brush;

    ffe::scenegraph::OctreeGroup &m_octree_group;

public:
    ffe::Texture2D &load_texture_resource(
            const std::string &resource_name,
            const QString &source_path);
    void update_size(const QSize &new_size);

};


enum MouseMode
{
    MOUSE_IDLE,
    MOUSE_TOOL_DRAG,
    MOUSE_ROTATE
};


/**
 * Wrap a brush for usage for Qt / QML.
 */
class BrushWrapper
{
public:
    static constexpr unsigned int preview_size = 32;

public:
    /**
     * Wrap and hold a brush for QML.
     *
     * @param brush Brush to hold
     * @param display_name Display name to show in the user interface
     */
    BrushWrapper(std::unique_ptr<Brush> &&brush,
                 const QString &display_name = "");
    ~BrushWrapper();

public:
    std::unique_ptr<Brush> m_brush;
    QString m_display_name;
    QImage m_preview_icon;

};


/**
 * A Qt list model to show brushes in QML.
 */
class BrushList: public QAbstractListModel
{
    Q_OBJECT
public:
    BrushList(QObject *parent = nullptr);

private:
    std::vector<std::unique_ptr<BrushWrapper> > m_brushes;

protected:
    BrushWrapper *resolve_index(const QModelIndex &index);
    const BrushWrapper *resolve_index(const QModelIndex &index) const;

public:
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

public:
    /**
     * Create a new BrushWrapper and add it to the list.
     *
     * @see BrushWrapper::BrushWrapper
     *
     * @param brush
     * @param display_name
     */
    void append(std::unique_ptr<Brush> &&brush,
                const QString &display_name = "");

    /**
     * Create and add a new brush
     *
     * @param brush Protocol Buffer using a brush.
     */
    void append(const gamedata::PixelBrushDef &brush);

    inline const std::vector<std::unique_ptr<BrushWrapper> > &vector() const
    {
        return m_brushes;
    }

};


namespace Ui {
class TerraformMode;
}


class CameraDrag: public PlaneToolDrag
{
public:
    CameraDrag(ffe::PerspectivalCamera &camera,
               const Vector2f &viewport_size,
               const Vector3f &start_world_pos);

private:
    ffe::PerspectivalCamera &m_camera;
    Vector3f m_drag_point;
    Vector3f m_drag_camera_pos;

private:
    sim::WorldOperationPtr plane_drag(const Vector2f &viewport_pos,
                                      const Vector3f &world_pos);

};


class TerraformMode: public ApplicationMode
{
    Q_OBJECT

    Q_PROPERTY(BrushList* brush_list_model READ brush_list_model NOTIFY brush_list_model_changed())

public:
    TerraformMode(Application &app, QWidget *parent = nullptr);
    ~TerraformMode() override;

private:
    Ui::TerraformMode *m_ui;
    QActionGroup m_tools;
    MouseActionDispatcher m_mouse_dispatcher;

    std::unique_ptr<TerraformScene> m_scene;
    std::unique_ptr<sim::Server> m_server;
    sim::Server::SyncSafeLock m_sync_lock;
    std::unique_ptr<ffe::FancyTerrainInterface> m_terrain_interface;
    std::unique_ptr<ToolBackend> m_tool_backend;

    QMetaObject::Connection m_advance_conn;
    QMetaObject::Connection m_after_gl_sync_conn;
    QMetaObject::Connection m_before_gl_sync_conn;

    std::mutex m_sim_callback_queue_mutex;
    std::vector<std::function<void()> > m_sim_callback_queue;

    float m_t;

    ffe::ViewportSize m_viewport_size;
    Vector2f m_mouse_pos_win;

    MouseMode m_mouse_mode;
    MouseAction *m_mouse_action;

    ToolDragPtr m_drag;

    bool m_mouse_world_pos_updated;
    bool m_mouse_world_pos_valid;
    Vector3f m_mouse_world_pos;

    ParzenBrush m_test_brush;
    BrushFrontend m_brush_frontend;
    bool m_brush_changed;

    std::unique_ptr<TerraRaiseLowerTool> m_tool_raise_lower;
    std::unique_ptr<TerraLevelTool> m_tool_level;
    std::unique_ptr<TerraSmoothTool> m_tool_smooth;
    std::unique_ptr<TerraRampTool> m_tool_ramp;
    std::unique_ptr<TerraFluidRaiseTool> m_tool_fluid_raise;
    std::unique_ptr<TerraTestingTool> m_tool_testing;
    std::unique_ptr<TerraFluidSourceTool> m_tool_fluid_source;

    AbstractTerraTool *m_curr_tool;

    BrushList m_brush_objects;

    bool m_paused;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void clear_mouse_mode();
    void enter_mouse_mode(MouseMode mode,
                          MouseAction *original_action);
    void initialise_tools();
    bool may_clear_mouse_mode(MouseMode potential_mode);

protected:
    void switch_to_tool(AbstractTerraTool *new_tool);
    void collect_aabbs(std::vector<AABB> &dest);
    void ensure_mouse_world_pos();
    void load_brushes();
    void load_brushes_from(QDir dir, bool recurse=true);
    void prepare_scene();
    void update_brush();

public slots:
    void advance(ffe::TimeInterval dt);
    void after_gl_sync();
    void before_gl_sync();

public:
    void activate(QWidget &parent) override;
    void deactivate() override;

public:
    BrushList *brush_list_model();
    std::tuple<Vector3f, bool> hittest(const Vector2f viewport);

signals:
    void brush_list_model_changed();

private slots:
    void on_action_terraform_tool_terrain_raise_lower_triggered();
    void on_action_terraform_tool_terrain_level_triggered();
    void on_action_terraform_tool_terrain_smooth_triggered();
    void on_action_terraform_tool_terrain_ramp_triggered();
    void on_action_terraform_tool_fluid_raise_lower_triggered();
    void on_slider_brush_size_valueChanged(int value);
    void on_slider_brush_strength_valueChanged(int value);
    void on_brush_list_clicked(const QModelIndex &index);
    void on_action_terraform_tool_test_triggered();
    void on_action_terraform_tool_fluid_edit_sources_triggered();

    void on_level_tool_reference_height_slider_valueChanged(int value);

private:
    void on_camera_pan_triggered();
    void on_camera_rotate_triggered();
    void on_tool_primary_triggered();
    void on_tool_secondary_triggered();

    void on_tool_level_reference_height_changed(float new_value);

};

#endif
