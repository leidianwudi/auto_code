/**
 * @file gen_icon.cpp
 * @brief 构建时工具：生成 res/app_icon.ico
 *
 * 由 CMake 自动编译，手动运行一次生成 ICO 文件。
 * 生成包含多尺寸（16/32/48/256）的标准 ICO 文件，
 * 使用 BMP（DIB）格式存储图像数据，确保最大兼容性。
 *
 * 用法：
 *   build> gen_icon.exe D:/work/github/auto_code/res/app_icon.ico
 */

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace {

QPixmap createIconPixmap(int size) {
  QPixmap px(size, size);
  px.fill(Qt::transparent);
  QPainter p(&px);
  p.setRenderHint(QPainter::Antialiasing);

  const qreal r = size * 0.2;
  QPainterPath path;
  path.addRoundedRect(QRectF(0, 0, size, size), r, r);
  p.fillPath(path, QColor(0xe0, 0xe0, 0xe0));

  QPen pen(QColor(0x33, 0x33, 0x33), qMax(1.0, size * 0.06));
  pen.setCapStyle(Qt::RoundCap);
  p.setPen(pen);
  QFont f(QStringLiteral("Consolas"), qMax(1, size * 11 / 20), QFont::Bold);
  p.setFont(f);
  p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, QStringLiteral("AC"));
  p.end();
  return px;
}

#pragma pack(push, 1)
struct IcoDir {
  quint16 reserved;
  quint16 type;
  quint16 count;
};

struct IcoDirEntry {
  quint8 width;
  quint8 height;
  quint8 colorCount;
  quint8 reserved;
  quint16 planes;
  quint16 bitCount;
  quint32 dataSize;
  quint32 dataOffset;
};

struct BmpInfoHeader {
  quint32 size;
  quint32 width;
  quint32 height;
  quint16 planes;
  quint16 bitCount;
  quint32 compression;
  quint32 imageSize;
  quint32 xPelsPerMeter;
  quint32 yPelsPerMeter;
  quint32 clrUsed;
  quint32 clrImportant;
};
#pragma pack(pop)

QByteArray imageToDib(const QImage &img) {
  QImage src = img.convertToFormat(QImage::Format_ARGB32);
  const int w = src.width();
  const int h = src.height();
  const int rowBytes = w * 4;
  const int andRowBytes = ((w + 31) / 32) * 4;
  const int xorSize = rowBytes * h;
  const int andSize = andRowBytes * h;
  const int dibSize = sizeof(BmpInfoHeader) + xorSize + andSize;

  QByteArray dib(dibSize, 0);
  char *ptr = dib.data();

  BmpInfoHeader hdr{};
  hdr.size = sizeof(BmpInfoHeader);
  hdr.width = w;
  hdr.height = h * 2;
  hdr.planes = 1;
  hdr.bitCount = 32;
  hdr.compression = 0;
  hdr.imageSize = xorSize + andSize;
  hdr.xPelsPerMeter = 3780;
  hdr.yPelsPerMeter = 3780;
  memcpy(ptr, &hdr, sizeof(hdr));
  ptr += sizeof(hdr);

  for (int y = h - 1; y >= 0; --y) {
    const uchar *scan = src.constScanLine(y);
    memcpy(ptr, scan, rowBytes);
    ptr += rowBytes;
  }

  memset(ptr, 0, andSize);

  return dib;
}

bool saveIco(const QString &path) {
  const QList<int> sizes = {16, 32, 48, 256};
  QList<QByteArray> dibDatas;

  for (int s : sizes) {
    QImage img = createIconPixmap(s).toImage();
    dibDatas.append(imageToDib(img));
  }

  const int count = sizes.size();
  const int headerSize = sizeof(IcoDir) + count * sizeof(IcoDirEntry);
  int offset = headerSize;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning("Cannot open: %s", qPrintable(path));
    return false;
  }

  IcoDir dir{};
  dir.reserved = 0;
  dir.type = 1;
  dir.count = static_cast<quint16>(count);
  file.write(reinterpret_cast<const char *>(&dir), sizeof(dir));

  for (int i = 0; i < count; ++i) {
    IcoDirEntry entry{};
    entry.width = static_cast<quint8>(sizes[i] >= 256 ? 0 : sizes[i]);
    entry.height = static_cast<quint8>(sizes[i] >= 256 ? 0 : sizes[i]);
    entry.colorCount = 0;
    entry.reserved = 0;
    entry.planes = 1;
    entry.bitCount = 32;
    entry.dataSize = static_cast<quint32>(dibDatas[i].size());
    entry.dataOffset = static_cast<quint32>(offset);
    file.write(reinterpret_cast<const char *>(&entry), sizeof(entry));
    offset += dibDatas[i].size();
  }

  for (const auto &ba : dibDatas) {
    file.write(ba);
  }

  file.close();
  return true;
}

}  // namespace

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  QString outPath = QStringLiteral("res/app_icon.ico");
  if (argc > 1) outPath = QString::fromLocal8Bit(argv[1]);

  if (saveIco(outPath)) {
    qInfo("OK: %s", qPrintable(QFileInfo(outPath).absoluteFilePath()));
    return 0;
  }
  qWarning("FAIL: %s", qPrintable(outPath));
  return 1;
}