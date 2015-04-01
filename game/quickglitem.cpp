#include "quickglitem.hpp"

#include <cassert>
#include <iostream>

#include <QOpenGLContext>
#include <QSGOpacityNode>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_3_2_Core>

#include "engine/math/matrix.hpp"

#include "engine/io/log.hpp"
#include "engine/io/mount.hpp"
#include "engine/render/scenegraph.hpp"

io::Logger &qml_gl_logger = io::logging().get_logger("qmlgl");


void load_image_to_texture(io::FileSystem &fs, const std::string &path)
{
    std::basic_string<uint8_t> texture_data;
    {
        std::unique_ptr<io::Stream> texfile(fs.open(path, io::OpenMode::READ));
        texture_data = texfile->read_all();
    }

    QImage texture = QImage::fromData((const unsigned char*)texture_data.data(), texture_data.size());
    texture.convertToFormat(QImage::Format_ARGB32);

    uint8_t *pixbase = texture.bits();
    for (int i = 0; i < texture.width()*texture.height(); i++)
    {
        const uint8_t A = pixbase[3];
        const uint8_t R = pixbase[2];
        const uint8_t G = pixbase[1];
        const uint8_t B = pixbase[0];

        pixbase[0] = R;
        pixbase[1] = G;
        pixbase[2] = B;
        pixbase[3] = A;

        pixbase += 4;
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, 0,
                    texture.width(), texture.height(),
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    texture.bits());
}


class GridNode: public engine::scenegraph::Node
{
public:
    GridNode(const unsigned int xcells,
             const unsigned int ycells,
             const float size):
        m_vbo(engine::VBOFormat({
                                    engine::VBOAttribute(3)
                                })),
        m_ibo(),
        m_material(),
        m_vbo_alloc(m_vbo.allocate((xcells+1+ycells+1)*2)),
        m_ibo_alloc(m_ibo.allocate((xcells+1+ycells+1)*2))
    {
        auto slice = engine::VBOSlice<Vector3f>(m_vbo_alloc, 0);
        const float x0 = -size*xcells/2.;
        const float y0 = -size*ycells/2.;

        uint16_t *dest = m_ibo_alloc.get();
        unsigned int base = 0;
        for (unsigned int x = 0; x < xcells+1; x++)
        {
            slice[base+2*x] = Vector3f(x0+x*size, y0, 0);
            slice[base+2*x+1] = Vector3f(x0+x*size, -y0, 0);
            *dest++ = base+2*x;
            *dest++ = base+2*x+1;
        }

        base = (xcells+1)*2;
        for (unsigned int y = 0; y < ycells+1; y++)
        {
            slice[base+2*y] = Vector3f(x0, y0+y*size, 0);
            slice[base+2*y+1] = Vector3f(-x0, y0+y*size, 0);
            *dest++ = base+2*y;
            *dest++ = base+2*y+1;
        }

        m_vbo_alloc.mark_dirty();
        m_ibo_alloc.mark_dirty();

        if (!m_material.shader().attach(
                    GL_VERTEX_SHADER,
                    "#version 330\n"
                    "layout(std140) uniform MatrixBlock {"
                    "  layout(row_major) mat4 proj;"
                    "  layout(row_major) mat4 view;"
                    "  layout(row_major) mat4 model;"
                    "  layout(row_major) mat3 normal;"
                    "};"
                    "in vec3 position;"
                    "void main() {"
                    "  gl_Position = proj*view*model*vec4(position, 1.0f);"
                    "}") ||
                !m_material.shader().attach(
                    GL_FRAGMENT_SHADER,
                    "#version 330\n"
                    "out vec4 color;"
                    "void main() {"
                    "  color = vec4(0.5, 0.5, 0.5, 1.0);"
                    "}") ||
                !m_material.shader().link())
        {
            throw std::runtime_error("failed to build shader");
        }

        engine::ArrayDeclaration decl;
        decl.declare_attribute("position", m_vbo, 0);
        decl.set_ibo(&m_ibo);

        m_vao = decl.make_vao(m_material.shader(), true);

        m_material.shader().bind();
        m_material.shader().check_uniform_block<engine::RenderContext::MatrixUBO>(
                    "MatrixBlock");
        m_material.shader().bind_uniform_block(
                    "MatrixBlock",
                    engine::RenderContext::MATRIX_BLOCK_UBO_SLOT);
    }

private:
    engine::VBO m_vbo;
    engine::IBO m_ibo;
    engine::Material m_material;
    std::unique_ptr<engine::VAO> m_vao;

    engine::VBOAllocation m_vbo_alloc;
    engine::IBOAllocation m_ibo_alloc;

public:
    void render(engine::RenderContext &context) override
    {
        context.draw_elements(GL_LINES, *m_vao, m_material, m_ibo_alloc);
    }

    void sync() override
    {
        m_vao->sync();
    }

};


QuickGLScene::QuickGLScene(QObject *parent):
    QObject(parent),
    m_initialized(false),
    m_resources(),
    m_camera(),
    m_terrain(65, 65),
    m_scenegraph(),
    m_t(hrclock::now()),
    m_t0(monoclock::now()),
    m_nframes(0),
    test_texture(m_resources.emplace<engine::Texture2D>("test_texture", GL_RGBA, 512, 512))
{
    /*engine::scenegraph::Transformation &transform =
            m_scenegraph.root().emplace<engine::scenegraph::Transformation>();
    transform.emplace_child<GridNode>(10, 10, 10);
    transform.transformation() = translation4(Vector3(100, 100, 0)) * rotation4(eZ, 1.4);*/

    m_scenegraph.root().emplace<GridNode>(64, 64, 1);
    engine::Terrain &terrain_node = m_scenegraph.root().emplace<engine::Terrain>(m_terrain);
    terrain_node.set_grass_texture(&test_texture);

    m_camera.controller().set_distance(40.0);
    m_camera.controller().set_rot(Vector2f(0, 0));
    m_camera.controller().set_pos(Vector3f(0, 0, 20.));
}

QuickGLScene::~QuickGLScene()
{

}

void QuickGLScene::paint()
{
    if (!m_initialized) {
        m_initialized = true;
    }

    glClearColor(0.4, 0.3, 0.2, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    test_texture.bind();
    m_scenegraph.render(m_camera);

    hrclock::time_point t1 = hrclock::now();
    const unsigned int msecs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - m_t).count();
    if (msecs >= 1000)
    {
        qml_gl_logger.logf(io::LOG_DEBUG, "fps: %.2lf", (double)m_nframes / (double)msecs * 1000.0d);
        m_nframes = 0;
        m_t = t1;
    }
    m_nframes += 1;
}

void QuickGLScene::advance(engine::TimeInterval seconds)
{
    m_camera.controller().advance(seconds);
    m_scenegraph.advance(seconds);
}

void QuickGLScene::boost_camera(const Vector2f &by)
{
    m_camera.controller().boost_movement(Vector3f(by[eX], by[eY], 0.));
}

void QuickGLScene::boost_camera_rot(const Vector2f &by)
{
    m_camera.controller().boost_rotation(by);
}

void QuickGLScene::set_viewport_size(const QSize &size)
{
    m_camera.set_viewport(size.width(), size.height());
    m_camera.set_znear(1.0);
    m_camera.set_zfar(100);
}

void QuickGLScene::sync()
{
    m_camera.sync();
    m_scenegraph.sync();
}

void QuickGLScene::zoom_camera(const float by)
{
    float distance = m_camera.controller().distance();
    distance -= by;
    if (distance < 5.0) {
        distance = 5.0;
    } else if (distance > 50.0) {
        distance = 50.0;
    }

    m_camera.controller().set_distance(distance, true);
}


QuickGLItem::QuickGLItem(QQuickItem *parent):
    QQuickItem(parent),
    m_renderer(nullptr),
    m_t(monoclock::now())
{
    m_vfs.mount("/resources",
                std::unique_ptr<io::MountDirectory>(
                    new io::MountDirectory("resources/", true)),
                io::MountPriority::FileSystem);

    setFlags(QQuickItem::ItemHasContents);
    setAcceptHoverEvents(false);
    setAcceptedMouseButtons(Qt::AllButtons);
    connect(this, SIGNAL(windowChanged(QQuickWindow*)),
            this, SLOT(handle_window_changed(QQuickWindow*)));
}

void QuickGLItem::hoverMoveEvent(QHoverEvent *event)
{
    qml_gl_logger.log(io::LOG_DEBUG, "hover");
    m_hover_pos = Vector2f(event->pos().x(), event->pos().y());
}

void QuickGLItem::mouseMoveEvent(QMouseEvent *event)
{
    qml_gl_logger.log(io::LOG_DEBUG, "move");

    const Vector2f new_pos = Vector2f(event->pos().x(),
                                      event->pos().y());
    const Vector2f dist = m_hover_pos - new_pos;

    m_hover_pos = new_pos;

    if (event->buttons() & Qt::LeftButton) {
        m_renderer->boost_camera(Vector2f(dist[eX], -dist[eY])*10.0);
    } else if (event->buttons() & Qt::RightButton) {
        m_renderer->boost_camera_rot(Vector2f(dist[eY], dist[eX]));
    }
}

void QuickGLItem::mousePressEvent(QMouseEvent *event)
{
    qml_gl_logger.log(io::LOG_DEBUG, "press");
    m_hover_pos = Vector2f(event->pos().x(), event->pos().y());
}

void QuickGLItem::wheelEvent(QWheelEvent *event)
{
    qml_gl_logger.log(io::LOG_DEBUG, "wheel");
    m_renderer->zoom_camera(event->angleDelta().y() / 10.);
}

QSGNode *QuickGLItem::updatePaintNode(
        QSGNode *oldNode,
        UpdatePaintNodeData *)
{
    update();
    if (!oldNode) {
        oldNode = new QSGOpacityNode();
    }
    oldNode->markDirty(QSGNode::DirtyForceUpdate);
    return oldNode;
}

void QuickGLItem::handle_window_changed(QQuickWindow *win)
{
    if (win)
    {
        qml_gl_logger.log(io::LOG_INFO, "initializing window...");

        connect(win, SIGNAL(beforeSynchronizing()),
                this, SLOT(sync()),
                Qt::DirectConnection);
        connect(win, SIGNAL(sceneGraphInvalidated()),
                this, SLOT(cleanup()),
                Qt::DirectConnection);
        connect(win, SIGNAL(beforeRendering()),
                this, SLOT(new_frame()),
                Qt::QueuedConnection);

        win->setSurfaceType(QSurface::OpenGLSurface);

        QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setVersion(3, 3);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setSamples(0);
        format.setRedBufferSize(8);
        format.setGreenBufferSize(8);
        format.setBlueBufferSize(8);
        format.setAlphaBufferSize(8);
        format.setStencilBufferSize(8);
        format.setDepthBufferSize(24);

        win->setFormat(format);
        win->create();

        QOpenGLContext *context = new QOpenGLContext();
        context->setFormat(format);
        if (!context->create()) {
            throw std::runtime_error("failed to create context");
        }

        qml_gl_logger.log(io::LOG_INFO)
                << "created context, version "
                << context->format().majorVersion()
                << "."
                << context->format().minorVersion()
                << io::submit;

        bool context_ok = true;
        io::LogLevel context_info_level = io::LOG_DEBUG;
        if (context->format().profile() != QSurfaceFormat::CoreProfile ||
                context->format().majorVersion() != 3 ||
                context->format().depthBufferSize() == 0)
        {
            context_ok = false;
            context_info_level = io::LOG_WARNING;
            qml_gl_logger.log(io::LOG_EXCEPTION)
                    << "Could not create Core-profile OpenGL 3+ context with depth buffer"
                    << io::submit;
        } else {
            qml_gl_logger.log(io::LOG_DEBUG,
                              "context deemed appropriate, continuing...");
        }

        qml_gl_logger.log(context_info_level)
                << "  renderable  : "
                << (context->format().renderableType() == QSurfaceFormat::OpenGL
                    ? "OpenGL"
                    : context->format().renderableType() == QSurfaceFormat::OpenGLES
                    ? "OpenGL ES"
                    : context->format().renderableType() == QSurfaceFormat::OpenVG
                    ? "OpenVG (software?)"
                    : "unknown")
                << io::submit;
        qml_gl_logger.log(context_info_level)
                << "  rgba        : "
                << context->format().redBufferSize() << " "
                << context->format().greenBufferSize() << " "
                << context->format().blueBufferSize() << " "
                << context->format().alphaBufferSize() << " "
                << io::submit;
        qml_gl_logger.log(context_info_level)
                << "  stencil     : "
                << context->format().stencilBufferSize()
                << io::submit;
        qml_gl_logger.log(context_info_level)
                << "  depth       : " << context->format().depthBufferSize()
                << io::submit;
        qml_gl_logger.log(context_info_level)
                << "  multisamples: " << context->format().samples()
                << io::submit;
        qml_gl_logger.log(context_info_level)
                << "  profile     : "
                << (context->format().profile() == QSurfaceFormat::CoreProfile
                    ? "core"
                    : "compatibility")
                << io::submit;

        if (!context_ok) {
            throw std::runtime_error("Failed to create appropriate OpenGL context");
        }

        context->makeCurrent(win);

        qml_gl_logger.log(io::LOG_INFO)
                << "initializing GLEW in experimental mode"
                << io::submit;
        glewExperimental = GL_TRUE;
        GLenum err = glewInit();
        if (err != GLEW_OK) {
            const std::string error = std::string((const char*)glewGetErrorString(err));
            qml_gl_logger.log(io::LOG_EXCEPTION)
                    << "GLEW failed to initialize"
                    << error
                    << io::submit;
            throw std::runtime_error("failed to initialize GLEW: " + error);
        }

        qml_gl_logger.log(io::LOG_DEBUG) << "turning off clear" << io::submit;
        win->setClearBeforeRendering(false);

        qml_gl_logger.log(io::LOG_INFO) << "Window and rendering context initialized :)"
                                        << io::submit;
    }
}

void QuickGLItem::sync()
{
    if (!m_renderer)
    {
        m_renderer = std::unique_ptr<QuickGLScene>(new QuickGLScene());
        connect(window(), SIGNAL(beforeRendering()),
                m_renderer.get(), SLOT(paint()),
                Qt::DirectConnection);
        m_renderer->test_texture.bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        load_image_to_texture(m_vfs, "/resources/textures/grass00.png");
        GLEW_GET_FUN(__glewGenerateMipmap)(GL_TEXTURE_2D);
    }
    m_renderer->set_viewport_size(window()->size() * window()->devicePixelRatio());
    m_renderer->sync();
}

void QuickGLItem::new_frame()
{
    const monoclock::time_point now = monoclock::now();
    const engine::TimeInterval seconds = std::chrono::duration_cast<
            std::chrono::duration<float, std::ratio<1> >
            >(now - m_t).count();

    qml_gl_logger.logf(io::LOG_DEBUG, "frame time: %.4f s", seconds);

    if (m_renderer) {
        m_renderer->advance(seconds);
    } else {
        qml_gl_logger.log(io::LOG_WARNING, "no renderer");
    }

    m_t = now;
}

void QuickGLItem::cleanup()
{
    if (m_renderer)
    {
        m_renderer = nullptr;
    }
}
