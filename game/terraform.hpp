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


struct TerraformScene
{
    engine::ResourceManager m_resources;
    engine::SceneGraph m_scenegraph;
    engine::PerspectivalCamera m_camera;
    engine::Texture2D *m_grass;
    engine::FancyTerrainNode *m_terrain_node;
    engine::scenegraph::Transformation *m_pointer_trafo_node;
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

    QMetaObject::Connection m_before_gl_sync_conn;

    Vector2f m_hover_pos;
    bool m_dragging;
    Vector3f m_drag_point;
    Vector3f m_drag_camera_pos;

    TerraformTool m_tool;

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
                    const sim::TerrainRect &r);
    void tool_lower(sim::Terrain::HeightField &field,
                    const sim::TerrainRect &r);

public slots:
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

};

#endif
