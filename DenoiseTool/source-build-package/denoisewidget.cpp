#include "denoisewidget.h"

#include "ui_DenoiseWidget.h"
#include "ui_DenoiseControlPanel.h"
#include "t4.h"
#include "t4/IPyThread.h"
#include "StringConstants.h"
#include "BGTaskProgressDialog.h"

#include "geo/LevelWidget.h"
#include "MathFuncs.h"

#include <QTimer>
#include <QPalette>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>
#include <QStandardPaths>
#include <QApplication>
#include <QProgressDialog>
#include <QThread>
#include <QCoreApplication>
#include <QEventLoop>


namespace {

/** Пути с '\' в подставляемых в Python строках дают предупреждения/ошибки (\\E в C:\\ESDL\\...). */
QString pathArgForPython(const QString& absolutePath)
{
    if (absolutePath.isEmpty())
        return {};
    return QDir::fromNativeSeparators(QDir::cleanPath(absolutePath));
}

void appendAutoDenoiseLog(const QString& line)
{
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
    const QString text = stamp + QLatin1Char(' ') + line + QLatin1Char('\n');
    const QStringList paths = {
        QStringLiteral("C:/t4vis/autodenoise_cpp.log"),
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/t4_autodenoise_cpp.log"),
    };
    for (const QString& path : paths)
    {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            QTextStream ts(&f);
            ts << text;
        }
    }
}

/** Если denoise.py реально выполнялся, он создаёт PY_RAN.txt (см. начало скрипта). */
void appendPyRanProbeLog()
{
    const QStringList check = {
        QStringLiteral("C:/t4vis/Scripts/analytics/WRAPPER_RAN.txt"),
        QStringLiteral("C:/t4vis/Scripts/analytics/autodenoise/PY_RAN.txt"),
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/t4_denoise_PY_RAN.txt"),
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + QStringLiteral("/t4_denoise_PY_RAN.txt"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/t4_denoise_PY_RAN.txt"),
    };
    for (const QString& p : check)
    {
        appendAutoDenoiseLog(
            QStringLiteral("PY_RAN exists=") + QString::number(QFile::exists(p) ? 1 : 0)
            + QStringLiteral(" path=") + p);
    }
}

int readProgressPercent(const QString& progressFilePath)
{
    QFile f(progressFilePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return -1;
    const QString txt = QString::fromUtf8(f.readAll()).trimmed();
    bool ok = false;
    const int p = txt.toInt(&ok);
    if (!ok)
        return -1;
    return qBound(0, p, 100);
}

bool allowFiltrationFallback()
{
    return qEnvironmentVariableIntValue("T4_DENOISE_ALLOW_FILTRATION_FALLBACK") == 1;
}

bool filtrationHasDenoiseHook(const QString& scriptsRoot)
{
    if (scriptsRoot.isEmpty())
        return false;
    const QString fp = QDir::cleanPath(scriptsRoot + QStringLiteral("/analytics/filtration.py"));
    QFile f(fp);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    const QString firstChunk = QString::fromUtf8(f.read(512));
    return firstChunk.contains(QStringLiteral("filtration_autodenoise_hook.py"));
}

QString wrapperProbePath(const QString& scriptsRoot)
{
    return pathArgForPython(QDir::cleanPath(scriptsRoot + QStringLiteral("/analytics/WRAPPER_RAN.txt")));
}

bool probeFileUpdated(const QString& path, const QDateTime& before)
{
    const QFileInfo fi(path);
    if (!fi.exists())
        return false;
    return !before.isValid() || fi.lastModified() > before;
}

QString denoiseLastRunPath(const QString& scriptsRoot)
{
    if (scriptsRoot.isEmpty())
        return {};
    return pathArgForPython(
        QDir::cleanPath(scriptsRoot + QStringLiteral("/analytics/autodenoise/denoise_last_run.txt")));
}

bool cudaFallbackDetected(const QString& scriptsRoot)
{
    const QString p = denoiseLastRunPath(scriptsRoot);
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    const QString txt = QString::fromUtf8(f.readAll());
    return txt.contains(QStringLiteral("requested_device': 'CUDA'"))
        && txt.contains(QStringLiteral("resolved_device': 'cpu'"));
}

} // namespace

DenoiseWidget::DenoiseWidget(Ui::DenoiseControlPanel* ctrlPanel, IApplicationData* appData)
    : ui_(new Ui::DenoiseWidget)
    , ctrlPanel_(ctrlPanel)
    , appData_(appData)
    , leaving_(false)
{
    ui_->setupUi(this);
    ui_->YFrame->setAppData(appData_);
    ui_->YFrameFiltered->setAppData(appData_);

    QPalette palette = ui_->YFrame->palette();
    QBrush brush(QColor(0, 0, 0, 255));
    brush.setStyle(Qt::SolidPattern);
    palette.setBrush(QPalette::Inactive, QPalette::Base, brush);
    palette.setBrush(QPalette::Inactive, QPalette::Window, brush);
    ui_->YFrame->setPalette(palette);
    ui_->YFrameFiltered->setPalette(palette);

    volSrc_ = appData->volumeSource2();

    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &DenoiseWidget::updateSlice);
    timer_->setInterval(100);

    if (IQVolumeSource* qvolSrc = dynamic_cast<IQVolumeSource*>(volSrc_))
    {
        connect(qvolSrc, &IQVolumeSource::newVolumeOpened, this, &DenoiseWidget::newVolumeOpened);
        connect(qvolSrc, &IQVolumeSource::volumeClosed, this, &DenoiseWidget::volumeClosed);
    }

    connect(ctrlPanel_->testSliceButton, &QPushButton::clicked, this, &DenoiseWidget::testSlice);
    connect(ctrlPanel_->startButton, &QPushButton::clicked, this, &DenoiseWidget::startFullVolume);
    connect(ctrlPanel_->selectOutDirButton, &QPushButton::clicked, this, &DenoiseWidget::selectOutDir);

    connect(ui_->YSlider, &QSlider::valueChanged, this, &DenoiseWidget::sliderValueChanged);

    ctrlPanel_->modelCmb->clear();
    {
        QDir dir(QStringLiteral("ai-models/denoise"));
        const QStringList files =
            dir.entryList(QStringList() << QStringLiteral("*.pth") << QStringLiteral("*.pt"),
                          QDir::Files,
                          QDir::Name);
        for (const QString& f : files)
            ctrlPanel_->modelCmb->addItem(f);
    }
    if (ctrlPanel_->modelCmb->count() == 0)
        ctrlPanel_->modelCmb->addItem(QStringLiteral("swinir.pth"));
    if (ctrlPanel_->deviceCmb->count() == 0)
    {
        ctrlPanel_->deviceCmb->addItem(QStringLiteral("Auto"));
        ctrlPanel_->deviceCmb->addItem(QStringLiteral("CUDA"));
        ctrlPanel_->deviceCmb->addItem(QStringLiteral("CPU"));
    }
    ctrlPanel_->deviceCmb->setCurrentText(QStringLiteral("Auto"));

    ctrlPanel_->denoiseControl->setEnabled(true);
    init();

    connect(this, &DenoiseWidget::setData_s, this, &DenoiseWidget::setData);
}

DenoiseWidget::~DenoiseWidget()
{
    delete ui_;
    ctrlPanel_->denoiseControl->setEnabled(false);
}

QString DenoiseWidget::pythonScriptPath() const
{
    const QString root = QSettings().value(PTHGroupLine + PTHPythonSystemPath).toString().trimmed();
    if (root.isEmpty())
        return {};
    // Без зависимости от filtration.py/hook: отдельная entrypoint-обёртка плагина.
    const QString joined = QDir::cleanPath(root + QStringLiteral("/analytics/denoise_volumeop.py"));
    return pathArgForPython(joined);
}

QString DenoiseWidget::filtrationScriptPath() const
{
    const QString root = QSettings().value(PTHGroupLine + PTHPythonSystemPath).toString().trimmed();
    if (root.isEmpty())
        return {};
    const QString joined = QDir::cleanPath(root + QStringLiteral("/analytics/filtration.py"));
    return pathArgForPython(joined);
}

QString DenoiseWidget::buildInterpreterCommand(const QString& args) const
{
    return pythonScriptPath() + QLatin1Char('|') + args;
}

QString DenoiseWidget::buildPythonArgs(
    int singleSlice,
    const QString& filtpathAbsolute,
    const QString& progressPathAbsolute)
{
    const QString modelFile = ctrlPanel_->modelCmb->currentText();
    const QString fmt = ctrlPanel_->cmbBinFormat->currentText();
    const QString device = ctrlPanel_->deviceCmb->currentText();

    // filterParams парсим в denoise.py: denoise:<модель>:<Tiff8|Tiff16>:<Auto|CUDA|CPU>
    QStringList parts;
    parts << QStringLiteral("sliceNumber=") + QString::number(singleSlice);
    parts << QStringLiteral("filterParams='denoise:") + modelFile + QStringLiteral(":") + fmt
          + QStringLiteral(":") + device + QStringLiteral("'");

    if (!filtpathAbsolute.isEmpty())
        parts << QStringLiteral("filtpath='") + pathArgForPython(filtpathAbsolute) + QStringLiteral("'");
    if (!progressPathAbsolute.isEmpty())
        parts << QStringLiteral("progress_path='") + pathArgForPython(progressPathAbsolute) + QStringLiteral("'");

    return parts.join(QLatin1Char(','));
}

void DenoiseWidget::init(double minLevel, double maxLevel)
{
    ctrlPanel_->denoiseControl->setEnabled(volSrc_ && volSrc_->isLoaded());
    if (!volSrc_ || !volSrc_->isLoaded() || leaving_)
        return;
    if (volSrc_->dim().y() < 1)
        return;

    SharedImageDataType frame = volSrc_->getYSlice(volSrc_->dim().y() / 2, true);

    double minV = 0;
    double maxV = 0;
    frame->minMaxValues(&minV, &maxV);
    if (minLevel == DBL_MAX)
    {
        minLevel = minV;
        maxLevel = maxV;
    }
    ui_->YFrame->setLevel(minV, maxV);
    LevelWidget* levelWidget = ui_->YFrame->levelWidget();
    const bool visible = levelWidget->isVisible();
    levelWidget->clear();
    levelWidget->setFrame(frame);
    levelWidget->updateLevel(minLevel, maxLevel);
    if (visible)
        levelWidget->show();

    timer_->setInterval(-1);
    ui_->YSlider->setMaximum(volSrc_->dim().y() - 1);
    ui_->YSlider->setValue(volSrc_->currentSliceY());
    timer_->setInterval(100);

    setupFrame();
}

QString DenoiseWidget::sliceLabel(int slice)
{
    const int labelType = QSettings().value(GeoGroupLine + GeoSliceLabelType).toInt();
    if (labelType == 0)
        return QString::number(slice);

    const int offset = 0;
    const QString mm =
        QString::number((slice + offset) * volSrc_->spacing0(), 'f', 1) + tr(" мм");
    if (labelType == 2)
        return mm + QStringLiteral(" [") + QString::number(slice) + QStringLiteral("]");
    return mm;
}

void DenoiseWidget::setupFrame()
{
    if (!volSrc_ || !volSrc_->isLoaded())
        return;
    SharedImageDataType frame = volSrc_->getYSlice(ui_->YSlider->value());
    ui_->YFrame->setFrame(frame);
    ui_->YLabel->setText(sliceLabel(ui_->YSlider->value()));
    ui_->YFrameFiltered->clear();
}

void DenoiseWidget::sliderValueChanged(int)
{
    if (timer_->interval() > 0)
        timer_->start();
}

void DenoiseWidget::updateSlice()
{
    setupFrame();
    timer_->stop();
}

void DenoiseWidget::newVolumeOpened()
{
    init();
}

void DenoiseWidget::volumeClosed()
{
    ui_->YFrame->clear();
    ui_->YFrameFiltered->clear();
}

void DenoiseWidget::selectOutDir()
{
    QString startDir = QDir::currentPath();
    if (volSrc_ && !volSrc_->fileName().isEmpty())
        startDir = QFileInfo(volSrc_->fileName()).absolutePath();
    const QString d = QFileDialog::getExistingDirectory(
        this,
        tr("Папка для сохранения денойзинга"),
        startDir);
    if (!d.isEmpty())
        ctrlPanel_->outDirEdt->setText(d);
}

void DenoiseWidget::testSlice()
{
    if (ctrlPanel_->modelCmb->currentIndex() < 0)
        return;
    const QString scriptsRoot = QSettings().value(PTHGroupLine + PTHPythonSystemPath).toString().trimmed();
    if (scriptsRoot.isEmpty())
    {
        QMessageBox::warning(
            this,
            tr("Денойзинг"),
            tr("Не задан путь к каталогу Python Scripts (настройки PTH)."));
        return;
    }
    if (!volSrc_ || !volSrc_->isLoaded())
        return;

    QString outDir = ctrlPanel_->outDirEdt->text();
    if (QFileInfo(outDir).isRelative())
        outDir = QFileInfo(volSrc_->fileName()).absolutePath() + QLatin1Char('/') + outDir;
    if (!outDir.isEmpty())
        QDir().mkpath(outDir);

    const QString args = buildPythonArgs(ui_->YSlider->value(), pathArgForPython(outDir), QString());
    const QString cmd = buildInterpreterCommand(args);

    ExecBGTaskProgressDialog(this, tr("Тестирование"),
        [&]() { QString script = QSettings().value(PTHGroupLine + PTHPythonSystemPath).toString() + "/analytics/autodenoise/denoise.py";
                appData_->pyThread()->executeInterpreter(script + "|" + args, true); }
    );
    return;

    const QString probePath = wrapperProbePath(scriptsRoot);
    const QDateTime probeBefore = QFileInfo(probePath).exists() ? QFileInfo(probePath).lastModified() : QDateTime();
    const bool cudaRequested =
        ctrlPanel_->deviceCmb->currentText().compare(QStringLiteral("CUDA"), Qt::CaseInsensitive) == 0;
    appendAutoDenoiseLog(QStringLiteral("testSlice cmd=") + cmd);
    if (IPyThread* py = appData_->pyThread())
    {
        const QString sp = pythonScriptPath();
        appendAutoDenoiseLog(QStringLiteral("denoise entry exists=")
                             + QString::number(QFileInfo::exists(sp))
                             + QStringLiteral(" path=") + sp);
    }
    else
        appendAutoDenoiseLog(QStringLiteral("testSlice ERROR pyThread is null"));

    // PyThread::executeInterpreter нужно вызывать из потока GUI. BGTaskProgressDialog
    // выполняет setFunction в фоновом потоке - скрипт не исполнялся (WRAPPER_RAN не создавался).
    IPyThread* py = appData_->pyThread();
    if (!py)
        return;
    appendAutoDenoiseLog(
        QStringLiteral("executeInterpreter onGuiThread=")
        + QString::number(QThread::currentThread() == QCoreApplication::instance()->thread()));

    QProgressDialog progressDlg(tr("Денойзинг среза..."), QString(), 0, 0, this);
    progressDlg.setCancelButton(nullptr);
    progressDlg.setWindowModality(Qt::ApplicationModal);
    progressDlg.setMinimumDuration(0);
    progressDlg.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool pyErr = false;
    const bool ok = py->executeInterpreter(cmd, true, &pyErr);
    QApplication::restoreOverrideCursor();
    progressDlg.close();

    const bool primaryRan = probeFileUpdated(probePath, probeBefore);
    appendAutoDenoiseLog(QStringLiteral("wrapperProbeUpdated=") + QString::number(primaryRan));

    bool finalOk = ok;
    bool finalErr = pyErr;
    if (!primaryRan && (allowFiltrationFallback() || filtrationHasDenoiseHook(scriptsRoot)))
    {
        const QString fbCmd = filtrationScriptPath() + QLatin1Char('|') + args;
        appendAutoDenoiseLog(QStringLiteral("fallback-> ") + fbCmd);

        QProgressDialog fallbackDlg(tr("Fallback denoise (filtration.py)..."), QString(), 0, 0, this);
        fallbackDlg.setCancelButton(nullptr);
        fallbackDlg.setWindowModality(Qt::ApplicationModal);
        fallbackDlg.setMinimumDuration(0);
        fallbackDlg.show();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        QApplication::setOverrideCursor(Qt::WaitCursor);
        bool fbErr = false;
        const bool fbOk = py->executeInterpreter(fbCmd, true, &fbErr);
        QApplication::restoreOverrideCursor();
        fallbackDlg.close();
        appendAutoDenoiseLog(
            QStringLiteral("fallback slice ok=") + QString::number(fbOk)
            + QStringLiteral(" pyErrFlag=") + QString::number(fbErr));
        finalOk = fbOk;
        finalErr = fbErr;
    }

    appendAutoDenoiseLog(
        QStringLiteral("executeInterpreter slice ok=") + QString::number(finalOk)
        + QStringLiteral(" pyErrFlag=") + QString::number(finalErr));
    appendPyRanProbeLog();
    if (cudaRequested && cudaFallbackDetected(scriptsRoot))
    {
        QMessageBox::warning(
            this,
            tr("CUDA недоступна"),
            tr("Выбрана платформа CUDA, но в текущем окружении t4vis CUDA недоступна.\n"
               "Инференс выполнен на CPU.\n"
               "Для GPU нужен CUDA-enabled PyTorch в C:/t4vis."));
    }
}

void DenoiseWidget::startFullVolume()
{
    if (ctrlPanel_->modelCmb->currentIndex() < 0)
        return;
    const QString scriptsRoot = QSettings().value(PTHGroupLine + PTHPythonSystemPath).toString().trimmed();
    if (scriptsRoot.isEmpty())
    {
        QMessageBox::warning(
            this,
            tr("Денойзинг"),
            tr("Не задан путь к каталогу Python Scripts (настройки PTH)."));
        return;
    }

    if (!volSrc_ || !volSrc_->isLoaded())
        return;

    QString dir = ctrlPanel_->outDirEdt->text();
    if (QFileInfo(dir).isRelative())
        dir = QFileInfo(volSrc_->fileName()).absolutePath() + QLatin1Char('/') + dir;

    if (QDir(dir).entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).count() != 0)
    {
        if (QMessageBox::question(this,
                                  tr("Подтверждение"),
                                  tr("Папка для сохранения содержит файлы. Очистить и продолжить?"))
            != QMessageBox::Yes)
            return;
        QDir(dir).removeRecursively();
    }
    QDir().mkpath(dir);

    const QString progressPath = pathArgForPython(
        QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + QStringLiteral("/t4_denoise_progress.txt")));
    const QString args = buildPythonArgs(-1, pathArgForPython(dir), progressPath);
    const QString cmd = buildInterpreterCommand(args);
    const QString probePath = wrapperProbePath(scriptsRoot);
    const QDateTime probeBefore = QFileInfo(probePath).exists() ? QFileInfo(probePath).lastModified() : QDateTime();
    const bool cudaRequested =
        ctrlPanel_->deviceCmb->currentText().compare(QStringLiteral("CUDA"), Qt::CaseInsensitive) == 0;
    appendAutoDenoiseLog(QStringLiteral("startFullVolume cmd=") + cmd);

    IPyThread* py = appData_->pyThread();
    if (!py)
        return;
    appendAutoDenoiseLog(
        QStringLiteral("executeInterpreter volume onGuiThread=")
        + QString::number(QThread::currentThread() == QCoreApplication::instance()->thread()));

    QProgressDialog progressDlg(tr("Денойзинг объёма..."), tr("Отмена"), 0, 100, this);
    progressDlg.setWindowModality(Qt::ApplicationModal);
    progressDlg.setMinimumDuration(0);
    progressDlg.setValue(0);
    progressDlg.show();

    QFile::remove(progressPath);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool pyErr = false;
    const bool started = py->executeInterpreter(cmd, false, &pyErr);
    bool ok = started;
    while (started && py->state() != PyEnums::Finished)
    {
        if (progressDlg.wasCanceled())
        {
            py->stopInterpreter_();
            ok = false;
            appendAutoDenoiseLog(QStringLiteral("volume cancelled by user"));
            break;
        }
        const int p = readProgressPercent(progressPath);
        if (p >= 0)
            progressDlg.setValue(p);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(80);
    }
    if (ok)
        progressDlg.setValue(100);
    QApplication::restoreOverrideCursor();

    const bool primaryRan = probeFileUpdated(probePath, probeBefore);
    appendAutoDenoiseLog(QStringLiteral("wrapperProbeUpdated=") + QString::number(primaryRan));
    bool finalOk = ok;
    bool finalErr = pyErr;
    if (!primaryRan && (allowFiltrationFallback() || filtrationHasDenoiseHook(scriptsRoot)))
    {
        const QString fbArgs = buildPythonArgs(-1, pathArgForPython(dir), QString());
        const QString fbCmd = filtrationScriptPath() + QLatin1Char('|') + fbArgs;
        appendAutoDenoiseLog(QStringLiteral("fallback-> ") + fbCmd);
        QProgressDialog fallbackDlg(tr("Fallback filtration.py..."), QString(), 0, 0, this);
        fallbackDlg.setCancelButton(nullptr);
        fallbackDlg.setWindowModality(Qt::ApplicationModal);
        fallbackDlg.setMinimumDuration(0);
        fallbackDlg.show();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        QApplication::setOverrideCursor(Qt::WaitCursor);
        bool fbErr = false;
        const bool fbOk = py->executeInterpreter(fbCmd, true, &fbErr);
        QApplication::restoreOverrideCursor();
        fallbackDlg.close();
        appendAutoDenoiseLog(
            QStringLiteral("fallback volume ok=") + QString::number(fbOk)
            + QStringLiteral(" pyErrFlag=") + QString::number(fbErr));
        finalOk = fbOk;
        finalErr = fbErr;
    }

    appendAutoDenoiseLog(
        QStringLiteral("executeInterpreter volume ok=") + QString::number(finalOk)
        + QStringLiteral(" pyErrFlag=") + QString::number(finalErr));
    appendPyRanProbeLog();
    if (cudaRequested && cudaFallbackDetected(scriptsRoot))
    {
        QMessageBox::warning(
            this,
            tr("CUDA недоступна"),
            tr("Выбрана платформа CUDA, но в текущем окружении t4vis CUDA недоступна.\n"
               "Инференс объёма выполнен на CPU.\n"
               "Для GPU нужен CUDA-enabled PyTorch в C:/t4vis."));
    }
}

void DenoiseWidget::setHostImage(SharedImageDataType img)
{
    setData(img);
}

void DenoiseWidget::setData(SharedImageDataType img)
{
    if (!img)
        return;
    img->initMinMax();
    ui_->YFrameFiltered->setFrame(img, false);
}

void DenoiseWidget::finalize()
{
    leaving_ = true;
    if (volSrc_)
        volSrc_->setFiltering(-1);
}

void DenoiseWidget::fit()
{
    ui_->YFrame->fit();
    ui_->YFrameFiltered->fit();
    ui_->YFrame->fit();
    ui_->YFrameFiltered->fit();
}
