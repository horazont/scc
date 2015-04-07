#ifndef SCC_TERRAFORM_H
#define SCC_TERRAFORM_H

#include <GL/glew.h>

#include <QQmlEngine>

#include "engine/render/camera.hpp"
#include "engine/render/scenegraph.hpp"
#include "engine/render/quadterrain.hpp"

#include "engine/sim/terrain.hpp"

#include "mode.hpp"
#include "quickglscene.hpp"


struct TerraformScene
{
    engine::ResourceManager m_resources;
    engine::SceneGraph m_scenegraph;
    engine::PerspectivalCamera m_camera;
    engine::Texture2D *m_grass;
    engine::QuadTerrainNode *m_terrain_node;
    engine::scenegraph::Transformation *m_pointer_trafo_node;
};


class TerraformMode: public ApplicationMode
{
    Q_OBJECT
public:
    TerraformMode(QQmlEngine *engine);

private:
    std::unique_ptr<TerraformScene> m_scene;
    sim::QuadTerrain m_terrain;

    QMetaObject::Connection m_before_gl_sync_conn;

    Vector2f m_hover_pos;
    bool m_dragging;
    Vector3f m_drag_point;
    Vector3f m_drag_camera_pos;

protected:
    void geometryChanged(const QRectF &newGeometry,
                         const QRectF &oldGeometry) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void prepare_scene();

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
