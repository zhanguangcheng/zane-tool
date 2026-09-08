#include "ziptool.h"

#include <QFile>

#include <cstring>

#include <zlib.h>

namespace {

quint16 le16(const QByteArray &d, int off)
{
    const quint8 b0 = static_cast<quint8>(d.at(off));
    const quint8 b1 = static_cast<quint8>(d.at(off + 1));
    return static_cast<quint16>(b0 | (static_cast<quint16>(b1) << 8));
}

quint32 le32(const QByteArray &d, int off)
{
    quint32 v = 0;
    for (int i = 0; i < 4; ++i)
        v |= static_cast<quint32>(static_cast<quint8>(d.at(off + i))) << (8 * i);
    return v;
}

bool inflateRaw(const QByteArray &compressed, QByteArray *out, QString *errorMessage)
{
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
        if (errorMessage)
            *errorMessage = QStringLiteral("zlib 初始化失败");
        return false;
    }

    out->clear();
    QByteArray buf(64 * 1024, Qt::Uninitialized);
    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));
    zs.avail_in = static_cast<uInt>(compressed.size());

    int ret = Z_OK;
    do {
        zs.next_out = reinterpret_cast<Bytef *>(buf.data());
        zs.avail_out = static_cast<uInt>(buf.size());
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&zs);
            if (errorMessage)
                *errorMessage = QStringLiteral("ZIP 解压失败 (zlib 错误 %1)").arg(ret);
            return false;
        }
        const uInt produced = static_cast<uInt>(buf.size()) - zs.avail_out;
        if (produced > 0)
            out->append(buf.constData(), produced);
    } while (ret != Z_STREAM_END);

    inflateEnd(&zs);
    return true;
}

bool deflateRaw(const QByteArray &raw, QByteArray *out)
{
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return false;

    out->clear();
    QByteArray buf(64 * 1024, Qt::Uninitialized);
    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(raw.constData()));
    zs.avail_in = static_cast<uInt>(raw.size());

    int ret = Z_OK;
    do {
        zs.next_out = reinterpret_cast<Bytef *>(buf.data());
        zs.avail_out = static_cast<uInt>(buf.size());
        ret = deflate(&zs, Z_FINISH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            deflateEnd(&zs);
            return false;
        }
        const uInt produced = static_cast<uInt>(buf.size()) - zs.avail_out;
        if (produced > 0)
            out->append(buf.constData(), produced);
    } while (ret != Z_STREAM_END);

    deflateEnd(&zs);
    return true;
}

quint32 crc32(const QByteArray &data)
{
    static quint32 table[256] = {0};
    static bool initialized = false;
    if (!initialized) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        initialized = true;
    }

    quint32 crc = 0xFFFFFFFFu;
    for (int i = 0; i < data.size(); ++i)
        crc = table[(crc ^ static_cast<quint32>(static_cast<quint8>(data.at(i)))) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

void putLe(QByteArray *buf, quint32 value, int bytes)
{
    for (int i = 0; i < bytes; ++i)
        buf->append(static_cast<char>((value >> (8 * i)) & 0xFF));
}

} // namespace

bool zipRead(const QString &filePath, QList<ZipEntry> *entries, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    const int size = data.size();
    if (size < 22) {
        if (errorMessage)
            *errorMessage = QStringLiteral("不是有效的 ZIP 文件");
        return false;
    }

    int eocd = -1;
    const int maxBack = qMin(size, 22 + 65535);
    for (int i = size - 22; i >= size - maxBack; --i) {
        if (i < 0)
            break;
        if (le32(data, i) == 0x06054b50u) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("找不到 ZIP 中央目录");
        return false;
    }

    const quint32 cdOffset = le32(data, eocd + 16);
    const quint16 cdCount = le16(data, eocd + 10);

    entries->clear();
    quint32 pos = cdOffset;
    for (int i = 0; i < cdCount; ++i) {
        if (pos + 46 > static_cast<quint32>(size) || le32(data, pos) != 0x02014b50u) {
            if (errorMessage)
                *errorMessage = QStringLiteral("ZIP 中央目录损坏");
            return false;
        }

        const quint16 method = le16(data, pos + 10);
        const quint32 compSize = le32(data, pos + 20);
        const quint16 nameLen = le16(data, pos + 28);
        const quint16 extraLen = le16(data, pos + 30);
        const quint16 commentLen = le16(data, pos + 32);
        const quint32 localOffset = le32(data, pos + 42);
        const int nameStart = static_cast<int>(pos) + 46;

        if (nameStart + nameLen > size) {
            if (errorMessage)
                *errorMessage = QStringLiteral("ZIP 文件名超界");
            return false;
        }

        const QString name = QString::fromUtf8(data.mid(nameStart, nameLen));

        quint32 dataStart = localOffset;
        if (localOffset + 30 <= static_cast<quint32>(size) && le32(data, localOffset) == 0x04034b50u) {
            const quint16 lNameLen = le16(data, localOffset + 26);
            const quint16 lExtraLen = le16(data, localOffset + 28);
            dataStart = localOffset + 30 + lNameLen + lExtraLen;
        }

        if (dataStart > static_cast<quint32>(size)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("ZIP 本地头超界");
            return false;
        }

        ZipEntry e;
        e.name = name;
        const int avail = size - static_cast<int>(dataStart);
        const int toRead = qMin<qint64>(static_cast<qint64>(compSize), static_cast<qint64>(qMax(0, avail)));
        const QByteArray raw = data.mid(static_cast<int>(dataStart), toRead);

        if (method == 0) {
            e.data = raw;
        } else if (method == 8) {
            if (!inflateRaw(raw, &e.data, errorMessage))
                return false;
        } else {
            if (errorMessage)
                *errorMessage = QStringLiteral("不支持的 ZIP 压缩方式 (%1)").arg(method);
            return false;
        }

        entries->append(e);
        pos = nameStart + nameLen + extraLen + commentLen;
    }

    return true;
}

bool zipWrite(const QString &filePath, const QList<ZipEntry> &entries, QString *errorMessage)
{
    struct OutEntry
    {
        QString name;
        QByteArray comp;
        quint32 crc = 0;
        quint16 method = 0;
        quint32 rawSize = 0;
    };

    QVector<OutEntry> out;
    out.reserve(entries.size());
    for (const ZipEntry &entry : entries) {
        OutEntry e;
        e.name = entry.name;
        e.crc = crc32(entry.data);
        e.rawSize = static_cast<quint32>(entry.data.size());
        QByteArray comp;
        if (deflateRaw(entry.data, &comp) && comp.size() < entry.data.size()) {
            e.comp = comp;
            e.method = 8;
        } else {
            e.comp = entry.data;
            e.method = 0;
        }
        out.append(e);
    }

    QByteArray zip;
    QVector<quint32> offsets;
    offsets.reserve(out.size());

    for (const OutEntry &e : out) {
        offsets.append(static_cast<quint32>(zip.size()));
        const QByteArray nameBytes = e.name.toUtf8();

        putLe(&zip, 0x04034b50u, 4);   // local file header signature
        putLe(&zip, 20, 2);            // version needed to extract
        putLe(&zip, 0x0800, 2);        // general purpose bit flag: UTF-8 names
        putLe(&zip, e.method, 2);      // compression method
        putLe(&zip, 0, 2);             // last mod time
        putLe(&zip, 0x21, 2);          // last mod date (1980-01-01)
        putLe(&zip, e.crc, 4);
        putLe(&zip, e.comp.size(), 4); // compressed size
        putLe(&zip, e.rawSize, 4);     // uncompressed size
        putLe(&zip, nameBytes.size(), 2);
        putLe(&zip, 0, 2);             // extra field length
        zip.append(nameBytes);
        zip.append(e.comp);
    }

    const quint32 centralDirOffset = static_cast<quint32>(zip.size());
    for (int i = 0; i < out.size(); ++i) {
        const OutEntry &e = out.at(i);
        const QByteArray nameBytes = e.name.toUtf8();

        putLe(&zip, 0x02014b50u, 4);   // central directory signature
        putLe(&zip, 20, 2);            // version made by
        putLe(&zip, 20, 2);            // version needed to extract
        putLe(&zip, 0x0800, 2);        // general purpose bit flag
        putLe(&zip, e.method, 2);      // compression method
        putLe(&zip, 0, 2);             // last mod time
        putLe(&zip, 0x21, 2);          // last mod date
        putLe(&zip, e.crc, 4);
        putLe(&zip, e.comp.size(), 4);
        putLe(&zip, e.rawSize, 4);
        putLe(&zip, nameBytes.size(), 2);
        putLe(&zip, 0, 2);             // extra field length
        putLe(&zip, 0, 2);             // file comment length
        putLe(&zip, 0, 2);             // disk number start
        putLe(&zip, 0, 2);             // internal file attributes
        putLe(&zip, 0, 4);             // external file attributes
        putLe(&zip, offsets.at(i), 4);
        zip.append(nameBytes);
    }

    const quint32 centralDirSize = static_cast<quint32>(zip.size()) - centralDirOffset;
    putLe(&zip, 0x06054b50u, 4);       // end of central directory signature
    putLe(&zip, 0, 2);                 // disk number
    putLe(&zip, 0, 2);                 // disk with central directory
    putLe(&zip, out.size(), 2);
    putLe(&zip, out.size(), 2);
    putLe(&zip, centralDirSize, 4);
    putLe(&zip, centralDirOffset, 4);
    putLe(&zip, 0, 2);                 // comment length

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    file.write(zip);
    file.close();
    return true;
}