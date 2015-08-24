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

#include <QAbstractItemModel>
#include <QDir>

#include "ffengine/render/camera.hpp"
#include "ffengine/render/scenegraph.hpp"
#include "ffengine/render/fancyterrain.hpp"

#include "ffengine/sim/terrain.hpp"

#include "mode.hpp"

#include "terraform/brush.hpp"
#include "terraform/terratool.hpp"


struct TerraformScene
{
    engine::ResourceManager m_resources;
    engine::WindowRenderTarget m_window;
    engine::RenderGraph m_rendergraph;
    engine::SceneGraph m_scenegraph;
    engine::SceneGraph m_water_scenegraph;
    engine::PerspectivalCamera m_camera;
    engine::Texture2D *m_grass;
    engine::Texture2D *m_rock;
    engine::Texture2D *m_blend;
    engine::Texture2D *m_waves;
    engine::FancyTerrainNode *m_terrain_node;
    engine::scenegraph::Transformation *m_pointer_trafo_node;
    engine::scenegraph::Transformation *m_fluidplane_trafo_node;
    engine::Material *m_fluid;
    engine::Material *m_overlay;
    engine::Texture2D *m_brush;

    engine::Texture2D *m_prewater_colour_buffer;
    engine::Texture2D *m_prewater_depth_buffer;
    engine::FBO *m_prewater_pass;
};


enum MouseMode
{
    MOUSE_IDLE,
    MOUSE_PAINT,
    MOUSE_DRAG,
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
    QString m_image_url;

};


/**
 * A Qt list model to show brushes in QML.
 */
class BrushList: public QAbstractItemModel
{
    Q_OBJECT
public:
    enum BrushListRole {
        ROLE_DISPLAY_NAME = Qt::UserRole+1,
        ROLE_IMAGE_URL
    };

public:
    BrushList(QObject *parent = nullptr);

private:
    std::vector<std::unique_ptr<BrushWrapper> > m_brushes;

protected:
    bool valid_brush_index(const QModelIndex &index) const;
    BrushWrapper *resolve_index(const QModelIndex &index);
    const BrushWrapper *resolve_index(const QModelIndex &index) const;

public:
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool hasChildren(const QModelIndex &parent) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QModelIndex sibling(int row, int column, const QModelIndex &idx) const override;

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


class TerraformMode: public ApplicationMode
{
    Q_OBJECT

    Q_PROPERTY(BrushList* brush_list_model READ brush_list_model NOTIFY brush_list_model_changed())

public:
    TerraformMode(QWidget *parent = nullptr);

private:
    std::unique_ptr<TerraformScene> m_scene;
    sim::Server m_server;
    sim::Server::SyncSafeLock m_sync_lock;
    engine::FancyTerrainInterface m_terrain_interface;

    QMetaObject::Connection m_advance_conn;
    QMetaObject::Connection m_after_gl_sync_conn;
    QMetaObject::Connection m_before_gl_sync_conn;

    float m_t;

    engine::ViewportSize m_viewport_size;
    Vector2f m_mouse_pos_win;

    MouseMode m_mouse_mode;

    Vector3f m_drag_point;
    Vector3f m_drag_camera_pos;

    bool m_paint_secondary;

    bool m_mouse_world_pos_updated;
    bool m_mouse_world_pos_valid;
    Vector3f m_mouse_world_pos;

    ParzenBrush m_test_brush;
    BrushFrontend m_brush_frontend;
    bool m_brush_changed;

    ToolBackend m_tool_backend;
    TerraRaiseLowerTool m_tool_raise_lower;
    TerraLevelTool m_tool_level;
    TerraSmoothTool m_tool_smooth;
    TerraRampTool m_tool_ramp;
    TerraFluidRaiseTool m_tool_fluid_raise;
    TerraTool *m_curr_tool;

    BrushList m_brush_objects;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

protected:
    void apply_tool(const float x0, const float y0, bool secondary);
    void ensure_mouse_world_pos();
    void load_brushes();
    void load_brushes_from(QDir dir, bool recurse=true);
    void prepare_scene();

public slots:
    void advance(engine::TimeInterval dt);
    void after_gl_sync();
    void before_gl_sync();

public:
    void activate(Application &app, QWidget &parent) override;
    void deactivate() override;

public:
    BrushList *brush_list_model();
    std::tuple<Vector3f, bool> hittest(const Vector2f viewport);

public:
    Q_INVOKABLE void switch_to_tool_flatten();
    Q_INVOKABLE void switch_to_tool_raise_lower();
    Q_INVOKABLE void switch_to_tool_smooth();
    Q_INVOKABLE void switch_to_tool_ramp();

    Q_INVOKABLE void switch_to_tool_fluid_raise();

    Q_INVOKABLE void set_brush(int index);
    Q_INVOKABLE void set_brush_size(float size);
    Q_INVOKABLE void set_brush_strength(float strength);

signals:
    void brush_list_model_changed();

};

#endif
