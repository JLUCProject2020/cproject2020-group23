#include <QtMultimedia>
#include "Plant.h"
#include "ImageManager.h"
#include "GameScene.h"
#include "GameLevelData.h"
#include "MouseEventPixmapItem.h"
#include "Timer.h"
#include "Animate.h"

Plant::Plant()
        : hp(300),
          pKind(1), bKind(0),
          beAttackedPointL(20), beAttackedPointR(20),
          zIndex(0),
          canEat(true), canSelect(true), night(false),
          coolTime(7.5), stature(0), sleep(0), scene(nullptr)
{}

double Plant::getDX() const
{
    return -0.5 * width;
}

double Plant::getDY(int x, int y) const
{
    return scene->getPlant(x, y).contains(0) ? -21 : -15;
}

bool Plant::canGrow(int x, int y) const
{
    if (x < 1 || x > 9 || y < 1 || y > 5)
        return false;
    if (scene->isTombstone(x, y) || scene->isCrater(x, y))
        return false;
    int groundType = scene->getGameLevelData()->LF[y];
    QMap<int, PlantInstance *> plants = scene->getPlant(x, y);
    if (groundType == 1)
        return !plants.contains(1);
    return plants.contains(0) && !plants.contains(1);
}

void Plant::update()
{
    QPixmap pic = gImageCache->load(staticGif);
    width = pic.width();
    height = pic.height();
}

PlantInstance::PlantInstance(const Plant *plant) : plantProtoType(plant)
{
    uuid = QUuid::createUuid();
    hp = plantProtoType->hp;
    canTrigger = true;
    picture = new MoviePixmapItem;
}

PlantInstance::~PlantInstance()
{
    picture->deleteLater();
}

void PlantInstance::birth(int c, int r)
{
    Coordinate &coordinate = plantProtoType->scene->getCoordinate();
    double x = coordinate.getX(c) + plantProtoType->getDX(), y = coordinate.getY(r) + plantProtoType->getDY(c, r) - plantProtoType->height;
    col = c, row = r;
    attackedLX = x + plantProtoType->beAttackedPointL;
    attackedRX = x + plantProtoType->beAttackedPointR;
    picture->setMovie(plantProtoType->normalGif);
    picture->setPos(x, y);
    picture->setZValue(plantProtoType->zIndex + 3 * r);
    shadowPNG = new QGraphicsPixmapItem(gImageCache->load("interface/plantShadow.png"));
    shadowPNG->setPos(plantProtoType->width * 0.5 - 48, plantProtoType->height - 22);
    shadowPNG->setFlag(QGraphicsItem::ItemStacksBehindParent);
    shadowPNG->setParentItem(picture);
    picture->start();
    plantProtoType->scene->addToGame(picture);
    initTrigger();
}

void PlantInstance::initTrigger()
{
    Trigger *trigger = new Trigger(this, attackedLX, 880, 0, 0);
    triggers.insert(row, QList<Trigger *>{ trigger } );
    plantProtoType->scene->addTrigger(row, trigger);
}

bool PlantInstance::contains(const QPointF &pos)
{
    return picture->contains(pos);
}

void PlantInstance::triggerCheck(ZombieInstance *zombieInstance, Trigger *trigger)
{
    if (zombieInstance->altitude > 0) {
        canTrigger = false;
        QSharedPointer<std::function<void(QUuid)> > triggerCheck(new std::function<void(QUuid)>);
        *triggerCheck = [this, triggerCheck] (QUuid zombieUuid) {
            (new Timer(picture, 1400, [this, zombieUuid, triggerCheck] {
                ZombieInstance *zombie = this->plantProtoType->scene->getZombie(zombieUuid);
                if (zombie) {
                    for (auto i: triggers[zombie->row]) {
                        if (zombie->hp > 0 && i->from <= zombie->ZX && i->to >= zombie->ZX && zombie->altitude > 0) {
                            normalAttack(zombie);
                            (*triggerCheck)(zombie->uuid);
                            return;
                        }
                    }
                }
                canTrigger = true;
            }))->start();
        };
        normalAttack(zombieInstance);
        (*triggerCheck)(zombieInstance->uuid);
    }
}

void PlantInstance::normalAttack(ZombieInstance *zombieInstance)
{
    qDebug() << plantProtoType->cName << uuid << "Attack" << zombieInstance->zombieProtoType->cName << zombieInstance;
}

void PlantInstance::getHurt(ZombieInstance *zombie, int aKind, int attack)
{
    if (aKind == 0)
        hp -= attack;
    if (hp < 1 || aKind != 0)
        plantProtoType->scene->plantDie(this);
}

Peashooter::Peashooter()
{
    eName = "oPeashooter";
    cName = tr("Peashooter");
    beAttackedPointR = 51;
    sunNum = 100;
    cardGif = "Card/Plants/Peashooter.png";
    staticGif = "Plants/Peashooter/0.gif";
    normalGif = "Plants/Peashooter/Peashooter.gif";
    toolTip = tr("Shoots peas at zombies");
}

Repeater::Repeater()
{
    eName = "oRepeater";
    cName = tr("Repeater");
    beAttackedPointR = 51;
    sunNum = 200;
    cardGif = "Card/Plants/Repeater.png";
    staticGif = "Plants/Repeater/0.gif";
    normalGif = "Plants/Repeater/Repeater.gif";
    toolTip = tr("Shoots more peas at zombies");
}

SunFlower::SunFlower()
{
    eName = "oSunflower";
    cName = tr("Sunflower");
    beAttackedPointR = 53;
    sunNum = 50;
    cardGif = "Card/Plants/SunFlower.png";
    staticGif = "Plants/SunFlower/0.gif";
    normalGif = "Plants/SunFlower/SunFlower1.gif";
    toolTip = tr("Makes extra Sun for placing plants");
}

SunFlowerInstance::SunFlowerInstance(const Plant *plant)
        : PlantInstance(plant),
          lightedGif("Plants/SunFlower/SunFlower2.gif")
{

}

void SunFlowerInstance::initTrigger()
{
    (new Timer(picture, 5000, [this] {
        QSharedPointer<std::function<void(void)> > generateSun(new std::function<void(void)>);
        *generateSun = [this, generateSun] {
            picture->setMovieOnNewLoop(lightedGif, [this, generateSun] {
                (new Timer(picture, 1000, [this, generateSun] {
                    auto sunGifAndOnFinished = plantProtoType->scene->newSun(25);
                    MoviePixmapItem *sunGif = sunGifAndOnFinished.first;
                    std::function<void(bool)> onFinished = sunGifAndOnFinished.second;
                    Coordinate &coordinate = plantProtoType->scene->getCoordinate();
                    double fromX = coordinate.getX(col) - sunGif->boundingRect().width() / 2 + 15,
                            toX = coordinate.getX(col) - qrand() % 80,
                            toY = coordinate.getY(row) - sunGif->boundingRect().height();
                    sunGif->setScale(0.6);
                    sunGif->setPos(fromX, toY - 25);
                    sunGif->start();
                    Animate(sunGif).move(QPointF((fromX + toX) / 2, toY - 50)).scale(0.9).speed(0.2).shape(
                                    QTimeLine::EaseOutCurve).finish()
                            .move(QPointF(toX, toY)).scale(1.0).speed(0.2).shape(QTimeLine::EaseInCurve).finish(
                                    onFinished);
                    picture->setMovieOnNewLoop(plantProtoType->normalGif, [this, generateSun] {
                        (new Timer(picture, 24000, [this, generateSun] {
                            (*generateSun)();
                        }))->start();
                    });
                }))->start();
            });
        };
        (*generateSun)();
    }))->start();
}

WallNut::WallNut()
{
    eName = "oWallNut";
    cName = tr("Wall-nut");
    hp = 4000;
    beAttackedPointR = 45;
    sunNum = 50;
    coolTime = 30;
    cardGif = "Card/Plants/WallNut.png";
    staticGif = "Plants/WallNut/0.gif";
    normalGif = "Plants/WallNut/WallNut.gif";
    toolTip = tr("Stops zombies with its chewy shell");
}

bool WallNut::canGrow(int x, int y) const
{
   if (x < 1 || x > 9 || y < 1 || y > 5)
        return false;
    if (scene->isCrater(x, y) || scene->isTombstone(x, y))
        return false;
    int groundType = scene->getGameLevelData()->LF[y];
    QMap<int, PlantInstance *> plants = scene->getPlant(x, y);
    if (groundType == 1)
        return !plants.contains(1) || plants[1]->plantProtoType->eName == "oWallNut";
    return plants.contains(0) && (!plants.contains(1) || plants[1]->plantProtoType->eName == "oWallNut");

}

LawnCleaner::LawnCleaner()
{
    eName = "oLawnCleaner";
    cName = tr("Lawn Cleaner");
    beAttackedPointL = 0;
    beAttackedPointR = 71;
    sunNum = 0;
    staticGif = normalGif = "interface/LawnCleaner.png";
    canEat = 0;
    canSelect = 0;
    stature = 1;
    toolTip = tr("Normal lawn cleaner");
}

PoolCleaner::PoolCleaner()
{
    eName = "oPoolCleaner";
    cName = tr("Pool Cleaner");
    beAttackedPointR = 47;
    staticGif = normalGif = "interface/PoolCleaner.png";
    toolTip = tr("Pool Cleaner");
    update();
}

void WallNutInstance::initTrigger()
{}

WallNutInstance::WallNutInstance(const Plant *plant)
    : PlantInstance(plant)
{
    hurtStatus = 0;
    crackedGif1 = "Plants/WallNut/Wallnut_cracked1.gif";
    crackedGif2 = "Plants/WallNut/Wallnut_cracked2.gif";
}

void WallNutInstance::getHurt(ZombieInstance *zombie, int aKind, int attack)
{
    PlantInstance::getHurt(zombie, aKind, attack);
    if (hp > 0) {
        if (hp < 1334) {
            if (hurtStatus < 2) {
                hurtStatus = 2;
                picture->setMovie(crackedGif2);
                picture->start();
            }
        }
        else if (hp < 2667) {
            if (hurtStatus < 1) {
                hurtStatus = 1;
                picture->setMovie(crackedGif1);
                picture->start();
            }
        }
    }
}

TallNut::TallNut()
{
    eName = "oTallNut";
    cName = tr("Tall-nut");
    hp = 8000;
    beAttackedPointR = 45;
    sunNum = 125;
    coolTime = 30;
    cardGif = "Card/Plants/TallNut.png";
    staticGif = "Plants/TallNut/0.gif";
    normalGif = "Plants/TallNut/TallNut.gif";
    toolTip = tr("Stops zombies with its chewy shell");
}

bool TallNut::canGrow(int x, int y) const
{
    if (x < 1 || x > 9 || y < 1 || y > 5)
        return false;
    if (scene->isCrater(x, y) || scene->isTombstone(x, y))
        return false;
    int groundType = scene->getGameLevelData()->LF[y];
    QMap<int, PlantInstance*> plants = scene->getPlant(x, y);
    if (groundType == 1)
        return !plants.contains(1) || plants[1]->plantProtoType->eName == "oTallNut";
    return plants.contains(0) && (!plants.contains(1) || plants[1]->plantProtoType->eName == "oTallNut");

}

void TallNutInstance::initTrigger()
{}

TallNutInstance::TallNutInstance(const Plant* plant)
    : PlantInstance(plant)
{
    hurtStatus = 0;
    crackedGif1 = "Plants/TallNut/TallnutCracked1.gif";
    crackedGif2 = "Plants/TallNut/TallnutCracked2.gif";
}

void TallNutInstance::getHurt(ZombieInstance* zombie, int aKind, int attack)
{
    PlantInstance::getHurt(zombie, aKind, attack);
    if (hp > 0) {
        if (hp < 2666) {
            if (hurtStatus < 2) {
                hurtStatus = 2;
                picture->setMovie(crackedGif2);
                picture->start();
            }
        }
        else if (hp < 5333) {
            if (hurtStatus < 1) {
                hurtStatus = 1;
                picture->setMovie(crackedGif1);
                picture->start();
            }
        }
    }
}

LawnCleanerInstance::LawnCleanerInstance(const Plant *plant)
    : PlantInstance(plant)
{}

void LawnCleanerInstance::initTrigger()
{
    Trigger *trigger = new Trigger(this, attackedLX, attackedRX, 0, 0);
    triggers.insert(row, QList<Trigger *>{ trigger } );
    plantProtoType->scene->addTrigger(row, trigger);
}

void LawnCleanerInstance::triggerCheck(ZombieInstance *zombieInstance, Trigger *trigger)
{
    if (zombieInstance->beAttacked && zombieInstance->altitude > 0) {
        canTrigger = 0;
        normalAttack(nullptr);
    }
}

void LawnCleanerInstance::normalAttack(ZombieInstance *zombieInstance)
{
    QMediaPlayer *player = new QMediaPlayer(plantProtoType->scene);
    player->setMedia(QUrl("qrc:/audio/lawnmower.mp3"));
    player->play();
    QSharedPointer<std::function<void(void)> > crush(new std::function<void(void)>);
    *crush = [this, crush] {
        for (auto zombie: plantProtoType->scene->getZombieOnRowRange(row, attackedLX, attackedRX)) {
            zombie->crushDie();
        }
        if (attackedLX > 900)
            plantProtoType->scene->plantDie(this);
        else {
            attackedLX += 10;
            attackedRX += 10;
            picture->setPos(picture->pos() + QPointF(10, 0));
            (new Timer(picture, 10, *crush))->start();
        }
    };
    (*crush)();
}

PeashooterInstance::PeashooterInstance(const Plant *plant)
    : PlantInstance(plant), firePea(new QMediaPlayer(picture))
{
    firePea->setMedia(QUrl("qrc:/audio/firepea.mp3"));
}

void PeashooterInstance::normalAttack(ZombieInstance *zombieInstance)
{
    firePea->play();
    (new Bullet(plantProtoType->scene, 0, row, attackedLX, attackedLX - 40, picture->y() + 3, picture->zValue() + 2, 0))->start();
}

RepeaterInstance::RepeaterInstance(const Plant* plant)
    : PlantInstance(plant), firePea(new QMediaPlayer(picture))
{
    firePea->setMedia(QUrl("qrc:/audio/firepea.mp3"));
}

void RepeaterInstance::normalAttack(ZombieInstance* zombieInstance)
{
    firePea->play();
    (new Bullet(plantProtoType->scene, 0, row, attackedLX, attackedLX - 40, picture->y() + 3, picture->zValue() + 2, 0))->start();
    (new Timer(plantProtoType->scene, 200, [this] {
        (new Bullet(plantProtoType->scene, 0, row, attackedLX, attackedLX - 40, picture->y() + 3, picture->zValue() + 2, 0))->start();
    }))->start();
}


Bullet::Bullet(GameScene *scene, int type, int row, qreal from, qreal x, qreal y, qreal zvalue, int direction)
        : scene(scene), type(type), row(row), direction(direction), from(from)
{
    count = 0;
    picture = new QGraphicsPixmapItem(gImageCache->load(QString("Plants/PB%1%2.gif").arg(type).arg(direction)));
    picture->setPos(x, y);
    picture->setZValue(zvalue);
}

Bullet::~Bullet()
{
    delete picture;
}

void Bullet::start()
{
    (new Timer(scene, 10, [this] {
        move();
    }))->start();
}

void Bullet::move()
{
    if (count++ == 10)
        scene->addItem(picture);
    int col = scene->getCoordinate().getCol(from);
    QMap<int, PlantInstance *> plants = scene->getPlant(col, row);
    if (type < 1 && plants.contains(1) && plants[1]->plantProtoType->eName == "oTorchwood") {
        ++type;
        picture->setPixmap(gImageCache->load(QString("Plants/PB%1%2.gif").arg(type).arg(direction)));
    }
    ZombieInstance *zombie = nullptr;
    if (direction == 0) {
        QList<ZombieInstance *> zombies = scene->getZombieOnRow(row);
        for (auto iter = zombies.end(); iter-- != zombies.begin() && (*iter)->attackedLX <= from;) {
            if ((*iter)->hp > 0 && (*iter)->attackedRX >= from) {
                zombie = *iter;
                break;
            }
        }
    }
    // TODO: another direction
    if (zombie && zombie->altitude == 1) {
        // TODO: other attacks
        zombie->getPea(20, direction);
        picture->setPos(picture->pos() + QPointF(28, 0));
        picture->setPixmap(gImageCache->load("Plants/PeaBulletHit.gif"));
        (new Timer(scene, 100, [this] {
            delete this;
        }))->start();
    }
    else {
        from += direction ? -5 : 5;
        if (from < 900 && from > 100) {
            picture->setPos(picture->pos() + QPointF(direction ? -5 : 5, 0));
            (new Timer(scene, 10, [this] {
                move();
            }))->start();
        }
        else
            delete this;
    }
}

CherryBomb::CherryBomb()
{
    eName = "oCherryBomb";
    cName = tr("Cherry Bomb");
    beAttackedPointL = -150;
    beAttackedPointR = 150;
    hp = 300;
    sunNum = 150;
    coolTime = 50;
    cardGif = "Card/Plants/CherryBomb.png";
    staticGif = "Plants/CherryBomb/0.gif";
    normalGif = "Plants/CherryBomb/CherryBomb.gif";
    canEat = 0;
    stature = 1;
    toolTip = tr("Make them ashes.");
}

CherryBombInstance::CherryBombInstance(const Plant* plant)
    : PlantInstance(plant)
{
    emissionStatus = 0;
    rowCounter = -1;
    boomGif = "Plants/CherryBomb/Boom.gif";
}

void CherryBombInstance::initTrigger()
{
    emissionStatus = 0;
    Trigger* trigger = new Trigger(this, attackedLX, attackedRX, 0, 0);
    triggers.insert(row, QList<Trigger*>{ trigger });
    plantProtoType->scene->addTrigger(row, trigger);
    QMediaPlayer* player = new QMediaPlayer(plantProtoType->scene);
    player->setMedia(QUrl("qrc:/audio/cherrybomb.mp3"));
    player->play();
    picture->setMovie(boomGif);
    shadowPNG->setOpacity(0);
    picture->start();
    (new Timer(picture, 2500, [this] {
        if (emissionStatus == 0) {
            plantProtoType->scene->plantDie(this);
        }
        }))->start();
}

void CherryBombInstance::triggerCheck(ZombieInstance* zombieInstance, Trigger* trigger)
{
    if (zombieInstance->beAttacked && zombieInstance->altitude > 0) {
        canTrigger = 0;
        normalAttack(nullptr);
    }
}

void CherryBombInstance::normalAttack(ZombieInstance* zombieInstance)
{
        QSharedPointer<std::function<void(void)> > bomb(new std::function<void(void)>);
        *bomb = [this, bomb] {
            emissionStatus = 1;
            for (rowCounter = -1; rowCounter < 2; rowCounter++){
            for (auto zombie : plantProtoType->scene->getZombieOnRowRange(row + rowCounter, attackedLX, attackedRX)){
                zombie->getAsh();
            }
            if (row + rowCounter == 5) { break; }
            }
            (new Timer(picture, 2500, [this] {
                    plantProtoType->scene->plantDie(this);
                }))->start();
        };
        (*bomb)(); 
}

PotatoMine::PotatoMine()
{
    eName = "oPotatoMine";
    cName = tr("PotatoMine");
    beAttackedPointL = -40;
    beAttackedPointR = 80;
    hp = 300;
    sunNum = 25;
    coolTime = 30;
    cardGif = "Card/Plants/PotatoMine.png";
    staticGif = "Plants/PotatoMine/0.gif";
    normalGif = "Plants/PotatoMine/PotatoMineNotReady.gif";
    canEat = true;
    stature = 1;
    toolTip = tr("Make them ashes.");
}

PotatoMineInstance::PotatoMineInstance(const Plant* plant)
    : PlantInstance(plant)
{
    emissionStatus = 0;
    matureGif = "Plants/PotatoMine/PotatoMine.gif";
    boomGif = "Plants/PotatoMine/PotatoMine_Mashed.gif";
}

void PotatoMineInstance::initTrigger()
{
    shadowPNG->setOpacity(0);
    (new Timer(picture, 16000, [this] {
        QMediaPlayer* player = new QMediaPlayer(plantProtoType->scene);
        player->setMedia(QUrl("qrc:/audio/dirt_rise.mp3"));
        player->play();
        picture->setMovie(matureGif);
        shadowPNG->setOpacity(1);
        picture->start();
        Trigger* trigger = new Trigger(this, attackedLX, attackedRX, 0, 0);
        triggers.insert(row, QList<Trigger*>{ trigger });
        plantProtoType->scene->addTrigger(row, trigger);
    }))->start(); 
}

void PotatoMineInstance::triggerCheck(ZombieInstance* zombieInstance, Trigger* trigger)
{
    if (zombieInstance->beAttacked && zombieInstance->altitude > 0) {
        canTrigger = 0;
        normalAttack(nullptr);
    }
}

void PotatoMineInstance::normalAttack(ZombieInstance* zombieInstance)
{
    QMediaPlayer* player = new QMediaPlayer(plantProtoType->scene);
    player->setMedia(QUrl("qrc:/audio/potato_mine.mp3"));
    player->play();
    picture->setMovie(boomGif);
    shadowPNG->setOpacity(0);
    picture->start();
    QSharedPointer<std::function<void(void)> > potatobomb(new std::function<void(void)>);
    *potatobomb = [this, potatobomb] {
            for (auto zombie : plantProtoType->scene->getZombieOnRowRange(row, attackedLX, attackedRX)) {
                zombie->getAsh();
            }
        (new Timer(picture, 400, [this] {
            plantProtoType->scene->plantDie(this);
        }))->start();
    };
    (*potatobomb)();
}

Plant *PlantFactory(GameScene *scene, const QString &eName)
{
    Plant *plant = nullptr;
    if (eName == "oPeashooter")
        plant = new Peashooter;
    else if (eName == "oRepeater")
        plant = new Repeater;
    else if (eName == "oSunflower")
        plant = new SunFlower;
    else if (eName == "oWallNut")
        plant = new WallNut;
    else if (eName == "oTallNut")
        plant = new TallNut;
    else if (eName == "oLawnCleaner")
        plant = new LawnCleaner;
    else if (eName == "oPoolCleaner")
        plant = new PoolCleaner;
    else if (eName == "oCherryBomb")
        plant = new CherryBomb;
    else if (eName == "oPotatoMine")
        plant = new PotatoMine;
    if (plant) {
        plant->scene = scene;
        plant->update();
    }
    return plant;
}

PlantInstance *PlantInstanceFactory(const Plant *plant)
{
    if (plant->eName == "oPeashooter")
        return new PeashooterInstance(plant);
    if (plant->eName == "oRepeater")
        return new RepeaterInstance(plant);
    if (plant->eName == "oSunflower")
        return new SunFlowerInstance(plant);
    if (plant->eName == "oWallNut")
        return new WallNutInstance(plant);
    if (plant->eName == "oTallNut")
        return new TallNutInstance(plant);
    if (plant->eName == "oLawnCleaner")
        return new LawnCleanerInstance(plant);
    if (plant->eName == "oCherryBomb")
        return new CherryBombInstance(plant);
    if (plant->eName == "oPotatoMine")
        return new PotatoMineInstance(plant);
    return new PlantInstance(plant);
}



