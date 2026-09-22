#include "VideoWidget.h"

#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QVector2D>

#include <algorithm>

VideoWidget::VideoWidget(QWidget* parent)
    : QOpenGLWidget(parent) {
    setMinimumSize(640, 360);
}

VideoWidget::~VideoWidget() {
    if (!context() || !context()->isValid()) return;

    makeCurrent();

    if (texture_ != 0) glDeleteTextures(1, &texture_);
    if (vao_ != 0) glDeleteVertexArrays(1, &vao_);

    doneCurrent();
}

void VideoWidget::setFrame(const QImage& image) {
    frame_ = image.format() == QImage::Format_RGBA8888
        ? image
        : image.convertToFormat(QImage::Format_RGBA8888);

    textureDirty_ = true;
    update();
}

void VideoWidget::clearFrame() {
    frame_ = {};
    textureDirty_ = false;
    update();
}

void VideoWidget::initializeGL() {
    initializeOpenGLFunctions();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Core-profile OpenGL requires a VAO even when vertices come from gl_VertexID.
    glGenVertexArrays(1, &vao_);

    program_ = std::make_unique<QOpenGLShaderProgram>();

    static constexpr char VertexShader[] = R"(
        #version 330 core

        uniform vec2 uScale;
        out vec2 vTexCoord;

        const vec2 positions[4] = vec2[](
            vec2(-1.0,  1.0),
            vec2(-1.0, -1.0),
            vec2( 1.0,  1.0),
            vec2( 1.0, -1.0)
        );

        const vec2 texCoords[4] = vec2[](
            vec2(0.0, 0.0),
            vec2(0.0, 1.0),
            vec2(1.0, 0.0),
            vec2(1.0, 1.0)
        );

        void main() {
            vec2 position = positions[gl_VertexID] * uScale;
            gl_Position = vec4(position, 0.0, 1.0);
            vTexCoord = texCoords[gl_VertexID];
        }
    )";

    static constexpr char FragmentShader[] = R"(
        #version 330 core

        in vec2 vTexCoord;
        out vec4 fragColor;
        uniform sampler2D uTexture;

        void main() {
            fragColor = texture(uTexture, vTexCoord);
        }
    )";

    program_->addShaderFromSourceCode(QOpenGLShader::Vertex, VertexShader);
    program_->addShaderFromSourceCode(QOpenGLShader::Fragment, FragmentShader);
    program_->link();
}

void VideoWidget::uploadTexture() {
    if (frame_.isNull() || !textureDirty_) return;

    glBindTexture(GL_TEXTURE_2D, texture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    if (textureSize_ != frame_.size()) {
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            frame_.width(),
            frame_.height(),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            frame_.constBits()
        );
        textureSize_ = frame_.size();
    } else {
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            frame_.width(),
            frame_.height(),
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            frame_.constBits()
        );
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    textureDirty_ = false;
}

void VideoWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (frame_.isNull() || !program_ || !program_->isLinked()) return;

    uploadTexture();

    const float widgetAspect =
        static_cast<float>(width()) / std::max(height(), 1);
    const float imageAspect =
        static_cast<float>(frame_.width()) / std::max(frame_.height(), 1);

    float scaleX = 1.0f;
    float scaleY = 1.0f;

    if (imageAspect > widgetAspect) {
        scaleY = widgetAspect / imageAspect;
    } else {
        scaleX = imageAspect / widgetAspect;
    }

    program_->bind();
    program_->setUniformValue("uScale", QVector2D(scaleX, scaleY));
    program_->setUniformValue("uTexture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    program_->release();
}
