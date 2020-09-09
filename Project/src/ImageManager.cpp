#include "ImageManager.h"

ImageManager *gImageCache;

QPixmap ImageManager::load(const QString &path)
{
    if (pixmaps.find(path) == pixmaps.end())
        pixmaps.insert(path, QPixmap(":/images/" + path));
    return pixmaps[path];
}

void InitImageManager()
{
    gImageCache = new ImageManager;
}

void DestoryImageManager()
{
    delete gImageCache;
}