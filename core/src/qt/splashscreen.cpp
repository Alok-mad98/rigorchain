// Copyright (c) 2011-2022 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/rigor-config.h>
#endif

#include <qt/splashscreen.h>

#include <clientversion.h>
#include <qt/rigorwallet/walletidentity.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <qt/guiutil.h>
#include <qt/networkstyle.h>
#include <qt/walletmodel.h>
#include <util/system.h>
#include <util/translation.h>

#include <functional>

#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QFont>
#include <QPainter>
#include <QRadialGradient>
#include <QScreen>

namespace {
/** Same pale gold as the splash ring / inscription (~#E3C58B). */
const QColor SPLASH_STATUS_GOLD(0xE3, 0xC5, 0x8B);

/** Bottom status line (~2× app default point size); weight stays Normal. */
QFont SplashScreenStatusFont()
{
    QFont f = QApplication::font();
    if (f.pointSizeF() > 0.0) {
        f.setPointSizeF(f.pointSizeF() * 2.0);
    } else if (f.pixelSize() > 0) {
        f.setPixelSize(f.pixelSize() * 2);
    } else {
        f.setPointSizeF(18.0);
    }
    f.setWeight(QFont::Normal);
    return f;
}
} // namespace

SplashScreen::SplashScreen(const NetworkStyle* networkStyle)
    : QWidget()
{
    // set reference point, paddings
    int paddingRight            = 50;
    int paddingTop              = 50;
    int titleVersionVSpace      = 17;
    int titleCopyrightVSpace    = 40;

    float fontFactor            = 1.0;
    float devicePixelRatio      = 1.0;
    devicePixelRatio = static_cast<QGuiApplication*>(QCoreApplication::instance())->devicePixelRatio();

    // define text to place
    QString titleText       = RigorWallet::WalletIdentity::brand().productName;
    QString versionText     = QString("Version %1").arg(QString::fromStdString(FormatFullVersion()));
    QString copyrightText = QString::fromUtf8(
        "\xc2\xa9 2026 The Rigor Core developers.");
    const QString& titleAddText    = networkStyle->getTitleAddText();

    QString font            = QApplication::font().toString();

    // Prefer a custom splash image resource. Fallback to legacy painted splash.
    QPixmap custom_splash(QStringLiteral(":/images/splash-screen"));
    if (!custom_splash.isNull()) {
        pixmap = custom_splash;
        pixmap.setDevicePixelRatio(devicePixelRatio);
    } else {
        // create a bitmap according to device pixelratio
        QSize splashSize(480*devicePixelRatio,320*devicePixelRatio);
        pixmap = QPixmap(splashSize);

        // change to HiDPI if it makes sense
        pixmap.setDevicePixelRatio(devicePixelRatio);

        QPainter fallbackPaint(&pixmap);
        fallbackPaint.setPen(QColor(100,100,100));

        // draw a slightly radial gradient
        QRadialGradient gradient(QPoint(0,0), splashSize.width()/devicePixelRatio);
        gradient.setColorAt(0, Qt::white);
        gradient.setColorAt(1, QColor(247,247,247));
        QRect rGradient(QPoint(0,0), splashSize);
        fallbackPaint.fillRect(rGradient, gradient);

        // draw the rigor icon, expected size of PNG: 1024x1024
        QRect rectIcon(QPoint(-150,-122), QSize(430,430));
        const QSize requiredSize(1024,1024);
        QPixmap icon(networkStyle->getAppIcon().pixmap(requiredSize));
        fallbackPaint.drawPixmap(rectIcon, icon);
        fallbackPaint.end();
    }

    QPainter pixPaint(&pixmap);
    pixPaint.setPen(QColor(100,100,100));

    // check font size and drawing with
    pixPaint.setFont(QFont(font, 33*fontFactor));
    QFontMetrics fm = pixPaint.fontMetrics();
    int titleTextWidth = GUIUtil::TextWidth(fm, titleText);
    if (titleTextWidth > 176) {
        fontFactor = fontFactor * 176 / titleTextWidth;
    }

    pixPaint.setFont(QFont(font, 33*fontFactor));
    fm = pixPaint.fontMetrics();
    titleTextWidth  = GUIUtil::TextWidth(fm, titleText);
    pixPaint.drawText(pixmap.width()/devicePixelRatio-titleTextWidth-paddingRight,paddingTop,titleText);

    pixPaint.setFont(QFont(font, 15*fontFactor));

    // if the version string is too long, reduce size
    fm = pixPaint.fontMetrics();
    int versionTextWidth  = GUIUtil::TextWidth(fm, versionText);
    if(versionTextWidth > titleTextWidth+paddingRight-10) {
        pixPaint.setFont(QFont(font, 10*fontFactor));
        titleVersionVSpace -= 5;
    }
    pixPaint.drawText(pixmap.width()/devicePixelRatio-titleTextWidth-paddingRight+2,paddingTop+titleVersionVSpace,versionText);

    // draw copyright stuff
    {
        pixPaint.setFont(QFont(font, 10*fontFactor));
        const int x = pixmap.width()/devicePixelRatio-titleTextWidth-paddingRight;
        const int y = paddingTop+titleCopyrightVSpace;
        QRect copyrightRect(x, y, pixmap.width() - x - paddingRight, pixmap.height() - y);
        pixPaint.drawText(copyrightRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, copyrightText);
    }

    // draw additional text if special network
    if(!titleAddText.isEmpty()) {
        QFont boldFont = QFont(font, 10*fontFactor);
        boldFont.setWeight(QFont::Bold);
        pixPaint.setFont(boldFont);
        fm = pixPaint.fontMetrics();
        int titleAddTextWidth  = GUIUtil::TextWidth(fm, titleAddText);
        pixPaint.drawText(pixmap.width()/devicePixelRatio-titleAddTextWidth-10,15,titleAddText);
    }

    pixPaint.end();

    // Set window title
    setWindowTitle(titleText + " " + titleAddText);

    // Resize window and move to center of desktop, disallow resizing
    QRect r(QPoint(), QSize(pixmap.size().width()/devicePixelRatio,pixmap.size().height()/devicePixelRatio));
    resize(r.size());
    setFixedSize(r.size());
    // Splash content is a fixed pixmap; do not allow maximize (empty margins when enlarged).
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    move(QGuiApplication::primaryScreen()->geometry().center() - r.center());

    installEventFilter(this);

    GUIUtil::handleCloseWindowShortcut(this);
}

SplashScreen::~SplashScreen()
{
    if (m_node) unsubscribeFromCoreSignals();
}

void SplashScreen::setNode(interfaces::Node& node)
{
    assert(!m_node);
    m_node = &node;
    subscribeToCoreSignals();
    if (m_shutdown) m_node->startShutdown();
}

void SplashScreen::shutdown()
{
    m_shutdown = true;
    if (m_node) m_node->startShutdown();
}

void SplashScreen::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange && isMaximized()) {
        // Title-bar double-click / platform quirks can still maximize without the button.
        showNormal();
    }
    QWidget::changeEvent(event);
}

bool SplashScreen::eventFilter(QObject * obj, QEvent * ev) {
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(ev);
        if (keyEvent->key() == Qt::Key_Q) {
            shutdown();
        }
    }
    return QObject::eventFilter(obj, ev);
}

static void InitMessage(SplashScreen *splash, const std::string &message)
{
    bool invoked = QMetaObject::invokeMethod(splash, "showMessage",
        Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(int, Qt::AlignBottom|Qt::AlignHCenter),
        Q_ARG(QColor, SPLASH_STATUS_GOLD));
    assert(invoked);
}

static void ShowProgress(SplashScreen *splash, const std::string &title, int nProgress, bool resume_possible)
{
    InitMessage(splash, title + std::string("\n") +
            (resume_possible ? SplashScreen::tr("(press q to shutdown and continue later)").toStdString()
                                : SplashScreen::tr("press q to shutdown").toStdString()) +
            strprintf("\n%d", nProgress) + "%");
}

void SplashScreen::subscribeToCoreSignals()
{
    // Connect signals to client
    m_handler_init_message = m_node->handleInitMessage(std::bind(InitMessage, this, std::placeholders::_1));
    m_handler_show_progress = m_node->handleShowProgress(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    m_handler_init_wallet = m_node->handleInitWallet([this]() { handleLoadWallet(); });
}

void SplashScreen::handleLoadWallet()
{
#ifdef ENABLE_WALLET
    if (!WalletModel::isWalletEnabled()) return;
    m_handler_load_wallet = m_node->walletLoader().handleLoadWallet([this](std::unique_ptr<interfaces::Wallet> wallet) {
        m_connected_wallet_handlers.emplace_back(wallet->handleShowProgress(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2, false)));
        m_connected_wallets.emplace_back(std::move(wallet));
    });
#endif
}

void SplashScreen::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    m_handler_init_message->disconnect();
    m_handler_show_progress->disconnect();
    for (const auto& handler : m_connected_wallet_handlers) {
        handler->disconnect();
    }
    m_connected_wallet_handlers.clear();
    m_connected_wallets.clear();
}

void SplashScreen::showMessage(const QString &message, int alignment, const QColor &color)
{
    curMessage = message;
    curAlignment = alignment;
    curColor = color;
    update();
}

void SplashScreen::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.drawPixmap(0, 0, pixmap);
    QRect r = rect().adjusted(5, 5, -5, -5);
    painter.setFont(SplashScreenStatusFont());
    painter.setPen(curColor);
    painter.drawText(r, curAlignment, curMessage);
}

void SplashScreen::closeEvent(QCloseEvent *event)
{
    shutdown(); // allows an "emergency" shutdown during startup
    event->ignore();
}
