#pragma once

#include <QFont>
#include <QString>

// The faces the Bible can be read in. Each is bundled (all are under the SIL
// Open Font License) and loaded the first time it's chosen, so startup only
// pays for the one in use. The last is Omarchy's own monospace font.
namespace ReadingFonts {

int count();
QString name(int index);       // "Literata"
bool realSmallCaps(int index); // has OpenType small caps (LORD)
qreal lineHeight(int index);   // line pitch as a multiple of the size

// The face at `pixelSize`, loading it if need be. Variable faces get the
// optical size that matches, as browsers do.
QFont font(int index, int pixelSize, const QString &omarchyFamily);

} // namespace ReadingFonts
