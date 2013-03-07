#ifndef QTRENDERERPLATFORMSUPPORT_H
#define QTRENDERERPLATFORMSUPPORT_H

#include "AbstractRenderer.h"

class QtRendererPlatformSupport : public AbstractRendererPlatformSupport
{
public:
  virtual void RenderTextInOpenGL(
      const char *text,
      int x, int y, int w, int h,
      FontInfo font,
      int align_horiz, int align_vert,
      const Vector3d &rgbf);

};

#endif // QTRENDERERPLATFORMSUPPORT_H
