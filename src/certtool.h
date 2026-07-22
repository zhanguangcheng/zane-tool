#ifndef CERTTOOL_H
#define CERTTOOL_H

#include <QObject>
#include <QProcess>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>

class CertTool : public QObject
{
    Q_OBJECT
public:
    explicit CertTool(QWidget *mainWidget, const QString &mkcertPath, QObject *parent = nullptr);

    QWidget *createPage();

    QProcess *process() const { return m_certProcess; }

private slots:
    void onCertGenerate();
    void onCertInstallCa();
    void onCertUninstallCa();
    void onCertOutputBrowse();
    void onCertOpenOutputDir();
    void onCertOpenCaroot();
    void onCertProcessOutput();

private:
    void refreshCertCaStatus();

    QWidget *m_mainWidget;
    QString m_mkcertPath;

    QLabel *m_certCaStatusLabel;
    QLabel *m_certCarootLabel;
    QPushButton *m_certInstallCaBtn;
    QPushButton *m_certUninstallCaBtn;
    QPushButton *m_certOpenCarootBtn;
    QTextEdit *m_certDomainsInput;
    QLineEdit *m_certNameEdit;
    QLineEdit *m_certOutputDir;
    QPushButton *m_certOutputBrowseBtn;
    QPushButton *m_certGenerateBtn;
    QPushButton *m_certOpenOutputBtn;
    QTextEdit *m_certLogOutput;
    QProcess *m_certProcess;
    QString m_certCarootPath;
    bool m_certRunning;
};

#endif // CERTTOOL_H
