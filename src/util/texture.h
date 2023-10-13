#include <QSharedPointer>
#include <memory>

class QImage;
class QOpenGLTexture;
class QPixmap;
class Paintable;

QOpenGLTexture* createTexture(const QImage& image);
QOpenGLTexture* createTexture(const QPixmap& pixmap);
QOpenGLTexture* createTexture(const QSharedPointer<Paintable>& pPaintable);
QOpenGLTexture* createTexture(const std::shared_ptr<QImage>& pImage);
