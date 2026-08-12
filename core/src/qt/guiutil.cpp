// Copyright (c) 2011-2022 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/guiutil.h>

#include <qt/bitcoinaddressvalidator.h>
#include <qt/bitcoinunits.h>
#include <qt/platformstyle.h>
#include <qt/qvalidatedlineedit.h>
#include <qt/sendcoinsrecipient.h>

#include <wallet/asset_domain.h>

#include <base58.h>
#include <chainparams.h>
#include <interfaces/node.h>
#include <key_io.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <protocol.h>
#include <script/script.h>
#include <script/standard.h>
#include <util/exception.h>
#include <util/fs.h>
#include <util/fs_helpers.h>
#include <util/system.h>
#include <util/time.h>

#ifdef WIN32
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#endif

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QMainWindow>
#include <QPalette>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLatin1String>
#include <QLineEdit>
#include <QList>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPluginLoader>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QSize>
#include <QStandardPaths>
#include <QString>
#include <QTextDocument> // for Qt::mightBeRichText
#include <QThread>
#include <QUrlQuery>
#include <QtGlobal>

#include <cassert>
#include <chrono>
#include <exception>
#include <fstream>
#include <string>
#include <variant>
#include <vector>

#if defined(Q_OS_MACOS)

#include <QProcess>

void ForceActivation();
#endif

using namespace std::chrono_literals;

namespace GUIUtil {

QString dateTimeStr(const QDateTime &date)
{
    return QLocale::system().toString(date.date(), QLocale::ShortFormat) + QString(" ") + date.toString("hh:mm");
}

QString dateTimeStr(qint64 nTime)
{
    return dateTimeStr(QDateTime::fromSecsSinceEpoch(nTime));
}

QFont fixedPitchFont(bool use_embedded_font)
{
    if (use_embedded_font) {
        return {"Roboto Mono"};
    }
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

// Just some dummy data to generate a convincing random-looking (but consistent) address
static const uint8_t dummydata[] = {0xeb,0x15,0x23,0x1d,0xfc,0xeb,0x60,0x92,0x58,0x86,0xb6,0x7d,0x06,0x52,0x99,0x92,0x59,0x15,0xae,0xb1,0x72,0xc0,0x66,0x47};

/** Example native address for UI placeholders (adapts to chain Bech32 HRP). */
static std::string DummyNativeAddress(AssetType asset)
{
    if (asset == ASSET_RES) {
        unsigned char data[32]{};
        for (size_t i = 0; i < sizeof(data); ++i) {
            data[i] = dummydata[i % sizeof(dummydata)] ^ static_cast<uint8_t>(i * 17);
        }
        return EncodeDestination(WitnessV1Res{XOnlyPubKey{Span<const unsigned char>{data, 32}}});
    }
    uint160 id;
    std::copy(dummydata, dummydata + 20, id.begin());
    return EncodeDestination(WitnessV0KeyHash{id});
}

void updateAddressWidgetPlaceholder(QValidatedLineEdit* widget, AssetType asset)
{
    if (!widget) return;
    const QString example = QString::fromStdString(DummyNativeAddress(asset));
    if (asset == ASSET_RES) {
        widget->setPlaceholderText(QObject::tr("Enter a RES address (e.g. %1)").arg(example));
    } else {
        widget->setPlaceholderText(QObject::tr("Enter an ORU address (e.g. %1)").arg(example));
    }
}

void setupAddressWidget(QValidatedLineEdit *widget, QWidget *parent)
{
    parent->setFocusProxy(widget);

    widget->setFont(fixedPitchFont());
    // Default to ORU / Bech32; Send tab updates via updateAddressWidgetPlaceholder when Asset changes.
    updateAddressWidgetPlaceholder(widget, ASSET_ORU);
    widget->setValidator(new BitcoinAddressEntryValidator(parent));
    widget->setCheckValidator(new BitcoinAddressCheckValidator(parent));
}

void AddButtonShortcut(QAbstractButton* button, const QKeySequence& shortcut)
{
    QObject::connect(new QShortcut(shortcut, button), &QShortcut::activated, [button]() { button->animateClick(); });
}

bool parseBitcoinURI(const QUrl &uri, SendCoinsRecipient *out)
{
    // return if URI is not valid or is no rigor: URI
    if(!uri.isValid() || uri.scheme() != QString("rigor"))
        return false;

    SendCoinsRecipient rv;
    rv.address = uri.path();
    // Trim any following forward slash which may have been added by the OS
    if (rv.address.endsWith("/")) {
        rv.address.truncate(rv.address.length() - 1);
    }
    rv.amount = 0;

    QString amount_param;
    bool have_asset = false;
    AssetType uri_asset = ASSET_ORU;

    QUrlQuery uriQuery(uri);
    const QList<QPair<QString, QString>> items = uriQuery.queryItems();
    for (QList<QPair<QString, QString>>::const_iterator i = items.begin(); i != items.end(); ++i)
    {
        QString key = i->first;
        bool unknown_req = false;
        if (key.startsWith(QStringLiteral("req-")))
        {
            key.remove(0, 4);
            unknown_req = true;
        }

        if (key == QStringLiteral("label"))
        {
            rv.label = i->second;
            unknown_req = false;
        }
        else if (key == QStringLiteral("message"))
        {
            rv.message = i->second;
            unknown_req = false;
        }
        else if (key == QStringLiteral("amount"))
        {
            amount_param = i->second;
            unknown_req = false;
        }
        else if (key == QStringLiteral("asset"))
        {
            const QString v = i->second.trimmed().toLower();
            AssetType parsed = ASSET_ORU;
            if (v == QStringLiteral("oru")) {
                parsed = ASSET_ORU;
            } else if (v == QStringLiteral("res")) {
                parsed = ASSET_RES;
            } else {
                return false;
            }
            if (have_asset && uri_asset != parsed) {
                return false;
            }
            uri_asset = parsed;
            have_asset = true;
            unknown_req = false;
        }

        if (unknown_req)
            return false;
    }

    // W1.1D FREEZE INVARIANT (L6 GUI URI parser, F-A-09): rigor: URIs MUST carry an
    // explicit asset= query parameter. The GUI URI layer is the strictest UX surface
    // (manual user paste is the highest-risk human-error vector) and intentionally
    // forbids the ORU-default fallback that RPC callers may use. Do NOT add a default
    // here without a new constitutional review.
    if (!have_asset) {
        return false;
    }

    std::string dec_err;
    const CTxDestination uri_dest = DecodeDestination(rv.address.toStdString(), dec_err, nullptr);
    if (!IsValidDestination(uri_dest)) {
        return false;
    }
    const CScript uri_spk = GetScriptForDestination(uri_dest);
    if (!wallet::WalletStrictSendDestinationOk(uri_spk, uri_asset)) {
        return false;
    }

    if (!amount_param.isEmpty())
    {
        if (uri_asset == ASSET_RES) {
            if (!BitcoinUnits::parseRes(amount_param, &rv.amount)) {
                return false;
            }
        } else {
            if (!BitcoinUnits::parse(BitcoinUnit::ORU, amount_param, &rv.amount)) {
                return false;
            }
        }
    }

    rv.asset = uri_asset;

    if(out)
    {
        *out = rv;
    }
    return true;
}

bool parseBitcoinURI(QString uri, SendCoinsRecipient *out)
{
    QUrl uriInstance(uri);
    return parseBitcoinURI(uriInstance, out);
}

QString formatBitcoinURI(const SendCoinsRecipient &info)
{
    bool bech_32 = info.address.startsWith(QString::fromStdString(Params().Bech32HRP() + "1"));

    QString ret = QString("rigor:%1").arg(bech_32 ? info.address.toUpper() : info.address);

    AssetType asset_dom = ASSET_ORU;
    {
        std::string dec_err;
        const CTxDestination d = DecodeDestination(info.address.toStdString(), dec_err, nullptr);
        if (IsValidDestination(d)) {
            asset_dom = AssetTypeFromScriptPubKey(GetScriptForDestination(d));
        } else if (info.asset.has_value()) {
            asset_dom = *info.asset;
        }
    }

    ret += QStringLiteral("?asset=%1").arg(asset_dom == ASSET_RES ? QStringLiteral("res") : QStringLiteral("oru"));

    if (info.amount)
    {
        if (asset_dom == ASSET_RES) {
            ret += QStringLiteral("&amount=%1").arg(BitcoinUnits::formatRes(info.amount, false, BitcoinUnits::SeparatorStyle::NEVER));
        } else {
            ret += QStringLiteral("&amount=%1").arg(BitcoinUnits::format(BitcoinUnit::ORU, info.amount, false, BitcoinUnits::SeparatorStyle::NEVER));
        }
    }

    if (!info.label.isEmpty())
    {
        QString lbl(QUrl::toPercentEncoding(info.label));
        ret += QStringLiteral("&label=%1").arg(lbl);
    }

    if (!info.message.isEmpty())
    {
        QString msg(QUrl::toPercentEncoding(info.message));
        ret += QStringLiteral("&message=%1").arg(msg);
    }

    return ret;
}

QString FormatRecipientAssetDomainSummary(const QString& address)
{
    std::string dec_err;
    const CTxDestination dest = DecodeDestination(address.toStdString(), dec_err, nullptr);
    if (!IsValidDestination(dest)) {
        return QObject::tr("(invalid address)");
    }
    const CScript spk = GetScriptForDestination(dest);
    const AssetType at = AssetTypeFromScriptPubKey(spk);

    QString inferred;
    switch (at) {
    case ASSET_ORU: inferred = QObject::tr("ORU-domain"); break;
    case ASSET_RES: inferred = QObject::tr("RES"); break;
    default: inferred = QObject::tr("none / data"); break;
    }

    int witness_ver = -1;
    QString encoding = QObject::tr("Base58 (non-segwit)");
    int wv = 0;
    std::vector<unsigned char> wp;
    if (spk.IsWitnessProgram(wv, wp)) {
        witness_ver = wv;
        if (wv == 0) {
            encoding = QObject::tr("Bech32");
        } else if (wv == 1) {
            encoding = QObject::tr("Bech32m");
        } else {
            encoding = QObject::tr("Segwit (other)");
        }
    }

    const QString wv_text = (witness_ver >= 0) ? QString::number(witness_ver) : QObject::tr("n/a");
    return QObject::tr("Inferred type: %1; witness version: %2; encoding: %3")
        .arg(inferred, wv_text, encoding);
}

bool isDust(interfaces::Node& node, const QString& address, const CAmount& amount)
{
    CTxDestination dest = DecodeDestination(address.toStdString());
    CScript script = GetScriptForDestination(dest);
    CTxOut txOut(amount, script);
    return IsDust(txOut, node.getDustRelayFee());
}

QString HtmlEscape(const QString& str, bool fMultiLine)
{
    QString escaped = str.toHtmlEscaped();
    if(fMultiLine)
    {
        escaped = escaped.replace("\n", "<br>\n");
    }
    return escaped;
}

QString HtmlEscape(const std::string& str, bool fMultiLine)
{
    return HtmlEscape(QString::fromStdString(str), fMultiLine);
}

NativeReceiveAssetKind ClassifyNativeReceiveAddress(const QString& address)
{
    std::string err;
    const CTxDestination dest = DecodeDestination(address.toStdString(), err);
    if (!IsValidDestination(dest)) {
        return NativeReceiveAssetKind::Invalid;
    }
    if (std::holds_alternative<WitnessV0KeyHash>(dest) ||
        std::holds_alternative<WitnessV0ScriptHash>(dest)) {
        return NativeReceiveAssetKind::ORU;
    }
    if (std::holds_alternative<WitnessV1Res>(dest)) {
        return NativeReceiveAssetKind::RES;
    }
    return NativeReceiveAssetKind::Other;
}

void copyEntryData(const QAbstractItemView *view, int column, int role)
{
    if(!view || !view->selectionModel())
        return;
    QModelIndexList selection = view->selectionModel()->selectedRows(column);

    if(!selection.isEmpty())
    {
        // Copy first item
        setClipboard(selection.at(0).data(role).toString());
    }
}

QList<QModelIndex> getEntryData(const QAbstractItemView *view, int column)
{
    if(!view || !view->selectionModel())
        return QList<QModelIndex>();
    return view->selectionModel()->selectedRows(column);
}

bool hasEntryData(const QAbstractItemView *view, int column, int role)
{
    QModelIndexList selection = getEntryData(view, column);
    if (selection.isEmpty()) return false;
    return !selection.at(0).data(role).toString().isEmpty();
}

void LoadFont(const QString& file_name)
{
    const int id = QFontDatabase::addApplicationFont(file_name);
    assert(id != -1);
}

namespace {
QString g_ui_font_family_css;

bool TryLoadApplicationFont(const QString& path)
{
    if (path.isEmpty() || !QFile::exists(path)) {
        return false;
    }
    return QFontDatabase::addApplicationFont(path) != -1;
}

void LoadKnownCjkFonts()
{
    // Prefer system CJK UI fonts so zh_CN (and other CJK) is not tofu boxes.
    // Also try WSL mount of Windows Fonts when running a Linux build under WSL.
    const QStringList candidates = {
#ifdef WIN32
        QStringLiteral("C:/Windows/Fonts/msyh.ttc"),
        QStringLiteral("C:/Windows/Fonts/msyhbd.ttc"),
        QStringLiteral("C:/Windows/Fonts/msyhl.ttc"),
        QStringLiteral("C:/Windows/Fonts/simsun.ttc"),
        QStringLiteral("C:/Windows/Fonts/NotoSansSC-VF.ttf"),
        QStringLiteral("C:/Windows/Fonts/NotoSansSC-Regular.otf"),
        QStringLiteral("C:/Windows/Fonts/NotoSansCJKsc-Regular.otf"),
#else
        QStringLiteral("/mnt/c/Windows/Fonts/msyh.ttc"),
        QStringLiteral("/mnt/c/Windows/Fonts/msyhbd.ttc"),
        QStringLiteral("/mnt/c/Windows/Fonts/msyhl.ttc"),
        QStringLiteral("/mnt/c/Windows/Fonts/simsun.ttc"),
        QStringLiteral("/mnt/c/Windows/Fonts/NotoSansSC-VF.ttf"),
        QStringLiteral("/mnt/c/Windows/Fonts/NotoSansSC-Regular.otf"),
        QStringLiteral("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"),
        QStringLiteral("/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc"),
        QStringLiteral("/usr/share/fonts/opentype/noto/NotoSansCJKsc-Regular.otf"),
        QStringLiteral("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc"),
        QStringLiteral("/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"),
        QStringLiteral("/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf"),
#endif
    };
    for (const QString& path : candidates) {
        TryLoadApplicationFont(path);
    }
}

QStringList PreferAvailableFamilies(const QStringList& preferred)
{
    QStringList out;
    const QStringList available = QFontDatabase().families();
    for (const QString& family : preferred) {
        // Exact match or prefix (variable fonts / localized names).
        if (available.contains(family)) {
            out << family;
            continue;
        }
        for (const QString& avail : available) {
            if (avail.startsWith(family) || family.startsWith(avail)) {
                if (!out.contains(avail)) {
                    out << avail;
                }
                break;
            }
        }
    }
    return out;
}
} // namespace

void SetupUiFonts(const QString& lang_territory)
{
    LoadKnownCjkFonts();

    // Latin UI first for Latin glyphs; CJK fonts next so missing glyphs fall back.
    // When the UI language is Chinese, put a CJK UI face first to avoid tofu boxes
    // on platforms where default Latin fonts do not compose CJK fallbacks reliably.
    const QString lang = lang_territory.isEmpty() ? QLocale().name() : lang_territory;
    const bool prefer_cjk = lang.startsWith(QStringLiteral("zh"), Qt::CaseInsensitive);

    const QStringList cjk_prefer = {
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Microsoft YaHei"),
        QStringLiteral("Noto Sans SC"),
        QStringLiteral("Noto Sans CJK SC"),
        QStringLiteral("Source Han Sans SC"),
        QStringLiteral("WenQuanYi Micro Hei"),
        QStringLiteral("WenQuanYi Zen Hei"),
        QStringLiteral("Droid Sans Fallback"),
        QStringLiteral("PingFang SC"),
        QStringLiteral("Hiragino Sans GB"),
        QStringLiteral("SimHei"),
        QStringLiteral("SimSun"),
    };
    const QStringList latin_prefer = {
        QStringLiteral("Segoe UI"),
        QStringLiteral("Ubuntu"),
        QStringLiteral("Cantarell"),
        QStringLiteral("Noto Sans"),
        QStringLiteral("DejaVu Sans"),
        QStringLiteral("Sans Serif"),
    };

    QStringList families;
    const QStringList cjk_avail = PreferAvailableFamilies(cjk_prefer);
    const QStringList latin_avail = PreferAvailableFamilies(latin_prefer);
    if (prefer_cjk) {
        families << cjk_avail << latin_avail;
    } else {
        families << latin_avail << cjk_avail;
    }

    QFont app_font = QApplication::font();
    if (!families.contains(app_font.family())) {
        families << app_font.family();
    }
#if (QT_VERSION >= QT_VERSION_CHECK(5, 13, 0))
    if (!families.isEmpty()) {
        app_font.setFamilies(families);
    }
#else
    if (!families.isEmpty()) {
        app_font.setFamily(families.first());
    }
#endif
    QApplication::setFont(app_font);

    // Stylesheets that hardcode Segoe UI / Roboto need an explicit CJK substitute.
    if (!cjk_avail.isEmpty()) {
        const QString cjk = cjk_avail.first();
        for (const QString& latin : {QStringLiteral("Segoe UI"), QStringLiteral("Roboto"),
                                    QStringLiteral("Inter"), QStringLiteral("Ubuntu"),
                                    QStringLiteral("Cantarell"), QStringLiteral("Sans Serif")}) {
            QFont::insertSubstitution(latin, cjk);
        }
    }

    QStringList css_quoted;
    for (const QString& family : families) {
        css_quoted << QStringLiteral("\"%1\"").arg(family);
    }
    css_quoted << QStringLiteral("sans-serif");
    g_ui_font_family_css = css_quoted.join(QStringLiteral(", "));
}

QString UiFontFamilyCss()
{
    if (g_ui_font_family_css.isEmpty()) {
        return QStringLiteral(
            "\"Segoe UI\", \"Microsoft YaHei UI\", \"Microsoft YaHei\", "
            "\"Noto Sans SC\", \"Noto Sans CJK SC\", \"WenQuanYi Micro Hei\", sans-serif");
    }
    return g_ui_font_family_css;
}

QString getDefaultDataDirectory()
{
    return PathToQString(GetDefaultDataDir());
}

QString ExtractFirstSuffixFromFilter(const QString& filter)
{
    QRegularExpression filter_re(QStringLiteral(".* \\(\\*\\.(.*)[ \\)]"), QRegularExpression::InvertedGreedinessOption);
    QString suffix;
    QRegularExpressionMatch m = filter_re.match(filter);
    if (m.hasMatch()) {
        suffix = m.captured(1);
    }
    return suffix;
}

QString getSaveFileName(QWidget *parent, const QString &caption, const QString &dir,
    const QString &filter,
    QString *selectedSuffixOut)
{
    QString selectedFilter;
    QString myDir;
    if(dir.isEmpty()) // Default to user documents location
    {
        myDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    else
    {
        myDir = dir;
    }
    /* Directly convert path to native OS path separators */
    QString result = QDir::toNativeSeparators(QFileDialog::getSaveFileName(parent, caption, myDir, filter, &selectedFilter));

    QString selectedSuffix = ExtractFirstSuffixFromFilter(selectedFilter);

    /* Add suffix if needed */
    QFileInfo info(result);
    if(!result.isEmpty())
    {
        if(info.suffix().isEmpty() && !selectedSuffix.isEmpty())
        {
            /* No suffix specified, add selected suffix */
            if(!result.endsWith("."))
                result.append(".");
            result.append(selectedSuffix);
        }
    }

    /* Return selected suffix if asked to */
    if(selectedSuffixOut)
    {
        *selectedSuffixOut = selectedSuffix;
    }
    return result;
}

QString getOpenFileName(QWidget *parent, const QString &caption, const QString &dir,
    const QString &filter,
    QString *selectedSuffixOut)
{
    QString selectedFilter;
    QString myDir;
    if(dir.isEmpty()) // Default to user documents location
    {
        myDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    else
    {
        myDir = dir;
    }
    /* Directly convert path to native OS path separators */
    QString result = QDir::toNativeSeparators(QFileDialog::getOpenFileName(parent, caption, myDir, filter, &selectedFilter));

    if(selectedSuffixOut)
    {
        *selectedSuffixOut = ExtractFirstSuffixFromFilter(selectedFilter);
        ;
    }
    return result;
}

Qt::ConnectionType blockingGUIThreadConnection()
{
    if(QThread::currentThread() != qApp->thread())
    {
        return Qt::BlockingQueuedConnection;
    }
    else
    {
        return Qt::DirectConnection;
    }
}

bool checkPoint(const QPoint &p, const QWidget *w)
{
    QWidget *atW = QApplication::widgetAt(w->mapToGlobal(p));
    if (!atW) return false;
    return atW->window() == w;
}

bool isObscured(QWidget *w)
{
    return !(checkPoint(QPoint(0, 0), w)
        && checkPoint(QPoint(w->width() - 1, 0), w)
        && checkPoint(QPoint(0, w->height() - 1), w)
        && checkPoint(QPoint(w->width() - 1, w->height() - 1), w)
        && checkPoint(QPoint(w->width() / 2, w->height() / 2), w));
}

void bringToFront(QWidget* w)
{
#ifdef Q_OS_MACOS
    ForceActivation();
#endif

    if (w) {
        // activateWindow() (sometimes) helps with keyboard focus on Windows
        if (w->isMinimized()) {
            w->showNormal();
        } else {
            w->show();
        }
        w->activateWindow();
        w->raise();
    }
}

void handleCloseWindowShortcut(QWidget* w)
{
    QObject::connect(new QShortcut(QKeySequence(QObject::tr("Ctrl+W")), w), &QShortcut::activated, w, &QWidget::close);
}

void openDebugLogfile()
{
    fs::path pathDebug = gArgs.GetDataDirNet() / "debug.log";

    /* Open debug.log with the associated application */
    if (fs::exists(pathDebug))
        QDesktopServices::openUrl(QUrl::fromLocalFile(PathToQString(pathDebug)));
}

bool openBitcoinConf()
{
    fs::path pathConfig = gArgs.GetConfigFilePath();

    /* Create the file */
    std::ofstream configFile{pathConfig, std::ios_base::app};

    if (!configFile.good())
        return false;

    configFile.close();

    /* Open rigor.conf with the associated application */
    bool res = QDesktopServices::openUrl(QUrl::fromLocalFile(PathToQString(pathConfig)));
#ifdef Q_OS_MACOS
    // Workaround for macOS-specific behavior; see #15409.
    if (!res) {
        res = QProcess::startDetached("/usr/bin/open", QStringList{"-t", PathToQString(pathConfig)});
    }
#endif

    return res;
}

ToolTipToRichTextFilter::ToolTipToRichTextFilter(int _size_threshold, QObject *parent) :
    QObject(parent),
    size_threshold(_size_threshold)
{

}

bool ToolTipToRichTextFilter::eventFilter(QObject *obj, QEvent *evt)
{
    if(evt->type() == QEvent::ToolTipChange)
    {
        QWidget *widget = static_cast<QWidget*>(obj);
        QString tooltip = widget->toolTip();
        if(tooltip.size() > size_threshold && !tooltip.startsWith("<qt") && !Qt::mightBeRichText(tooltip))
        {
            // Envelop with <qt></qt> to make sure Qt detects this as rich text
            // Escape the current message as HTML and replace \n by <br>
            tooltip = "<qt>" + HtmlEscape(tooltip, true) + "</qt>";
            widget->setToolTip(tooltip);
            return true;
        }
    }
    return QObject::eventFilter(obj, evt);
}

LabelOutOfFocusEventFilter::LabelOutOfFocusEventFilter(QObject* parent)
    : QObject(parent)
{
}

bool LabelOutOfFocusEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::FocusOut) {
        auto focus_out = static_cast<QFocusEvent*>(event);
        if (focus_out->reason() != Qt::PopupFocusReason) {
            auto label = qobject_cast<QLabel*>(watched);
            if (label) {
                auto flags = label->textInteractionFlags();
                label->setTextInteractionFlags(Qt::NoTextInteraction);
                label->setTextInteractionFlags(flags);
            }
        }
    }

    return QObject::eventFilter(watched, event);
}

#ifdef WIN32
fs::path static StartupShortcutPath()
{
    std::string chain = gArgs.GetChainName();
    if (chain == CBaseChainParams::MAIN)
        return GetSpecialFolderPath(CSIDL_STARTUP) / "Rigor.lnk";
    if (chain == CBaseChainParams::TESTNET) // Remove this special case when CBaseChainParams::TESTNET = "testnet4"
        return GetSpecialFolderPath(CSIDL_STARTUP) / "Rigor (testnet).lnk";
    return GetSpecialFolderPath(CSIDL_STARTUP) / fs::u8path(strprintf("Rigor (%s).lnk", chain));
}

bool GetStartOnSystemStartup()
{
    // check for Rigor*.lnk
    return fs::exists(StartupShortcutPath());
}

bool SetStartOnSystemStartup(bool fAutoStart)
{
    // If the shortcut exists already, remove it for updating
    fs::remove(StartupShortcutPath());

    if (fAutoStart)
    {
        CoInitialize(nullptr);

        // Get a pointer to the IShellLink interface.
        IShellLinkW* psl = nullptr;
        HRESULT hres = CoCreateInstance(CLSID_ShellLink, nullptr,
            CLSCTX_INPROC_SERVER, IID_IShellLinkW,
            reinterpret_cast<void**>(&psl));

        if (SUCCEEDED(hres))
        {
            // Get the current executable path
            WCHAR pszExePath[MAX_PATH];
            GetModuleFileNameW(nullptr, pszExePath, ARRAYSIZE(pszExePath));

            // Start client minimized
            QString strArgs = "-min";
            // Set -testnet /-regtest options
            strArgs += QString::fromStdString(strprintf(" -chain=%s", gArgs.GetChainName()));

            // Set the path to the shortcut target
            psl->SetPath(pszExePath);
            PathRemoveFileSpecW(pszExePath);
            psl->SetWorkingDirectory(pszExePath);
            psl->SetShowCmd(SW_SHOWMINNOACTIVE);
            psl->SetArguments(strArgs.toStdWString().c_str());

            // Query IShellLink for the IPersistFile interface for
            // saving the shortcut in persistent storage.
            IPersistFile* ppf = nullptr;
            hres = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf));
            if (SUCCEEDED(hres))
            {
                // Save the link by calling IPersistFile::Save.
                hres = ppf->Save(StartupShortcutPath().wstring().c_str(), TRUE);
                ppf->Release();
                psl->Release();
                CoUninitialize();
                return true;
            }
            psl->Release();
        }
        CoUninitialize();
        return false;
    }
    return true;
}
#elif defined(Q_OS_LINUX)

// Follow the Desktop Application Autostart Spec:
// https://specifications.freedesktop.org/autostart-spec/autostart-spec-latest.html

fs::path static GetAutostartDir()
{
    char* pszConfigHome = getenv("XDG_CONFIG_HOME");
    if (pszConfigHome) return fs::path(pszConfigHome) / "autostart";
    char* pszHome = getenv("HOME");
    if (pszHome) return fs::path(pszHome) / ".config" / "autostart";
    return fs::path();
}

fs::path static GetAutostartFilePath()
{
    std::string chain = gArgs.GetChainName();
    if (chain == CBaseChainParams::MAIN)
        return GetAutostartDir() / "rigor.desktop";
    return GetAutostartDir() / fs::u8path(strprintf("rigor-%s.desktop", chain));
}

bool GetStartOnSystemStartup()
{
    std::ifstream optionFile{GetAutostartFilePath()};
    if (!optionFile.good())
        return false;
    // Scan through file for "Hidden=true":
    std::string line;
    while (!optionFile.eof())
    {
        getline(optionFile, line);
        if (line.find("Hidden") != std::string::npos &&
            line.find("true") != std::string::npos)
            return false;
    }
    optionFile.close();

    return true;
}

bool SetStartOnSystemStartup(bool fAutoStart)
{
    if (!fAutoStart)
        fs::remove(GetAutostartFilePath());
    else
    {
        char pszExePath[MAX_PATH+1];
        ssize_t r = readlink("/proc/self/exe", pszExePath, sizeof(pszExePath));
        if (r == -1 || r > MAX_PATH) {
            return false;
        }
        pszExePath[r] = '\0';

        fs::create_directories(GetAutostartDir());

        std::ofstream optionFile{GetAutostartFilePath(), std::ios_base::out | std::ios_base::trunc};
        if (!optionFile.good())
            return false;
        std::string chain = gArgs.GetChainName();
        // Write a rigor.desktop file to the autostart directory:
        optionFile << "[Desktop Entry]\n";
        optionFile << "Type=Application\n";
        if (chain == CBaseChainParams::MAIN)
            optionFile << "Name=Rigor\n";
        else
            optionFile << strprintf("Name=Rigor (%s)\n", chain);
        optionFile << "Exec=" << pszExePath << strprintf(" -min -chain=%s\n", chain);
        optionFile << "Terminal=false\n";
        optionFile << "Hidden=false\n";
        optionFile.close();
    }
    return true;
}

#else

bool GetStartOnSystemStartup() { return false; }
bool SetStartOnSystemStartup(bool fAutoStart) { return false; }

#endif

void setClipboard(const QString& str)
{
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(str, QClipboard::Clipboard);
    if (clipboard->supportsSelection()) {
        clipboard->setText(str, QClipboard::Selection);
    }
}

fs::path QStringToPath(const QString &path)
{
    return fs::u8path(path.toStdString());
}

QString PathToQString(const fs::path &path)
{
    return QString::fromStdString(path.u8string());
}

QString NetworkToQString(Network net)
{
    switch (net) {
    case NET_UNROUTABLE: return QObject::tr("Unroutable");
    //: Name of IPv4 network in peer info
    case NET_IPV4: return QObject::tr("IPv4", "network name");
    //: Name of IPv6 network in peer info
    case NET_IPV6: return QObject::tr("IPv6", "network name");
    //: Name of Tor network in peer info
    case NET_ONION: return QObject::tr("Onion", "network name");
    //: Name of I2P network in peer info
    case NET_I2P: return QObject::tr("I2P", "network name");
    //: Name of CJDNS network in peer info
    case NET_CJDNS: return QObject::tr("CJDNS", "network name");
    case NET_INTERNAL: return "Internal";  // should never actually happen
    case NET_MAX: assert(false);
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

QString ConnectionTypeToQString(ConnectionType conn_type, bool prepend_direction)
{
    QString prefix;
    if (prepend_direction) {
        prefix = (conn_type == ConnectionType::INBOUND) ?
                     /*: An inbound connection from a peer. An inbound connection
                         is a connection initiated by a peer. */
                     QObject::tr("Inbound") :
                     /*: An outbound connection to a peer. An outbound connection
                         is a connection initiated by us. */
                     QObject::tr("Outbound") + " ";
    }
    switch (conn_type) {
    case ConnectionType::INBOUND: return prefix;
    //: Peer connection type that relays all network information.
    case ConnectionType::OUTBOUND_FULL_RELAY: return prefix + QObject::tr("Full Relay");
    /*: Peer connection type that relays network information about
        blocks and not transactions or addresses. */
    case ConnectionType::BLOCK_RELAY: return prefix + QObject::tr("Block Relay");
    //: Peer connection type established manually through one of several methods.
    case ConnectionType::MANUAL: return prefix + QObject::tr("Manual");
    //: Short-lived peer connection type that tests the aliveness of known addresses.
    case ConnectionType::FEELER: return prefix + QObject::tr("Feeler");
    //: Short-lived peer connection type that solicits known addresses from a peer.
    case ConnectionType::ADDR_FETCH: return prefix + QObject::tr("Address Fetch");
    } // no default case, so the compiler can warn about missing cases
    assert(false);
    return {}; // silence -Wreturn-type (assert may be compiled out)
}

QString formatDurationStr(std::chrono::seconds dur)
{
    using days = std::chrono::duration<int, std::ratio<86400>>; // can remove this line after C++20
    const auto d{std::chrono::duration_cast<days>(dur)};
    const auto h{std::chrono::duration_cast<std::chrono::hours>(dur - d)};
    const auto m{std::chrono::duration_cast<std::chrono::minutes>(dur - d - h)};
    const auto s{std::chrono::duration_cast<std::chrono::seconds>(dur - d - h - m)};
    QStringList str_list;
    if (auto d2{d.count()}) str_list.append(QObject::tr("%1 d").arg(d2));
    if (auto h2{h.count()}) str_list.append(QObject::tr("%1 h").arg(h2));
    if (auto m2{m.count()}) str_list.append(QObject::tr("%1 m").arg(m2));
    const auto s2{s.count()};
    if (s2 || str_list.empty()) str_list.append(QObject::tr("%1 s").arg(s2));
    return str_list.join(" ");
}

QString FormatPeerAge(std::chrono::seconds time_connected)
{
    const auto time_now{GetTime<std::chrono::seconds>()};
    const auto age{time_now - time_connected};
    if (age >= 24h) return QObject::tr("%1 d").arg(age / 24h);
    if (age >= 1h) return QObject::tr("%1 h").arg(age / 1h);
    if (age >= 1min) return QObject::tr("%1 m").arg(age / 1min);
    return QObject::tr("%1 s").arg(age / 1s);
}

QString formatServicesStr(quint64 mask)
{
    QStringList strList;

    for (const auto& flag : serviceFlagsToStr(mask)) {
        strList.append(QString::fromStdString(flag));
    }

    if (strList.size())
        return strList.join(", ");
    else
        return QObject::tr("None");
}

QString formatPingTime(std::chrono::microseconds ping_time)
{
    return (ping_time == std::chrono::microseconds::max() || ping_time == 0us) ?
        QObject::tr("N/A") :
        QObject::tr("%1 ms").arg(QString::number((int)(count_microseconds(ping_time) / 1000), 10));
}

QString formatTimeOffset(int64_t nTimeOffset)
{
  return QObject::tr("%1 s").arg(QString::number((int)nTimeOffset, 10));
}

QString formatNiceTimeOffset(qint64 secs)
{
    // Represent time from last generated block in human readable text
    QString timeBehindText;
    const int HOUR_IN_SECONDS = 60*60;
    const int DAY_IN_SECONDS = 24*60*60;
    const int WEEK_IN_SECONDS = 7*24*60*60;
    const int YEAR_IN_SECONDS = 31556952; // Average length of year in Gregorian calendar
    if(secs < 60)
    {
        timeBehindText = QObject::tr("%n second(s)","",secs);
    }
    else if(secs < 2*HOUR_IN_SECONDS)
    {
        timeBehindText = QObject::tr("%n minute(s)","",secs/60);
    }
    else if(secs < 2*DAY_IN_SECONDS)
    {
        timeBehindText = QObject::tr("%n hour(s)","",secs/HOUR_IN_SECONDS);
    }
    else if(secs < 2*WEEK_IN_SECONDS)
    {
        timeBehindText = QObject::tr("%n day(s)","",secs/DAY_IN_SECONDS);
    }
    else if(secs < YEAR_IN_SECONDS)
    {
        timeBehindText = QObject::tr("%n week(s)","",secs/WEEK_IN_SECONDS);
    }
    else
    {
        qint64 years = secs / YEAR_IN_SECONDS;
        qint64 remainder = secs % YEAR_IN_SECONDS;
        timeBehindText = QObject::tr("%1 and %2").arg(QObject::tr("%n year(s)", "", years)).arg(QObject::tr("%n week(s)","", remainder/WEEK_IN_SECONDS));
    }
    return timeBehindText;
}

QString formatBytes(uint64_t bytes)
{
    if (bytes < 1'000)
        return QObject::tr("%1 B").arg(bytes);
    if (bytes < 1'000'000)
        return QObject::tr("%1 kB").arg(bytes / 1'000);
    if (bytes < 1'000'000'000)
        return QObject::tr("%1 MB").arg(bytes / 1'000'000);

    return QObject::tr("%1 GB").arg(bytes / 1'000'000'000);
}

qreal calculateIdealFontSize(int width, const QString& text, QFont font, qreal minPointSize, qreal font_size) {
    while(font_size >= minPointSize) {
        font.setPointSizeF(font_size);
        QFontMetrics fm(font);
        if (TextWidth(fm, text) < width) {
            break;
        }
        font_size -= 0.5;
    }
    return font_size;
}

ThemedLabel::ThemedLabel(const PlatformStyle* platform_style, QWidget* parent)
    : QLabel{parent}, m_platform_style{platform_style}
{
    assert(m_platform_style);
}

void ThemedLabel::setThemedPixmap(const QString& image_filename, int width, int height)
{
    m_image_filename = image_filename;
    m_pixmap_width = width;
    m_pixmap_height = height;
    updateThemedPixmap();
}

void ThemedLabel::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::PaletteChange) {
        updateThemedPixmap();
    }

    QLabel::changeEvent(e);
}

void ThemedLabel::updateThemedPixmap()
{
    setPixmap(m_platform_style->SingleColorIcon(m_image_filename).pixmap(m_pixmap_width, m_pixmap_height));
}

ClickableLabel::ClickableLabel(const PlatformStyle* platform_style, QWidget* parent)
    : ThemedLabel{platform_style, parent}
{
}

void ClickableLabel::mouseReleaseEvent(QMouseEvent *event)
{
    Q_EMIT clicked(event->pos());
}

void ClickableProgressBar::mouseReleaseEvent(QMouseEvent *event)
{
    Q_EMIT clicked(event->pos());
}

bool ItemDelegate::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            Q_EMIT keyEscapePressed();
        }
    }
    return QItemDelegate::eventFilter(object, event);
}

void PolishProgressDialog(QProgressDialog* dialog)
{
#ifdef Q_OS_MACOS
    // Workaround for macOS-only Qt bug; see: QTBUG-65750, QTBUG-70357.
    const int margin = TextWidth(dialog->fontMetrics(), ("X"));
    dialog->resize(dialog->width() + 2 * margin, dialog->height());
#endif
    // QProgressDialog estimates the time the operation will take (based on time
    // for steps), and only shows itself if that estimate is beyond minimumDuration.
    // The default minimumDuration value is 4 seconds, and it could make users
    // think that the GUI is frozen.
    dialog->setMinimumDuration(0);
}

int TextWidth(const QFontMetrics& fm, const QString& text)
{
    return fm.horizontalAdvance(text);
}

void LogQtInfo()
{
#ifdef QT_STATIC
    const std::string qt_link{"static"};
#else
    const std::string qt_link{"dynamic"};
#endif
#ifdef QT_STATICPLUGIN
    const std::string plugin_link{"static"};
#else
    const std::string plugin_link{"dynamic"};
#endif
    LogPrintf("Qt %s (%s), plugin=%s (%s)\n", qVersion(), qt_link, QGuiApplication::platformName().toStdString(), plugin_link);
    const auto static_plugins = QPluginLoader::staticPlugins();
    if (static_plugins.empty()) {
        LogPrintf("No static plugins.\n");
    } else {
        LogPrintf("Static plugins:\n");
        for (const QStaticPlugin& p : static_plugins) {
            QJsonObject meta_data = p.metaData();
            const std::string plugin_class = meta_data.take(QString("className")).toString().toStdString();
            const int plugin_version = meta_data.take(QString("version")).toInt();
            LogPrintf(" %s, version %d\n", plugin_class, plugin_version);
        }
    }

    LogPrintf("Style: %s / %s\n", QApplication::style()->objectName().toStdString(), QApplication::style()->metaObject()->className());
    LogPrintf("System: %s, %s\n", QSysInfo::prettyProductName().toStdString(), QSysInfo::buildAbi().toStdString());
    for (const QScreen* s : QGuiApplication::screens()) {
        LogPrintf("Screen: %s %dx%d, pixel ratio=%.1f\n", s->name().toStdString(), s->size().width(), s->size().height(), s->devicePixelRatio());
    }
}

void PopupMenu(QMenu* menu, const QPoint& point, QAction* at_action)
{
    // The qminimal plugin does not provide window system integration.
    if (QApplication::platformName() == "minimal") return;
    menu->popup(point, at_action);
}

QDateTime StartOfDay(const QDate& date)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    return date.startOfDay();
#else
    return QDateTime(date);
#endif
}

bool HasPixmap(const QLabel* label)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    return !label->pixmap(Qt::ReturnByValue).isNull();
#else
    return label->pixmap() != nullptr;
#endif
}

QImage GetImage(const QLabel* label)
{
    if (!HasPixmap(label)) {
        return QImage();
    }

#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    return label->pixmap(Qt::ReturnByValue).toImage();
#else
    return label->pixmap()->toImage();
#endif
}

QString MakeHtmlLink(const QString& source, const QString& link)
{
    return QString(source).replace(
        link,
        QLatin1String("<a href=\"") + link + QLatin1String("\">") + link + QLatin1String("</a>"));
}

void PrintSlotException(
    const std::exception* exception,
    const QObject* sender,
    const QObject* receiver)
{
    std::string description = sender->metaObject()->className();
    description += "->";
    description += receiver->metaObject()->className();
    PrintExceptionContinue(exception, description);
}

void ShowModalDialogAsynchronously(QDialog* dialog)
{
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->show();
}

QString RigorDialogControlStylesheet()
{
    // Explicit colors override native Windows styling that left light text on white panes.
    // Surfaces/labels/tabs are dark; editable inputs stay white+dark text for contrast.
    const QString ui_font = UiFontFamilyCss();
    return QStringLiteral(
        "QDialog, QMessageBox {"
        "  background-color: #0F0F10; color: #E6E6E6; font-family: %1;"
        "}"
        /* Fill tab pages / frames that Windows styles paint with QPalette::Base (white). */
        "QDialog QWidget { background-color: #0F0F10; color: #E6E6E6; }"
        "QDialog QLabel, QMessageBox QLabel { color: #E6E6E6; background: transparent; font-family: %1; }"
        "QDialog QGroupBox {"
        "  color: #E6E6E6; background-color: #0F0F10; border: 1px solid #2C2C30;"
        "  margin-top: 8px; padding-top: 8px; font-family: %1;"
        "}"
        "QDialog QGroupBox::title { color: #E6E6E6; subcontrol-origin: margin; left: 8px; }"
        "QDialog QCheckBox, QDialog QRadioButton {"
        "  color: #E6E6E6; spacing: 6px; background: transparent; font-family: %1;"
        "}"
        "QDialog QCheckBox:disabled, QDialog QRadioButton:disabled { color: #888888; }"
        "QDialog QCheckBox::indicator, QDialog QRadioButton::indicator {"
        "  width: 16px; height: 16px; border: 1px solid #777777;"
        "  background: #ffffff; border-radius: 2px;"
        "}"
        "QDialog QRadioButton::indicator { border-radius: 8px; }"
        "QDialog QCheckBox::indicator:checked, QDialog QRadioButton::indicator:checked {"
        "  background: #FF6A2B; border: 1px solid #FF6A2B;"
        "}"
        "QDialog QCheckBox::indicator:disabled, QDialog QRadioButton::indicator:disabled {"
        "  background: #cccccc; border: 1px solid #999999;"
        "}"
        "QDialog QTabWidget::pane {"
        "  background-color: #0F0F10; border: 1px solid #2C2C30; top: -1px;"
        "}"
        "QDialog QTabBar::tab {"
        "  background: #1A1A1D; color: #A0A0A0; border: 1px solid #2C2C30;"
        "  padding: 6px 12px; margin-right: 2px; font-family: %1;"
        "}"
        "QDialog QTabBar::tab:selected { background: #222226; color: #E6E6E6; }"
        "QDialog QTabBar::tab:hover:!selected { background: #222226; color: #E6E6E6; }"
        "QDialog QScrollArea, QDialog QAbstractScrollArea, QDialog QFrame {"
        "  background-color: #0F0F10; color: #E6E6E6; border: none;"
        "}"
        /* Read-only / help surfaces: dark. Editable line/combo/spin: white. */
        "QDialog QTextEdit, QDialog QPlainTextEdit, QDialog QTextBrowser {"
        "  background-color: #1A1A1D;"
        "  color: #E6E6E6;"
        "  border: 1px solid #2C2C30;"
        "  border-radius: 4px;"
        "  padding: 4px 6px;"
        "  selection-background-color: #FF6A2B;"
        "  selection-color: #FFFFFF;"
        "  font-family: %1;"
        "}"
        "QDialog QLineEdit, QDialog QSpinBox, QDialog QDoubleSpinBox, QDialog QComboBox,"
        "QDialog QDateTimeEdit, QDialog QAbstractSpinBox {"
        "  background-color: #ffffff;"
        "  color: #111111;"
        "  border: 1px solid #777777;"
        "  border-radius: 4px;"
        "  padding: 4px 6px;"
        "  selection-background-color: #FF6A2B;"
        "  selection-color: #FFFFFF;"
        "  font-family: %1;"
        "}"
        "QDialog QLineEdit:disabled, QDialog QComboBox:disabled, QDialog QSpinBox:disabled {"
        "  background-color: #e8e8e8;"
        "  color: #555555;"
        "}"
        "QDialog QComboBox::drop-down { border: none; width: 20px; background: transparent; }"
        "QDialog QComboBox QAbstractItemView {"
        "  background-color: #ffffff; color: #111111;"
        "  selection-background-color: #FF6A2B; selection-color: #FFFFFF;"
        "  font-family: %1;"
        "}"
        "QDialog QListView, QDialog QTreeView, QDialog QTableView {"
        "  background-color: #1A1A1D; alternate-background-color: #222226; color: #E6E6E6;"
        "  border: 1px solid #2C2C30; gridline-color: #2C2C30;"
        "  selection-background-color: #FF6A2B; selection-color: #FFFFFF;"
        "}"
        "QDialog QHeaderView::section {"
        "  background: #222226; color: #A0A0A0; border: 1px solid #2C2C30; padding: 4px;"
        "}"
        "QDialog QPushButton, QMessageBox QPushButton {"
        "  background-color: #2A2A2E;"
        "  color: #F2F2F2;"
        "  border: 1px solid #6A6A70;"
        "  border-radius: 4px;"
        "  padding: 6px 16px;"
        "  min-height: 26px;"
        "  min-width: 80px;"
        "  font-family: %1;"
        "}"
        "QDialog QPushButton:hover:!disabled, QMessageBox QPushButton:hover:!disabled {"
        "  background-color: #3A3A42; color: #FFFFFF;"
        "}"
        "QDialog QPushButton:pressed { background-color: #1E1E22; }"
        "QDialog QPushButton:disabled, QMessageBox QPushButton:disabled {"
        "  background-color: #1A1A1D;"
        "  color: #9A9A9A;"
        "  border: 1px solid #444448;"
        "}"
        "QDialog QPushButton:default { border: 2px solid #4A9EFF; font-weight: 600; }"
        "QDialog QDialogButtonBox QPushButton { min-width: 88px; }"
        "QDialog QScrollBar:vertical { background: #1A1A1D; width: 12px; }"
        "QDialog QScrollBar::handle:vertical { background: #444444; min-height: 20px; border-radius: 4px; }"
        "QDialog QScrollBar:horizontal { background: #1A1A1D; height: 12px; }"
        "QDialog QScrollBar::handle:horizontal { background: #444444; min-width: 20px; border-radius: 4px; }"
    ).arg(ui_font);
}

void ApplyRigorDialogChrome(QWidget* dialog)
{
    if (!dialog) return;
    dialog->setAutoFillBackground(true);
    QPalette pal = dialog->palette();
    pal.setColor(QPalette::Window, QColor(QStringLiteral("#0F0F10")));
    pal.setColor(QPalette::WindowText, QColor(QStringLiteral("#E6E6E6")));
    pal.setColor(QPalette::Button, QColor(QStringLiteral("#2A2A2E")));
    pal.setColor(QPalette::ButtonText, QColor(QStringLiteral("#F2F2F2")));
    // Keep Base/Text high-contrast for native spin/combo painting if stylesheet lags.
    pal.setColor(QPalette::Base, QColor(QStringLiteral("#FFFFFF")));
    pal.setColor(QPalette::Text, QColor(QStringLiteral("#111111")));
    dialog->setPalette(pal);
    dialog->setStyleSheet(RigorDialogControlStylesheet());
}

void ApplyRigorWalletChrome(QMainWindow* mainWindow, QWidget* walletFrame, QWidget* rpcConsole)
{
    if (!mainWindow) return;

    QPalette pal = QGuiApplication::palette();
    pal.setColor(QPalette::Window, QColor(QStringLiteral("#0F0F10")));
    pal.setColor(QPalette::WindowText, QColor(QStringLiteral("#E6E6E6")));
    // Base/Text used by native styles for QLineEdit — keep high contrast if stylesheet lags.
    pal.setColor(QPalette::Base, QColor(QStringLiteral("#FFFFFF")));
    pal.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#1A1A1D")));
    pal.setColor(QPalette::Text, QColor(QStringLiteral("#111111")));
    pal.setColor(QPalette::Button, QColor(QStringLiteral("#2A2A2E")));
    pal.setColor(QPalette::ButtonText, QColor(QStringLiteral("#F2F2F2")));
    pal.setColor(QPalette::Highlight, QColor(QStringLiteral("#FF6A2B")));
    pal.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#FFFFFF")));
    pal.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#1A1A1D")));
    pal.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#E6E6E6")));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(QStringLiteral("#9A9A9A")));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#555555")));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(QStringLiteral("#888888")));
    QApplication::setPalette(pal);

    // App-wide dialog chrome so Create Wallet / Intro / About are readable on Windows.
    qApp->setStyleSheet(RigorDialogControlStylesheet());

    mainWindow->setObjectName(QStringLiteral("RigorMainWindow"));
    const QString ui_font = UiFontFamilyCss();
    mainWindow->setStyleSheet(QStringLiteral(
        "QMainWindow#RigorMainWindow {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #0F0F10, stop:1 #141416);"
        "  color: #E6E6E6;"
        "  font-family: %1;"
        "}"
        "QMenuBar { background-color: #1A1A1D; color: #E6E6E6; border-bottom: 1px solid #2C2C30; font-family: %1; }"
        "QMenuBar::item:selected { background: #222226; }"
        "QMenu { background-color: #1A1A1D; color: #E6E6E6; border: 1px solid #2C2C30; font-family: %1; }"
        "QMenu::item:selected { background: #222226; }"
        "QToolBar { background-color: #1A1A1D; border: none; spacing: 4px; padding: 2px; }"
        "QToolButton { color: #E6E6E6; font-family: %1; }"
        "QStatusBar { background-color: #1A1A1D; color: #A0A0A0; border-top: 1px solid #2C2C30; font-family: %1; }"
        "QProgressBar { border: 1px solid #2C2C30; border-radius: 4px; text-align: center; background: #222226; color: #E6E6E6; }"
        "QProgressBar::chunk { background-color: #FF6A2B; border-radius: 3px; }"
        "QPushButton {"
        "  background-color: #2A2A2E; color: #F2F2F2; border: 1px solid #6A6A70;"
        "  border-radius: 4px; padding: 6px 14px; min-height: 24px; font-family: %1;"
        "}"
        "QPushButton:hover:!disabled { background-color: #3A3A42; color: #FFFFFF; }"
        "QPushButton:disabled { background-color: #1A1A1D; color: #9A9A9A; border: 1px solid #444448; }"
        "QPushButton:default { border: 2px solid #4A9EFF; }"
        "QDialog, QDialog QLabel, QDialog QTabBar::tab, QDialog QCheckBox, QDialog QGroupBox,"
        "QDialog QComboBox, QDialog QLineEdit { font-family: %1; }"
    ).arg(ui_font));

    const QString wallet_ss = QStringLiteral(
        "QWidget#RigorWalletFrame { background: transparent; color: #E6E6E6; font-family: %1; }"
        "QStackedWidget { background: transparent; }"
        "QLabel { color: #E6E6E6; font-family: %1; }"
        "QCheckBox { color: #E6E6E6; font-family: %1; }"
        "QGroupBox { color: #E6E6E6; border: 1px solid #2C2C30; margin-top: 8px; font-family: %1; }"
        "QAbstractItemView { background: #1A1A1D; alternate-background-color: #222226; color: #E6E6E6; "
        "  gridline-color: #2C2C30; selection-background-color: #FF6A2B; selection-color: #FFFFFF; font-family: %1; }"
        "QHeaderView::section { background: #222226; color: #A0A0A0; border: 1px solid #2C2C30; }"
        /* High-contrast inputs: white field + dark text (fixes Windows native light-on-white). */
        "QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {"
        "  background-color: #ffffff; color: #111111;"
        "  border: 1px solid #777777; border-radius: 4px; padding: 4px;"
        "  selection-background-color: #FF6A2B; selection-color: #FFFFFF; font-family: %1;"
        "}"
        "QLineEdit:disabled, QComboBox:disabled { background-color: #e8e8e8; color: #555555; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #ffffff; color: #111111; "
        "  selection-background-color: #FF6A2B; selection-color: #FFFFFF; font-family: %1; }"
        "QTabWidget::pane { border: 1px solid #2C2C30; background: #0F0F10; }"
        "QTabBar::tab { background: #1A1A1D; color: #A0A0A0; border: 1px solid #2C2C30; padding: 6px 12px; font-family: %1; }"
        "QTabBar::tab:selected { background: #222226; color: #E6E6E6; }"
        "QPushButton {"
        "  background-color: #2A2A2E; color: #F2F2F2; border: 1px solid #6A6A70;"
        "  border-radius: 4px; padding: 6px 14px; min-height: 24px; font-family: %1;"
        "}"
        "QPushButton:hover:!disabled { background-color: #3A3A42; color: #FFFFFF; }"
        "QPushButton:disabled { background-color: #1A1A1D; color: #9A9A9A; border: 1px solid #444448; }"
        "QScrollBar:vertical { background: #1A1A1D; width: 12px; }"
        "QScrollBar::handle:vertical { background: #444444; min-height: 20px; border-radius: 4px; }"
        "QScrollBar:horizontal { background: #1A1A1D; height: 12px; }"
        "QScrollBar::handle:horizontal { background: #444444; min-width: 20px; border-radius: 4px; }"
    ).arg(ui_font);
    if (walletFrame) {
        walletFrame->setStyleSheet(wallet_ss);
    }

    // Node window / RPC console: same dark surface as Send/Receive pages.
    // Command input stays high-contrast white field + dark text (like amount fields).
    const QString rpc_ss = QStringLiteral(
        "QWidget#RigorRpcConsole {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #0F0F10, stop:1 #141416);"
        "  color: #E6E6E6;"
        "}"
        "QWidget#RigorRpcConsole QWidget { background: transparent; color: #E6E6E6; }"
        "QWidget#RigorRpcConsole QLabel { color: #E6E6E6; background: transparent; }"
        "QWidget#RigorRpcConsole QGroupBox { color: #E6E6E6; border: 1px solid #2C2C30; margin-top: 8px; }"
        "QWidget#RigorRpcConsole QTableView, QWidget#RigorRpcConsole QTreeWidget, QWidget#RigorRpcConsole QListWidget {"
        "  background: #1A1A1D; alternate-background-color: #222226; color: #E6E6E6; "
        "  gridline-color: #2C2C30; selection-background-color: #FF6A2B; selection-color: #FFFFFF;"
        "  border: 1px solid #2C2C30;"
        "}"
        "QWidget#RigorRpcConsole QHeaderView::section { background: #222226; color: #A0A0A0; border: 1px solid #2C2C30; }"
        "QWidget#RigorRpcConsole QLineEdit {"
        "  background-color: #ffffff; color: #111111; border: 1px solid #777777; border-radius: 4px; padding: 4px;"
        "  selection-background-color: #FF6A2B; selection-color: #FFFFFF;"
        "}"
        "QWidget#RigorRpcConsole QTextEdit, QWidget#RigorRpcConsole QPlainTextEdit {"
        "  background-color: #1A1A1D; color: #E6E6E6; border: 1px solid #2C2C30; border-radius: 4px;"
        "  selection-background-color: #FF6A2B; selection-color: #FFFFFF;"
        "}"
        "QWidget#RigorRpcConsole QTabWidget::pane { border: 1px solid #2C2C30; background: #0F0F10; }"
        "QWidget#RigorRpcConsole QTabBar::tab { background: #1A1A1D; color: #A0A0A0; border: 1px solid #2C2C30; padding: 6px 10px; }"
        "QWidget#RigorRpcConsole QTabBar::tab:selected { background: #222226; color: #E6E6E6; }"
        "QWidget#RigorRpcConsole QPushButton {"
        "  background-color: #2A2A2E; color: #F2F2F2; border: 1px solid #6A6A70; border-radius: 4px; padding: 6px 14px;"
        "}"
        "QWidget#RigorRpcConsole QPushButton:hover:!disabled { background-color: #3A3A42; color: #FFFFFF; }"
        "QWidget#RigorRpcConsole QPushButton:disabled { color: #9A9A9A; border: 1px solid #444448; }"
        "QWidget#RigorRpcConsole QScrollBar:vertical { background: #1A1A1D; width: 12px; }"
        "QWidget#RigorRpcConsole QScrollBar::handle:vertical { background: #444444; min-height: 20px; border-radius: 4px; }"
    );
    if (rpcConsole) {
        rpcConsole->setObjectName(QStringLiteral("RigorRpcConsole"));
        rpcConsole->setStyleSheet(rpc_ss);
    }
}

} // namespace GUIUtil
