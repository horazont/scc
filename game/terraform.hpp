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
    engine::WindowRenderTarget m_window;
    engine::RenderGraph m_rendergraph;
    engine::SceneGraph m_scenegraph;
    engine::PerspectivalCamera m_camera;
    engine::Texture2D *m_grass;
    engine::Texture2D *m_rock;
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

class Brush
{
public:
    typedef float density_t;

public:
    Brush();
    virtual ~Brush();

public:
    virtual density_t sample(const float x, const float y) const = 0;

};

class BitmapBrush: public Brush
{
public:
    BitmapBrush(const unsigned int base_size);

private:
    unsigned int m_base_size;
    std::vector<density_t> m_pixels;

public:
    inline unsigned int base_size() const
    {
        return m_base_size;
    }

    inline void set(unsigned int x, unsigned int y, density_t value)
    {
        assert(x < m_base_size && y < m_base_size);
        m_pixels[y*m_base_size+x] = value;
    }

    inline density_t get(unsigned int x, unsigned int y) const
    {
        assert(x < m_base_size && y < m_base_size);
        return m_pixels[y*m_base_size+x];
    }

    inline density_t *scanline(unsigned int y)
    {
        return &m_pixels[y*m_base_size];
    }

public:
    density_t sample(float x, float y) const override;

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

    BitmapBrush m_test_brush;
    BitmapBrush *m_curr_brush;
    bool m_brush_changed;
    float m_brush_size;

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
                    const sim::TerrainRect &r);

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

};

#endif
