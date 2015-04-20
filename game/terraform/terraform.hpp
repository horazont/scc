#ifndef SCC_TERRAFORM_H
#define SCC_TERRAFORM_H

#include "fixups.hpp"

#include <QAbstractItemModel>
#include <QDir>
#include <QQmlEngine>
#include <QQuickImageProvider>

#include "engine/render/camera.hpp"
#include "engine/render/scenegraph.hpp"
#include "engine/render/fancyterrain.hpp"

#include "engine/sim/terrain.hpp"

#include "mode.hpp"
#include "quickglscene.hpp"

#include "terraform/brush.hpp"
#include "terraform/terratool.hpp"


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


enum MouseMode
{
    MOUSE_IDLE,
    MOUSE_PAINT,
    MOUSE_DRAG,
    MOUSE_ROTATE
};


class BrushListImageProvider: public QQuickImageProvider
{
public:
    typedef unsigned int ImageID;
    static const QString provider_name;

public:
    BrushListImageProvider();
    ~BrushListImageProvider() override;

private:
    std::shared_timed_mutex m_images_mutex;
    ImageID m_image_id_ctr;
    std::unordered_map<ImageID, std::unique_ptr<QPixmap> > m_images;

public:
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

public:
    ImageID publish_pixmap(std::unique_ptr<QPixmap> &&pixmap);
    void unpublish_pixmap(ImageID id);

    static QString image_id_to_url(ImageID id);

};


class BrushWrapper
{
public:
    static constexpr unsigned int preview_size = 32;

public:
    BrushWrapper(std::unique_ptr<Brush> &&brush, const QString &display_name = "");
    ~BrushWrapper();

public:
    std::unique_ptr<Brush> m_brush;
    QString m_display_name;
    BrushListImageProvider::ImageID m_image_id;
    QString m_image_url;

};


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
    static BrushListImageProvider *m_image_provider;
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
    void append(std::unique_ptr<Brush> &&brush, const QString &display_name = "");
    void append(const gamedata::PixelBrushDef &brush);

    inline const std::vector<std::unique_ptr<BrushWrapper> > &vector() const
    {
        return m_brushes;
    }

public:
    static BrushListImageProvider *image_provider();

};


class TerraformMode: public ApplicationMode
{
    Q_OBJECT

    Q_PROPERTY(BrushList* brush_list_model READ brush_list_model NOTIFY brush_list_model_changed())

public:
    TerraformMode(QQmlEngine *engine);

private:
    std::unique_ptr<TerraformScene> m_scene;
    sim::Terrain m_terrain;
    engine::FancyTerrainInterface m_terrain_interface;

    QMetaObject::Connection m_advance_conn;
    QMetaObject::Connection m_before_gl_sync_conn;

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
    TerraTool *m_curr_tool;

    BrushList m_brush_objects;

protected:
    void geometryChanged(const QRectF &newGeometry,
                         const QRectF &oldGeometry) override;
    void hoverMoveEvent(QHoverEvent *event) override;
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
    void before_gl_sync();

public:
    void activate(Application &app, QQuickItem &parent) override;
    void deactivate() override;

public:
    BrushList *brush_list_model();
    std::tuple<Vector3f, bool> hittest(const Vector2f viewport);

public:
    Q_INVOKABLE void switch_to_tool_flatten();
    Q_INVOKABLE void switch_to_tool_raise_lower();

    Q_INVOKABLE void set_brush(int index);
    Q_INVOKABLE void set_brush_size(float size);
    Q_INVOKABLE void set_brush_strength(float strength);

signals:
    void brush_list_model_changed();

};

#endif
