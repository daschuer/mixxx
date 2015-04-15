#ifndef MPRIS_H
#define MPRIS_H

#include <QObject>

class Mpris : public QObject
{
    Q_OBJECT
  public:
    Mpris(QObject *parent = 0);
    ~Mpris();
};

#endif // MPRIS_H
