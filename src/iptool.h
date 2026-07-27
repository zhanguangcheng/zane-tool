#ifndef IPTOOL_H
#define IPTOOL_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QNetworkAccessManager>

class IpTool : public QWidget
{
    Q_OBJECT

public:
    explicit IpTool(QWidget *parent = nullptr);

private slots:
    void onIpWanQuery();
    void onIpCopyLan();
    void onIpCopyWan();

private:
    void setupUi();
    void refreshLanIps();

    QTextEdit *m_ipLanEdit;
    QPushButton *m_ipCopyLanBtn;
    QComboBox *m_ipWanSourceCombo;
    QPushButton *m_ipWanQueryBtn;
    QLineEdit *m_ipWanEdit;
    QPushButton *m_ipCopyWanBtn;
    QNetworkAccessManager *m_ipNetworkManager;
    QString m_ipLanText;
};

#endif // IPTOOL_H
