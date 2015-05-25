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
    QSize origImageSize;
    QPoint offset;
};

QRect getImageRect(const QImage& img)
{
    QRect res(-1, -1, 0, 0);

    for (int i = 0; i < img.height(); i++)
    {
        int left = -1;
        int right = -1;
        const QRgb* rgb = reinterpret_cast<const QRgb*>(img.scanLine(i));
        for (int j = 0; j < img.width(); j++)
        {
            if (qAlpha(*rgb) > 0)
            {
                if (left == -1)
                    left = j;
                right = j;
            }
            rgb++;
        }

        qDebug() << i << left << right;

        bool isEmptyLine = left == -1;

        if (left >= 0)
        {
            if (res.left() == -1)
                res.setLeft(left);
            else
                res.setLeft(qMin(left, res.left()));
        }

        if (right >= 0)
            res.setRight(qMax(right, res.right()));

        if (!isEmptyLine)
        {
            if (res.top() == -1)
                res.setTop(i);
            res.setBottom(i);
        }
    }
    return res;
}


void packTextures(const QStringList& fileNames, const QString& outputName, bool crop)
{
    QString mapFileName = outputName + ".png";
    QString plistFileName = outputName + ".plist";

    QList<Texture> textures;

    foreach (const QString& s, fileNames)
    {
        if (s == mapFileName || s == plistFileName)
            continue;

        qDebug() << "Load texture from" << s;
        QImage img(s);
        if (img.isNull())
        {
            qDebug() << "Cant load from" << s;
            continue;
        }
        img.convertToFormat(QImage::Format_ARGB32);

        Texture t;
        t.fileName = s;
        t.origImageSize = img.size();

        if (crop)
        {
            QRect r = getImageRect(img);
            QImage img2(r.size(), QImage::Format_ARGB32);
            img2.fill(Qt::transparent);
            QPainter p(&img2);
            p.drawImage(QPoint(0, 0), img, r);
            t.image = img2;
            t.offset = r.topLeft();
        }
        else
        {
            t.image = img;
            t.offset = QPoint(0, 0);
        }

        textures << t;
    }


    int width = 2;
    int unused = 0;
    TEXTURE_PACKER::TexturePacker *packer = 0;
    while (true)
    {
        qDebug() << "Try to pack for texture size" << width;
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

        const Texture& t = textures[i];

        plistWriter.writeTextElement("key", QFileInfo(t.fileName).fileName());
        plistWriter.writeStartElement("dict");

        plistWriter.writeTextElement("key", "frame");
        plistWriter.writeTextElement("string", QString("{{%1,%2},{%3,%4}}").arg(x).arg(y).arg(w).arg(h));
        plistWriter.writeTextElement("key", "offset");
        plistWriter.writeTextElement("string", QString("{%1,%2}")
                                     .arg(t.offset.x() - (t.origImageSize.width() - t.image.width()) / 2)
                                     .arg(t.offset.y() - (t.origImageSize.height() - t.image.height())/ 2)
                                     );
        plistWriter.writeTextElement("key", "rotated");
        if (rotated)
            plistWriter.writeEmptyElement("true");
        else
            plistWriter.writeEmptyElement("false");
        plistWriter.writeTextElement("key", "sourceColorRect");
        plistWriter.writeTextElement("string", QString("{{%1,%2},{%3,%4}}").arg(t.offset.x()).arg(t.offset.y())
                                     .arg(t.image.width()).arg(t.image.height()));
        plistWriter.writeTextElement("key", "sourceSize");
                plistWriter.writeTextElement("string", QString("{%1,%2}")
                                             .arg(t.origImageSize.width())
                                             .arg(t.origImageSize.height()));

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

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("Texture packer");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Packs some images to single texture atlas");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("source", QCoreApplication::translate("main", "Source image files"));

    QCommandLineOption outputNameOption(QStringList() << "O" << "output",
                                             QCoreApplication::translate("main", "Creates texture atlas with the specified <name> in current directory. Default name is \"map\""),
                                             QCoreApplication::translate("main", "name"));
    parser.addOption(outputNameOption);
    QCommandLineOption cropEmptyOption(QStringList() << "C" << "crop",
                                        QCoreApplication::translate("main", "Crops empty spaces in textures"));
    parser.addOption(cropEmptyOption);

    parser.process(app);

    QString mapName = parser.value("output");
    if (mapName.isEmpty())
        mapName = "map";

    const QStringList inFiles = parser.positionalArguments();
    if (inFiles.isEmpty())
    {
        qCritical() << "You must specify some image files to pack.\n";
        parser.showHelp();
        return 1;
    }

    packTextures(inFiles, mapName, parser.isSet("crop"));
    return 0;
}

