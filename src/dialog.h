// ELİF VİLDAN TEDİK
//22100011033
#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QTimer>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QList>
#include <QPointF>
#include <QPixmap>

QT_BEGIN_NAMESPACE
namespace Ui { class Dialog; }
QT_END_NAMESPACE

// Ekranda düşen bir meyve ya da bomba
struct Meyve {
    double x;
    double y;
    double vy;        // her karede düşme hızı (piksel)
    bool   bomba;
    bool   kesildi;
    int    kesilmeKare; // kesildikten sonra geçen kare sayısı
};

// Fare izinin bir noktası
struct IzNokta {
    QPointF p;
    int     omur;       // kalan kare sayısı (solma efekti)
    bool    baslangic;  // yeni bir çizginin ilk noktası mı
};

class Dialog : public QDialog
{
    Q_OBJECT

public:
    Dialog(QWidget *parent = nullptr);
    ~Dialog();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void oyunTick();       // her saniye
    void karpuzGoster();   // yeni meyve / bomba
    void kareGuncelle();   // ~60 FPS hareket
    void oyunuBitir();

private:
    Ui::Dialog *ui;

    QList<Meyve>   meyveler;
    QList<IzNokta> iz;

    QTimer *oyunTimer;
    QTimer *karpuzTimer;
    QTimer *kareTimer;

    QPixmap butunKarpuz;
    QPixmap kesilmisKarpuz;
    QPixmap arkaplan;
    QPixmap arkaplanOlcekli;

    int sure;
    int kesilenSayi;
    int kacirilanSayi;
    int can;
    int puan;
    int kombo;
    int enIyiKombo;
    int patlamaKare;   // bomba efekti için

    bool    oyunAktif;
    bool    fareBasili;
    QPointF sonNokta;

    void kesmeKontrol(QPointF a, QPointF b);
    void canKaybet();
    void bitisiBaslat();
    void etiketleriGuncelle();
    void oyunuSifirla();
    void skoruKaydet();
    QList<int> enIyiSkorlar(int adet);
};

#endif // DIALOG_H