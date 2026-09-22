#pragma once

#include <QImage>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#include <QSize>

#include <memory>

class QOpenGLShaderProgram;

class VideoWidget final
    : public QOpenGLWidget,
      protected QOpenGLFunctions_3_3_Core {

public:
    explicit VideoWidget(
        QWidget* parent = nullptr
    );

    ~VideoWidget() override;

    void setFrame(
        const QImage& image
    );

    void clearFrame();

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    void uploadTexture();

    QImage frame_;

    std::unique_ptr<
        QOpenGLShaderProgram
    > program_;

    GLuint texture_ = 0;
    GLuint vao_ = 0;

    QSize textureSize_;

    bool textureDirty_ = false;
};