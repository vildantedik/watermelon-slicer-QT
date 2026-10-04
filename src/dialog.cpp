// ELİF VİLDAN TEDİK
//22100011033
#include "dialog.h"
#include "ui_dialog.h"
#include <QFile>
#include <QTextStream>
#include <QPainter>
#include <QPainterPath>
#include <QMessageBox>
#include <QApplication>
#include <QScreen>
#include <QPushButton>
#include <QStandardPaths>
#include <QDir>
#include <QLineF>
#include <QRandomGenerator>
#include <algorithm>
#include <functional>

// ---- Oyun ayarları (istediğin gibi değiştirebilirsin) ----
static const int BOYUT          = 60;   // meyve boyutu
static const int UST_BAR        = 100;  // üstteki etiket alanı
static const int BASLANGIC_SURE = 45;   // saniye
static const int BASLANGIC_CAN  = 3;
static const int IZ_OMRU        = 12;   // kesme izinin kare cinsinden ömrü
static const int KESIK_OMRU     = 30;   // kesilmiş meyvenin kare cinsinden ömrü

// Skorların yazılabilir olduğu yol (her bilgisayarda çalışır)
static QString skorYolu()
{
    QString klasor = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(klasor);
    return klasor + "/skorlar.txt";
}

// Bir noktanın doğru parçasına en kısa uzaklığı
static double noktaSegmentMesafe(QPointF p, QPointF a, QPointF b)
{
    QPointF ab = b - a;
    double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 < 1e-9) return QLineF(p, a).length();
    double t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2;
    t = qBound(0.0, t, 1.0);
    return QLineF(p, a + ab * t).length();
}

// Kodla çizilen bomba (görsel dosyası gerekmez)
static void bombaCiz(QPainter &p, double x, double y)
{
    QPointF c(x + BOYUT / 2.0, y + BOYUT / 2.0 + 4);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(30, 30, 30));
    p.drawEllipse(c, 24, 24);
    p.setBrush(QColor(110, 110, 110, 190));
    p.drawEllipse(QPointF(c.x() - 8, c.y() - 8), 7, 7);
    p.setPen(QPen(QColor(150, 110, 60), 4));
    p.drawLine(QPointF(c.x() + 8, c.y() - 20), QPointF(c.x() + 16, c.y() - 30));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 170, 0));
    p.drawEllipse(QPointF(c.x() + 17, c.y() - 32), 5, 5);
    p.setBrush(QColor(255, 80, 0));
    p.drawEllipse(QPointF(c.x() + 17, c.y() - 32), 2.5, 2.5);
}

// Kodla çizilen kalp (can göstergesi)
static void kalpCiz(QPainter &p, double cx, double cy, double s, bool dolu)
{
    QPainterPath yol;
    yol.moveTo(cx, cy + s * 0.9);
    yol.cubicTo(cx - s * 1.6, cy - s * 0.2, cx - s * 0.6, cy - s * 1.2, cx, cy - s * 0.4);
    yol.cubicTo(cx + s * 0.6, cy - s * 1.2, cx + s * 1.6, cy - s * 0.2, cx, cy + s * 0.9);
    p.setPen(QPen(QColor(120, 0, 20), 2));
    p.setBrush(dolu ? QColor(230, 30, 60) : QColor(0, 0, 0, 90));
    p.drawPath(yol);
}

// Okunaklı (gölgeli) yazı
static void yaziCiz(QPainter &p, int x, int y, const QString &metin, QColor renk)
{
    p.setPen(QColor(0, 0, 0, 200));
    p.drawText(x + 2, y + 2, metin);
    p.setPen(renk);
    p.drawText(x, y, metin);
}

Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Dialog)
{
    ui->setupUi(this);

    // Ekran boyutuna getir
    QSize ekran = QApplication::primaryScreen()->size();
    setFixedSize(ekran);

    // Resimleri yükle (yoksa kodla çizilen yedek görünüm kullanılır)
    butunKarpuz    = QPixmap(":/images/1.png").scaled(BOYUT, BOYUT, Qt::KeepAspectRatio);
    kesilmisKarpuz = QPixmap(":/images/2.png").scaled(BOYUT, BOYUT, Qt::KeepAspectRatio);
    arkaplan       = QPixmap(":/images/back.jpg");
    if (!arkaplan.isNull())
        arkaplanOlcekli = arkaplan.scaled(width(), height() - UST_BAR);

    // Değişkenleri sıfırla
    sure          = BASLANGIC_SURE;
    kesilenSayi   = 0;
    kacirilanSayi = 0;
    can           = BASLANGIC_CAN;
    puan          = 0;
    kombo         = 0;
    enIyiKombo    = 0;
    patlamaKare   = 0;
    oyunAktif     = true;
    fareBasili    = false;

    etiketleriGuncelle();

    // Oyun sayacı (her saniye)
    oyunTimer = new QTimer(this);
    connect(oyunTimer, &QTimer::timeout, this, &Dialog::oyunTick);
    oyunTimer->start(1000);

    // Yeni meyve sayacı (süre geçtikçe hızlanır)
    karpuzTimer = new QTimer(this);
    connect(karpuzTimer, &QTimer::timeout, this, &Dialog::karpuzGoster);
    karpuzTimer->start(900);

    // Hareket sayacı (~60 FPS)
    kareTimer = new QTimer(this);
    connect(kareTimer, &QTimer::timeout, this, &Dialog::kareGuncelle);
    kareTimer->start(16);
}

Dialog::~Dialog()
{
    delete ui;
}

// Her saniye çağrılır
void Dialog::oyunTick()
{
    if (!oyunAktif) return;

    sure--;
    etiketleriGuncelle();

    if (sure <= 0)
        bitisiBaslat();
}

// Yeni meyve ya da bomba ekle (rastgele konum, süreyle artan zorluk)
void Dialog::karpuzGoster()
{
    if (!oyunAktif) return;

    int gecen = BASLANGIC_SURE - sure;
    auto *rnd = QRandomGenerator::global();

    Meyve m;
    m.x           = rnd->bounded(qMax(1, width() - BOYUT));
    m.y           = UST_BAR;
    m.vy          = 3.5 + gecen * 0.08 + rnd->generateDouble() * 2.0;
    m.bomba       = rnd->generateDouble() < (0.12 + gecen * 0.003);
    m.kesildi     = false;
    m.kesilmeKare = 0;
    meyveler.append(m);

    // Süre ilerledikçe daha sık meyve gelsin
    karpuzTimer->setInterval(qMax(350, 900 - gecen * 12));
}

// Her karede: meyveleri düşür, izi söndür
void Dialog::kareGuncelle()
{
    if (!oyunAktif) return;

    for (Meyve &m : meyveler) {
        m.y += m.vy;
        if (m.kesildi) m.kesilmeKare++;
    }

    // Ekranın altına inen kesilmemiş meyve = can kaybı
    int kacan = 0;
    for (const Meyve &m : std::as_const(meyveler)) {
        if (m.y > height() && !m.kesildi && !m.bomba)
            kacan++;
    }

    // Biten / ekran dışına çıkan / kesilip sönen meyveleri sil
    meyveler.removeIf([this](const Meyve &m) {
        return m.y > height()
        || (m.bomba && m.kesildi)
            || (m.kesildi && m.kesilmeKare > KESIK_OMRU);
    });

    // İz noktalarını söndür
    for (IzNokta &n : iz) n.omur--;
    iz.removeIf([](const IzNokta &n) { return n.omur <= 0; });

    if (patlamaKare > 0) patlamaKare--;

    for (int i = 0; i < kacan && oyunAktif; ++i) {
        kacirilanSayi++;
        kombo = 0;
        canKaybet();
    }

    etiketleriGuncelle();
    update();
}

// Fare izi (a -> b) meyvelere değdi mi?
void Dialog::kesmeKontrol(QPointF a, QPointF b)
{
    for (Meyve &m : meyveler) {
        if (m.kesildi) continue;

        QPointF merkez(m.x + BOYUT / 2.0, m.y + BOYUT / 2.0);
        if (noktaSegmentMesafe(merkez, a, b) > BOYUT / 2.0 + 2) continue;

        m.kesildi = true;
        m.kesilmeKare = 0;

        if (m.bomba) {
            // Bomba kesildi: can kaybı + kombo bozulur
            kombo = 0;
            patlamaKare = 14;
            canKaybet();
        } else {
            // Meyve kesildi: kombo arttıkça bonus puan
            kesilenSayi++;
            kombo++;
            enIyiKombo = qMax(enIyiKombo, kombo);
            puan += 1 + kombo / 3;
        }
    }
}

void Dialog::canKaybet()
{
    can--;
    if (can <= 0)
        bitisiBaslat();
}

// Zamanlayıcıları durdurup oyunu bitirir (tek sefer)
void Dialog::bitisiBaslat()
{
    if (!oyunAktif) return;
    oyunAktif = false;
    oyunTimer->stop();
    karpuzTimer->stop();
    kareTimer->stop();
    fareBasili = false;
    QTimer::singleShot(0, this, &Dialog::oyunuBitir);
}

// ---- Fare olayları ----
void Dialog::mousePressEvent(QMouseEvent *event)
{
    if (!oyunAktif) return;
    fareBasili = true;
    sonNokta   = event->position();
    iz.append({sonNokta, IZ_OMRU, true});
}

void Dialog::mouseMoveEvent(QMouseEvent *event)
{
    if (!oyunAktif || !fareBasili) return;

    QPointF simdi = event->position();
    kesmeKontrol(sonNokta, simdi);
    iz.append({simdi, IZ_OMRU, false});
    sonNokta = simdi;
}

void Dialog::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    fareBasili = false;
}

// ---- Çizim ----
void Dialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Arka plan (resim yoksa koyu mavi)
    if (!arkaplanOlcekli.isNull())
        p.drawPixmap(0, UST_BAR, arkaplanOlcekli);
    else
        p.fillRect(0, UST_BAR, width(), height() - UST_BAR, QColor(30, 50, 80));

    // Meyveler ve bombalar
    for (const Meyve &m : std::as_const(meyveler)) {
        if (m.bomba) {
            bombaCiz(p, m.x, m.y);
            continue;
        }

        if (m.kesildi) {
            p.setOpacity(qMax(0.0, 1.0 - double(m.kesilmeKare) / KESIK_OMRU));
            if (!kesilmisKarpuz.isNull()) {
                p.drawPixmap(QPointF(m.x, m.y), kesilmisKarpuz);
            } else {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(220, 50, 70));
                p.drawEllipse(QPointF(m.x + BOYUT / 2.0, m.y + BOYUT / 2.0), 26, 26);
            }
            p.setOpacity(1.0);
        } else if (!butunKarpuz.isNull()) {
            p.drawPixmap(QPointF(m.x, m.y), butunKarpuz);
        } else {
            p.setPen(QPen(QColor(20, 90, 30), 3));
            p.setBrush(QColor(50, 160, 60));
            p.drawEllipse(QPointF(m.x + BOYUT / 2.0, m.y + BOYUT / 2.0), 26, 26);
        }
    }

    // Kesme izi (solarak kaybolur)
    for (int i = 1; i < iz.size(); ++i) {
        if (iz[i].baslangic) continue;
        double oran = double(iz[i].omur) / IZ_OMRU;
        QPen kalem(QColor(255, 255, 255, int(255 * oran)));
        kalem.setWidthF(2 + 6 * oran);
        kalem.setCapStyle(Qt::RoundCap);
        p.setPen(kalem);
        p.drawLine(iz[i - 1].p, iz[i].p);
    }

    // Bomba patlama efekti
    if (patlamaKare > 0)
        p.fillRect(0, UST_BAR, width(), height() - UST_BAR,
                   QColor(255, 60, 0, patlamaKare * 12));

    // Puan, kombo ve canlar
    QFont yazi("Arial", 20, QFont::Bold);
    p.setFont(yazi);
    yaziCiz(p, 20, UST_BAR + 40, QString("Puan: %1").arg(puan), Qt::white);
    if (kombo >= 2)
        yaziCiz(p, 20, UST_BAR + 78, QString("Kombo x%1").arg(kombo), QColor(255, 210, 0));

    for (int i = 0; i < BASLANGIC_CAN; ++i)
        kalpCiz(p, width() - 50 - i * 48, UST_BAR + 36, 14, i < can);
}

// Üst etiketleri güncelle
void Dialog::etiketleriGuncelle()
{
    ui->lb_sure->setText(QString::number(sure));
    ui->lbl_kesilen_sayisi->setText(QString::number(kesilenSayi));
    ui->lbl_kacirilan_sayisi->setText(QString::number(kacirilanSayi));
}

void Dialog::oyunuBitir()
{
    // Önce eski rekoru oku, sonra yeni skoru kaydet
    QList<int> eski = enIyiSkorlar(1);
    int eskiMaks = eski.isEmpty() ? 0 : eski.first();

    skoruKaydet();
    QList<int> ilk5 = enIyiSkorlar(5);

    QString baslik = (puan > eskiMaks)
                         ? "Oyun Bitti! Tebrikler, yeni rekor!"
                         : "Oyun Bitti! Rekoru geçemediniz.";

    QString neden = (can <= 0) ? "Canlarınız bitti." : "Süre doldu.";

    QString liste;
    for (int i = 0; i < ilk5.size(); ++i)
        liste += QString("%1. %2\n").arg(i + 1).arg(ilk5[i]);

    QString mesaj = QString("%1\n%2\n\n"
                            "Puan: %3\n"
                            "Kesilen Meyve: %4\n"
                            "Kaçırılan Meyve: %5\n"
                            "En İyi Kombo: %6\n\n"
                            "En İyi 5 Skor:\n%7")
                        .arg(baslik, neden)
                        .arg(puan).arg(kesilenSayi).arg(kacirilanSayi)
                        .arg(enIyiKombo).arg(liste);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Bilgi");
    msgBox.setText(mesaj);

    QPushButton *tekrarOynaButonu = msgBox.addButton("Tekrar Oyna", QMessageBox::AcceptRole);
    QPushButton *cikisButonu = msgBox.addButton("Çıkış", QMessageBox::RejectRole);

    msgBox.exec();

    if (msgBox.clickedButton() == tekrarOynaButonu) {
        oyunuSifirla();
    } else if (msgBox.clickedButton() == cikisButonu) {
        close();
    }
}

void Dialog::oyunuSifirla()
{
    meyveler.clear();
    iz.clear();

    sure          = BASLANGIC_SURE;
    kesilenSayi   = 0;
    kacirilanSayi = 0;
    can           = BASLANGIC_CAN;
    puan          = 0;
    kombo         = 0;
    enIyiKombo    = 0;
    patlamaKare   = 0;
    fareBasili    = false;
    oyunAktif     = true;

    etiketleriGuncelle();

    oyunTimer->start(1000);
    karpuzTimer->start(900);
    kareTimer->start(16);
    update();
}

// Puanı skorlar.txt'ye kaydet
void Dialog::skoruKaydet()
{
    QFile dosya(skorYolu());
    if (!dosya.open(QIODevice::Append | QIODevice::Text)) return;

    QTextStream out(&dosya);
    out << puan << "\n";
    dosya.close();
}

// En yüksek 'adet' kadar skoru büyükten küçüğe döndürür
QList<int> Dialog::enIyiSkorlar(int adet)
{
    QList<int> liste;

    QFile dosya(skorYolu());
    if (!dosya.open(QIODevice::ReadOnly | QIODevice::Text)) return liste;

    QTextStream in(&dosya);
    while (!in.atEnd()) {
        QString satir = in.readLine().trimmed();
        if (!satir.isEmpty())
            liste.append(satir.toInt());
    }
    dosya.close();

    std::sort(liste.begin(), liste.end(), std::greater<int>());
    if (liste.size() > adet) liste.resize(adet);
    return liste;
}