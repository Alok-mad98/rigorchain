/****************************************************************************
** Meta object code from reading C++ file 'explorer_status_service.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "qt/rigorwallet/explorer/explorer_status_service.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'explorer_status_service.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_RigorWallet__ExplorerStatusService_t {
    QByteArrayData data[9];
    char stringdata0[135];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_RigorWallet__ExplorerStatusService_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_RigorWallet__ExplorerStatusService_t qt_meta_stringdata_RigorWallet__ExplorerStatusService = {
    {
QT_MOC_LITERAL(0, 0, 34), // "RigorWallet::ExplorerStatusSe..."
QT_MOC_LITERAL(1, 35, 13), // "statusUpdated"
QT_MOC_LITERAL(2, 49, 0), // ""
QT_MOC_LITERAL(3, 50, 31), // "RigorWallet::ExplorerLiveStatus"
QT_MOC_LITERAL(4, 82, 6), // "status"
QT_MOC_LITERAL(5, 89, 7), // "refresh"
QT_MOC_LITERAL(6, 97, 9), // "doRefresh"
QT_MOC_LITERAL(7, 107, 16), // "applyFetchResult"
QT_MOC_LITERAL(8, 124, 10) // "elapsed_ms"

    },
    "RigorWallet::ExplorerStatusService\0"
    "statusUpdated\0\0RigorWallet::ExplorerLiveStatus\0"
    "status\0refresh\0doRefresh\0applyFetchResult\0"
    "elapsed_ms"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_RigorWallet__ExplorerStatusService[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   34,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       5,    0,   37,    2, 0x0a /* Public */,
       6,    0,   38,    2, 0x08 /* Private */,
       7,    2,   39,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 3, QMetaType::LongLong,    4,    8,

       0        // eod
};

void RigorWallet::ExplorerStatusService::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ExplorerStatusService *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->statusUpdated((*reinterpret_cast< const RigorWallet::ExplorerLiveStatus(*)>(_a[1]))); break;
        case 1: _t->refresh(); break;
        case 2: _t->doRefresh(); break;
        case 3: _t->applyFetchResult((*reinterpret_cast< const RigorWallet::ExplorerLiveStatus(*)>(_a[1])),(*reinterpret_cast< qint64(*)>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ExplorerStatusService::*)(const RigorWallet::ExplorerLiveStatus & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ExplorerStatusService::statusUpdated)) {
                *result = 0;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject RigorWallet::ExplorerStatusService::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_RigorWallet__ExplorerStatusService.data,
    qt_meta_data_RigorWallet__ExplorerStatusService,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *RigorWallet::ExplorerStatusService::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *RigorWallet::ExplorerStatusService::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_RigorWallet__ExplorerStatusService.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int RigorWallet::ExplorerStatusService::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void RigorWallet::ExplorerStatusService::statusUpdated(const RigorWallet::ExplorerLiveStatus & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
