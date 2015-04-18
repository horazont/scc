#ifndef SCC_TERRAFORM_H
#define SCC_TERRAFORM_H

#include "fixups.hpp"

#include <QQmlEngine>

#include "engine/render/camera.hpp"
#include "engine/render/scenegraph.hpp"
#include "engine/render/fancyterrain.hpp"

#include "engine/sim/terrain.hpp"

#include "mode.hpp"
#include "quickglscene.hpp"

#include "terraform/brush.hpp"


struct TerraformScene
{
    engine::ResourceManager m_resources;
    engine::WindowRenderTarget m_window;
    engine::RenderGraph m_rendergraph;
    engine::SceneGraph m_scenegraph;
    engine::PerspectivalCamera m_camera;
    engine::Texture2D *m_grass;
    engine::Texture2D *m_rock;
    engine::Texture2D *m_blend;
    engine::FancyTerrainNode *m_terrain_node;
    engine::scenegraph::Transformation *m_pointer_trafo_node;
    engine::Material *m_overlay;
    engine::Texture2D *m_brush;
};


enum class TerraformTool {
    RAISE,
    LOWER,
    FLATTEN
};



class TerraformMode: public ApplicationMode
{
    Q_OBJECT
public:
    TerraformMode(QQmlEngine *engine);

private:
    std::unique_ptr<TerraformScene> m_scene;
    sim::Terrain m_terrain;
    engine::FancyTerrainInterface m_terrain_interface;

    QMetaObject::Connection m_advance_conn;
    QMetaObject::Connection m_before_gl_sync_conn;

    engine::ViewportSize m_viewport_size;
    Vector2f m_hover_pos;
    bool m_dragging;
    Vector3f m_drag_point;
    Vector3f m_drag_camera_pos;
    Vector3f m_hover_world;

    TerraformTool m_tool;

    GaussBrush m_test_brush;
    BrushFrontend m_brush_frontend;
    bool m_brush_changed;

protected:
    void geometryChanged(const QRectF &newGeometry,
                         const QRectF &oldGeometry) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    void prepare_scene();

protected:
    void apply_tool(const unsigned int x0,
                    const unsigned int y0);
    void tool_flatten(sim::Terrain::HeightField &field,
                      const sim::TerrainRect &r,
                      const unsigned int x0,
                      const unsigned int y0);
    void tool_raise(sim::Terrain::HeightField &field,
                    const float x0,
                    const float y0);
    void tool_lower(sim::Terrain::HeightField &field,
                    const float x0,
                    const float y0);

public slots:
    void advance(engine::TimeInterval dt);
    void before_gl_sync();

public:
    void activate(Application &app, QQuickItem &parent) override;
    void deactivate() override;

public:
    std::tuple<Vector3f, bool> hittest(const Vector2f viewport);

public:
    Q_INVOKABLE void switch_to_tool_raise();
    Q_INVOKABLE void switch_to_tool_lower();
    Q_INVOKABLE void switch_to_tool_flatten();

    Q_INVOKABLE void set_brush_size(float size);
    Q_INVOKABLE void set_brush_strength(float strength);

};

#endif
