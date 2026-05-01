#ifndef NOTIFIER_H
#define NOTIFIER_H

#include <QObject>
#include <QString>

class Notifier : public QObject
{
    Q_OBJECT
public:
    explicit Notifier(QObject *parent = nullptr);

    void notify(const QString &title, const QString &artist = QString());
};

#endif // NOTIFIER_H
