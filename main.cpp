#include <iostream>

#include <QList>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QDebug>
#include <QCommandLineParser>
#include <QXmlStreamWriter>

#include "TexturePacker.h"

using namespace std;

struct Texture
{
    QString fileName;
    QImage image;
};


void packTextures(const QStringList& fileNames, const QString& outputName)
{
    QString mapFileName = outputName + ".png";
    QString plistFileName = outputName + ".plist";

    QList<Texture> textures;

    foreach (const QString& s, fileNames)
    {
        if (s == mapFileName || s == plistFileName)
            continue;

        qDebug() << "Load texture from" << s;
        Texture t;
        t.image = QImage(s);

        if (t.image.isNull())
        {
            qDebug() << "Cant load from" << s;
            continue;
        }

        t.fileName = s;
        textures << t;
    }


    int width = 2;
    int unused = 0;
    TEXTURE_PACKER::TexturePacker *packer = 0;
    while (true)
    {
        qDebug() << "Pack for texture size" << width;
        packer = TEXTURE_PACKER::createTexturePacker();
        packer->setTextureCount(textures.size());
        foreach (const Texture& t, textures)
        {
            packer->addTexture(t.image.width(), t.image.height());
        }
        bool success = packer->packTextures(width, width, true, true, unused);
        if (success)
            break;
        width *= 2;
        TEXTURE_PACKER::releaseTexturePacker(packer);
    }

    qDebug() << "Unused space" << unused;

    QImage result(width, width, QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    QPainter p(&result);

    QFile f(plistFileName);
    if (!f.open(QFile::WriteOnly))
    {
        qCritical() << "Cant write file" << plistFileName;
        return;
    }
    QXmlStreamWriter plistWriter(&f);
    plistWriter.setAutoFormatting(true);

    plistWriter.writeStartDocument();
    plistWriter.writeDTD("<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
    plistWriter.writeStartElement("plist");
    plistWriter.writeAttribute("version", "1.0");
    plistWriter.writeStartElement("dict");
    plistWriter.writeTextElement("key", "frames");
    plistWriter.writeStartElement("dict");

    for (int i = 0; i < textures.size(); i++)
    {
        int x, y, w, h;
        bool rotated = packer->getTextureLocation(i, x, y, w, h);

        if (rotated)
        {
            std::swap(w, h);
        }

        plistWriter.writeTextElement("key", textures[i].fileName);
        plistWriter.writeStartElement("dict");

        plistWriter.writeTextElement("key", "frame");
        plistWriter.writeTextElement("string", QString("{{%1,%2},{%3,%4}}").arg(x).arg(y).arg(w).arg(h));
        plistWriter.writeTextElement("key", "offset");
        plistWriter.writeTextElement("string", "{0,0}");
        plistWriter.writeTextElement("key", "rotated");
        if (rotated)
            plistWriter.writeEmptyElement("true");
        else
            plistWriter.writeEmptyElement("false");
        plistWriter.writeTextElement("key", "sourceSize");
        plistWriter.writeTextElement("string", QString("{%1,%2}").arg(w).arg(h));

        plistWriter.writeEndElement();

        const QImage& img = textures[i].image;

        if (!rotated)
        {
            p.drawImage(x, y, img);
        }
        else
        {
            p.save();
            p.translate(x + img.height(), y);
            p.rotate(90);
            p.drawImage(0, 0, img);
            p.restore();
        }
    }
    plistWriter.writeEndElement();

    plistWriter.writeTextElement("key", "metadata");
    plistWriter.writeStartElement("dict");
    plistWriter.writeTextElement("key", "format");
    plistWriter.writeTextElement("integer", "2");
    plistWriter.writeTextElement("key", "textureFileName");
    plistWriter.writeTextElement("string", mapFileName);
    plistWriter.writeTextElement("key", "realTextureFileName");
    plistWriter.writeTextElement("string", mapFileName);
    plistWriter.writeTextElement("key", "size");
    plistWriter.writeTextElement("string", QString("{%1,%2}").arg(result.width()).arg(result.height()));
    plistWriter.writeEndElement();

    plistWriter.writeTextElement("key", "texture");
    plistWriter.writeStartElement("dict");
    plistWriter.writeTextElement("key", "width");
    plistWriter.writeTextElement("integer", QString::number(result.width()));
    plistWriter.writeTextElement("key", "height");
    plistWriter.writeTextElement("integer", QString::number(result.height()));
    plistWriter.writeEndElement();

    plistWriter.writeEndElement();
    plistWriter.writeEndDocument();

    qDebug() << "Result saved to" << mapFileName << plistFileName;
    result.save(mapFileName);

    TEXTURE_PACKER::releaseTexturePacker(packer);
}

int main()
{
    QDir d;
    packTextures(d.entryList(), "map");
    return 0;
}

