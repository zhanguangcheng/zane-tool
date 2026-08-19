#ifndef UPDATETOOL_H
#define UPDATETOOL_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

class QWidget;
class QNetworkReply;
class QDialog;
class QLabel;
class QPushButton;

class UpdateTool : public QObject
{
    Q_OBJECT

public:
    static void checkForUpdate(QWidget *parent, bool silentNoUpdate);

private:
    explicit UpdateTool(QWidget *parent, bool silentNoUpdate);

    void processNextVersionSource();
    void versionReplyFinished(QNetworkReply *reply);
    void handleLatest();
    void manualCheck();
    void buildDialog();
    void setDialogSuccess();
    void setDialogFailed();
    void finish();

    QWidget *m_parent;
    QNetworkAccessManager *m_manager;
    bool m_silentNoUpdate;
    int m_sourceIndex = 0;
    QDialog *m_dialog = nullptr;
    QLabel *m_label = nullptr;
    QPushButton *m_proxyBtn = nullptr;
    QPushButton *m_githubBtn = nullptr;
    QString m_latest;
    QString m_proxyUrl;
    QString m_githubUrl;

signals:
    void done();
};

#endif // UPDATETOOL_H